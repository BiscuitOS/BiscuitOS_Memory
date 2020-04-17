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

#include "biscuitos/kernel.h"
#include "biscuitos/mm.h"
#include "biscuitos/bootmem.h"
#include "asm-generated/map.h"
#include "asm-generated/system.h"
#include "asm-generated/map.h"
#include "asm-generated/domain.h"
#include "asm-generated/setup.h"
#include "asm-generated/vmalloc.h"
#include "asm-generated/tlbflush.h"
#include "asm-generated/cacheflush.h"

#define CPOLICY_UNCACHE		0
#define CPOLICY_BUFFERED	1
#define CPOLICY_WRITETHROUGH	2
#define CPOLICY_WRITEBACK	3
#define CPOLICY_WRITEALLOC	4

static unsigned int cachepolicy __initdata = CPOLICY_WRITEBACK;
static unsigned int ecc_mask_bs __initdata = 0;
pgprot_t_bs pgprot_kernel_bs;
EXPORT_SYMBOL_GPL(pgprot_kernel_bs);

pmd_t_bs *top_pmd_bs;

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

static inline pmd_t_bs *pmd_off_bs(pgd_t_bs *pgd, unsigned long virt)
{
	return pmd_offset_bs(pgd, virt);
}

static inline pmd_t_bs *pmd_off_k_bs(unsigned long virt)
{
	return pmd_off_bs(pgd_offset_k_bs(virt), virt);
}

/*
 * Add a PAGE mapping between VIRT and PHYS in domain
 * DOMIAN with protection PROT. Note that due to the
 * way we map the PTEs, we must allocated two PTE_SIZE'd
 * blocks - one for the Linux pte table, and one for
 * the hardware pte table.
 */
static inline void
alloc_init_page_bs(unsigned long virt, unsigned long phys, 
				unsigned int prot_l1, pgprot_t_bs prot)
{
	pmd_t_bs *pmdp = pmd_off_k_bs(virt);
	pte_t_bs *ptep;

	if (pmd_none_bs(*pmdp)) {
		unsigned long pmdval;
		ptep = alloc_bootmem_low_pages_bs(2 * PTRS_PER_PTE *
						sizeof(pte_t_bs));

		pmdval = __pa_bs(ptep) | prot_l1;
		pmdp[0] = __pmd_bs(pmdval);
		pmdp[1] = __pmd_bs(pmdval + 256 * sizeof(pte_t_bs));
		/* FIXME: Use ARMv7 */
		flush_pmd_entry_bs(pmdp);
	}
	ptep = pte_offset_kernel_bs(pmdp, virt);

	/* FIXME: Use ARMv7 */
	set_pte_bs(ptep, pfn_pte_bs(phys >> PAGE_SHIFT, prot));
}

/*
 * Clear any PGD mapping. On a two-level page table system,
 * the clearnance is done by the middle-level functions (pmd)
 * rather than the top-level (pgd) functions.
 */
static inline void clear_mapping_bs(unsigned long virt)
{
	pmd_clear_bs(pmd_off_k_bs(virt));
}

/*
 * Create a SECTION PGD between VIRT and PHYS in domain
 * DOMAIN with protection PROT. This operates on half-pgdir
 # entry increments.
 */
static inline void
alloc_init_section_bs(unsigned long virt, unsigned long phys, int prot)
{
	pmd_t_bs *pmdp = pmd_off_k_bs(virt);

	if (virt & (1 << 20))
		pmdp++;

	*pmdp = __pmd_bs(phys | prot);
	flush_pmd_entry_bs(pmdp);
}

#define vectors_base_bs()	(vectors_high_bs() ? 0xffff0000 : 0)

/*
 * Create the page directory entries and any necessary
 * page tables for the mapping specified by 'md'. We
 * are able to cope here with varying sizes and address
 * offsets, and we take full advantage of sections and
 * supersections.
 */
