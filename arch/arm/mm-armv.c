/*
 * arm/mm/mm-armv.c
 *
 * (C) 2020.04.14 BuddyZhang1 <buddy.zhang@aliyun.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/kernel.h>
#include "biscuitos/mm.h"
#include "asm-generated/map.h"
#include "asm-generated/system.h"
#include "asm-generated/pgtable.h"
#include "asm-generated/map.h"
#include "asm-generated/domain.h"

#define CPOLICY_UNCACHE		0
#define CPOLICY_BUFFERED	1
#define CPOLICY_WRITETHROUGH	2
#define CPOLICY_WRITEBACK	3
#define CPOLICY_WRITEALLOC	4

static unsigned int cachepolicy __initdata = CPOLICY_WRITEBACK;
static unsigned int ecc_mask_bs __initdata = 0;
pgprot_t pgprot_kernel_bs;
EXPORT_SYMBOL_GPL(pgprot_kernel_bs);

struct cachepolicy {
	const char	policy[16];
	unsigned int	cr_mask;
	unsigned int	pmd;
	unsigned int	pte;
};

struct mem_types {
	unsigned int	prot_pte;
	unsigned int	prot_l1;
	unsigned int	prot_sect;
	unsigned int	domain;
};

static struct mem_types mem_types_bs[] __initdata = {
	[MT_DEVICE] = {
		.prot_pte  = L_PTE_PRESENT | L_PTE_YOUNG | L_PTE_DIRTY |
			     L_PTE_WRITE,
		.prot_l1   = PMD_TYPE_TABLE,
		.prot_sect = PMD_TYPE_SECT | PMD_SECT_UNCACHED |
			     PMD_SECT_AP_WRITE,
		.domain    = DOMAIN_IO,
	},
	[MT_CACHECLEAN] = {
		.prot_sect = PMD_TYPE_SECT,
		.domain    = DOMAIN_KERNEL,
	},      
	[MT_MINICLEAN] = {
		.prot_sect = PMD_TYPE_SECT | PMD_SECT_MINICACHE,
		.domain    = DOMAIN_KERNEL,
	},      
	[MT_LOW_VECTORS] = {
		.prot_pte  = L_PTE_PRESENT | L_PTE_YOUNG | L_PTE_DIRTY |
			     L_PTE_EXEC,
		.prot_l1   = PMD_TYPE_TABLE,
		.domain    = DOMAIN_USER,
	},
	[MT_HIGH_VECTORS] = {
		.prot_pte  = L_PTE_PRESENT | L_PTE_YOUNG | L_PTE_DIRTY |
			     L_PTE_USER | L_PTE_EXEC,
		.prot_l1   = PMD_TYPE_TABLE,
		.domain    = DOMAIN_USER,
	},
	[MT_MEMORY] = {
		.prot_sect = PMD_TYPE_SECT | PMD_SECT_AP_WRITE,
		.domain    = DOMAIN_KERNEL,
	},
	[MT_ROM] = {
		.prot_sect = PMD_TYPE_SECT,
		.domain    = DOMAIN_KERNEL,
	},
	[MT_IXP2000_DEVICE] = { /* IXP2400 requires XCB=101 for on-chip I/O */
		.prot_pte  = L_PTE_PRESENT | L_PTE_YOUNG | L_PTE_DIRTY |
			     L_PTE_WRITE,
		.prot_l1   = PMD_TYPE_TABLE,
		.prot_sect = PMD_TYPE_SECT | PMD_SECT_UNCACHED |
			     PMD_SECT_AP_WRITE | PMD_SECT_BUFFERABLE |
			     PMD_SECT_TEX(1),
		.domain    = DOMAIN_IO,
	}
};

static struct cachepolicy cache_policies_bs[] __initdata = {
	{
		.policy		= "uncached",
		.cr_mask	= CR_W|CR_C,
		.pmd		= PMD_SECT_UNCACHED,
		.pte		= 0,
	}, {
		.policy		= "buffered",
		.cr_mask	= CR_C,
		.pmd		= PMD_SECT_BUFFERED,
		.pte		= PTE_BUFFERABLE,
	}, {
		.policy		= "writethrough",
		.cr_mask	= 0,
		.pmd		= PMD_SECT_WT,
		.pte		= PTE_CACHEABLE,
	}, {
		.policy		= "writeback",
		.cr_mask	= 0,
		.pmd		= PMD_SECT_WB,
		.pte		= PTE_BUFFERABLE|PTE_CACHEABLE,
	}, {
		.policy		= "writealloc",
		.cr_mask	= 0,
		.pmd		= PMD_SECT_WBWA,
		.pte		= PTE_BUFFERABLE|PTE_CACHEABLE,
	}
};

/*
 * Adjust the PMD section entries according to the CPU in use.
 */
static void __init build_mem_type_table_bs(void)
{
	struct cachepolicy *cp;
	unsigned int cr = get_cr_bs();
	int cpu_arch = cpu_architecture_bs();
	int i;

#if defined(CONFIG_CPU_DCACHE_DISABLE)
	if (cachepolicy > CPOLICY_BUFFERED)
		cachepolicy = CPOLICY_BUFFERED;
#elif defined(CONFIG_CPU_DCACHE_WRITETHROUGH)
	if (cachepolicy > CPOLICY_WRITETHROUGH)
		cachepolicy = CPOLICY_WRITETHROUGH;
#endif
	if (cpu_arch < CPU_ARCH_ARMv5) {
		if (cachepolicy >= CPOLICY_WRITEALLOC)
			cachepolicy = CPOLICY_WRITEBACK;
		ecc_mask_bs = 0;
	}

	if (cpu_arch <= CPU_ARCH_ARMv5) {
		for (i = 0; i < ARRAY_SIZE(mem_types_bs); i++) {
			if (mem_types_bs[i].prot_l1)
				mem_types_bs[i].prot_l1 |= PMD_BIT4;
			if (mem_types_bs[i].prot_sect)
				mem_types_bs[i].prot_sect |= PMD_BIT4;
		}
	}
	
	/*
	 * ARMv6 and above have extended page tables.
	 */
	if (cpu_arch >= CPU_ARCH_ARMv6 && (cr & CR_XP)) {
		/*
		 * bit 4 becomes XN which we must clear for the
		 * kernel memory mapping.
		 */
		mem_types_bs[MT_MEMORY].prot_sect &= ~PMD_BIT4;
		mem_types_bs[MT_ROM].prot_sect &= ~PMD_BIT4;
		/*
		 * Mark cache clean areas and XIP ROM read only
		 * from SVC mode and no access from userspace.
		 */
		mem_types_bs[MT_ROM].prot_sect |= 
					PMD_SECT_APX | PMD_SECT_AP_WRITE;
		mem_types_bs[MT_MINICLEAN].prot_sect |=
					PMD_SECT_APX | PMD_SECT_AP_WRITE;
		mem_types_bs[MT_CACHECLEAN].prot_sect |=
					PMD_SECT_APX | PMD_SECT_AP_WRITE;
	}

	cp = &cache_policies_bs[cachepolicy];

	if (cpu_arch >= CPU_ARCH_ARMv5) {
		mem_types_bs[MT_LOW_VECTORS].prot_pte |= 
						cp->pte & PTE_CACHEABLE;
		mem_types_bs[MT_HIGH_VECTORS].prot_pte |=
						cp->pte & PTE_CACHEABLE;
	} else {
		mem_types_bs[MT_LOW_VECTORS].prot_pte |= cp->pte;
		mem_types_bs[MT_HIGH_VECTORS].prot_pte |= cp->pte;
		mem_types_bs[MT_MINICLEAN].prot_pte &= ~PMD_SECT_TEX(1);
	}

	mem_types_bs[MT_LOW_VECTORS].prot_l1 |= ecc_mask_bs;
	mem_types_bs[MT_HIGH_VECTORS].prot_l1 |= ecc_mask_bs;
	mem_types_bs[MT_MEMORY].prot_sect |= ecc_mask_bs | cp->pmd;
	mem_types_bs[MT_ROM].prot_sect |= cp->pmd;

	for (i = 0; i< 16; i++) {
		unsigned long v = pgprot_val_bs(protection_map_bs[i]);

		v &= (~(PTE_BUFFERABLE | PTE_CACHEABLE)) | cp->pte;
		protection_map_bs[i] = __pgprot_bs(v);
	}

	pgprot_kernel_bs = __pgprot_bs(L_PTE_PRESENT | L_PTE_YOUNG |
				       L_PTE_DIRTY   | L_PTE_WRITE |
				       L_PTE_EXEC    | cp->pte);

	switch (cp->pmd) {
	case PMD_SECT_WT:
		mem_types_bs[MT_CACHECLEAN].prot_sect |= PMD_SECT_WT;
		break;
	case PMD_SECT_WB:
	case PMD_SECT_WBWA:
		mem_types_bs[MT_CACHECLEAN].prot_sect |= PMD_SECT_WB;
		break;
	}
	printk("Memory policy: ECC %sabled, Data cache %s\n",
		ecc_mask_bs ? "en" : "dis", cp->policy);
}

/*
 * Setup initial mappings. We use the page we allocated for zero page to
 * hold the mappings, which will get overwritten by the vectors in
 * traps_init(). The mappings must be in virtual address order.
 */
void __init memtable_init_bs(struct meminfo *mi)
{
	struct map_desc *init_maps, *p, *q;
	unsigned long address = 0;
	int i;

	build_mem_type_table_bs();
}