static void __init create_mapping_bs(struct map_desc *md)
{
	unsigned long virt, length;
	int prot_sect, prot_l1, domain;
	pgprot_t_bs prot_pte;
	long off;

	if (md->virtual != vectors_base_bs() && md->virtual < TASK_SIZE_BS) {
		printk(KERN_WARNING "BUG: not create mapping for "
			"%#lx at %#lx in user region\n",
			md->physical, md->virtual);
		return;
	}

	if ((md->type == MT_DEVICE || md->type == MT_ROM) &&
	     md->virtual >= PAGE_OFFSET_BS && md->virtual < VMALLOC_END) {
		printk(KERN_WARNING "BUG: mapping for %#lx at %#lx "
			"overlaps vmalloc space\n",
			md->physical, md->virtual);
	}

	domain    = mem_types_bs[md->type].domain;
	prot_pte  = __pgprot_bs(mem_types_bs[md->type].prot_pte);
	prot_l1   = mem_types_bs[md->type].prot_l1 | PMD_DOMAIN(domain);
	prot_sect = mem_types_bs[md->type].prot_sect | PMD_DOMAIN(domain);

	virt      = md->virtual;
	off       = md->physical - virt;
	length    = md->length;	

	if (mem_types_bs[md->type].prot_l1 == 0 &&
		(virt & 0xfffff || (virt + off) & 0xfffff || 
		(virt + length) & 0xfffff)) {
		printk(KERN_WARNING "BUG: map for %#lx at %#lx can not "
			"be mapped using pages, ignoring.\n",
			md->physical, md->virtual);
		return;
	}

	while ((virt & 0xfffff || (virt + off) & 0xfffff) && 
						length >= PAGE_SIZE) {
		alloc_init_page_bs(virt, virt + off, prot_l1, prot_pte);

		virt   += PAGE_SIZE;
		length -= PAGE_SIZE;
	}

	/*
	 * N.B. ARMv6 supersections are only defined to work with domain 0.
	 *      Since domain assignments can in fact be arbitrary, the
	 *      'domain == 0' check below is required to insure that ARMv6
	 *      supersections are only allocated for domain 0 regardless
	 *      of the actual domain assignment in use.
	 */
	if (cpu_architecture_bs() >= CPU_ARCH_ARMv6 && domain == 0) {
		/* Align to supersection boundary */
		while ((virt & ~SUPERSECTION_MASK || (virt + off) &
			~SUPERSECTION_MASK) && length >= (PGDIR_SIZE / 2)) {
			alloc_init_section_bs(virt, virt + off, prot_sect);

			virt   += (PGDIR_SIZE / 2);
			length -= (PGDIR_SIZE / 2);
		}

		while (length >= SUPERSECTION_SIZE) {
			BS_DUP();

			virt   += SUPERSECTION_SIZE;
			length += SUPERSECTION_SIZE;
		}
	}

	return;
	/*
	 * A section mapping cover half a "pgdir" entry.
	 */
	while (length >= (PGDIR_SIZE / 2)) {
		alloc_init_section_bs(virt, virt + off, prot_sect);

		virt   += (PGDIR_SIZE / 2);
		length -= (PGDIR_SIZE / 2);
	}

	while (length >= PAGE_SIZE) {
		alloc_init_page_bs(virt, virt + off, prot_l1, prot_pte);

		virt   += PAGE_SIZE;
		length -= PAGE_SIZE;
	}
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

	init_maps = p = alloc_bootmem_low_pages_bs(PAGE_SIZE);

	for (i = 0; i < mi->nr_banks; i++) {
		if (mi->bank[i].size == 0)
			continue;

		p->physical	= mi->bank[i].start;
		p->virtual	= __phys_to_virt_bs(p->physical);
		p->length	= mi->bank[i].size;
		p->type		= MT_MEMORY;
		p++;
	}

	/*
	 * Go through the initial mappings, but clear out any
	 * pgdir entries that are not in the escription.
	 */
	q = init_maps;
	do {
		if (address < q->virtual || q == p) {
			/* FIXME: BiscuitOS doesn't clear PGD out of RAM */
			//clear_mapping_bs(address);
			address += PGDIR_SIZE;
		} else {
			create_mapping_bs(q);

			address = q->virtual + q->length;
			address = (address + PGDIR_SIZE - 1) & PGDIR_MASK;
			q++;
		}
	} while (address != 0);

	/*
	 * Create a mapping for the machine vectors at the high-vectors
	 * location (0xffff0000). If we aren't using high-vectors, also
	 * create a mapping at the low-vectors virtual address.
	 */
	init_maps->physical   = virt_to_phys_bs(init_maps);
	init_maps->virtual    = 0xffff0000;
	init_maps->length     = PAGE_SIZE;
	init_maps->type       = MT_HIGH_VECTORS;
	create_mapping_bs(init_maps);

	if (!vectors_high_bs()) {
		init_maps->virtual = 0;
		init_maps->type = MT_LOW_VECTORS;
		create_mapping_bs(init_maps);
	}

	flush_cache_all_bs();
	flush_tlb_all_bs();
	
	top_pmd_bs = pmd_off_k_bs(0xffff0000);
}
