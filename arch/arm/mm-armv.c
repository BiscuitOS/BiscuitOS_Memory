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
#include "biscuitos/nodemask.h"
#include "biscuitos/highmem.h"
#include "asm-generated/map.h"
#include "asm-generated/system.h"
#include "asm-generated/map.h"
#include "asm-generated/domain.h"
#include "asm-generated/setup.h"
#include "asm-generated/vmalloc.h"
#include "asm-generated/tlbflush.h"
#include "asm-generated/cacheflush.h"
#include "asm-generated/pgalloc.h"
#include "asm-generated/cputype.h"
#include "asm-generated/fixmap.h"
#include "asm-generated/highmem.h"

#define CPOLICY_UNCACHE_BS		0
#define CPOLICY_BUFFERED_BS		1
#define CPOLICY_WRITETHROUGH_BS		2
#define CPOLICY_WRITEBACK_BS		3
#define CPOLICY_WRITEALLOC_BS		4

extern unsigned long _stext_bs, _end_bs;
static unsigned int cachepolicy_bs __initdata = CPOLICY_WRITEBACK_BS;
static unsigned int ecc_mask_bs __initdata = 0;
pgprot_t_bs pgprot_kernel_bs;
EXPORT_SYMBOL_GPL(pgprot_kernel_bs);
pgprot_t_bs pgprot_user_bs;
EXPORT_SYMBOL_GPL(pgprot_user_bs);
pmdval_t_bs user_pmd_table_bs = _PAGE_USER_TABLE_BS;

pmd_t_bs *top_pmd_bs;

struct cachepolicy {
	const char	policy[16];
	unsigned int	cr_mask;
	pmdval_t_bs	pmd;
	pteval_t_bs	pte;
	pteval_t_bs	pte_s2;
};

struct mem_types {
	unsigned int	prot_pte;
	unsigned int	prot_l1;
	unsigned int	prot_sect;
	unsigned int	domain;
};

#define PROT_PTE_DEVICE_BS		L_PTE_PRESENT_BS|L_PTE_YOUNG_BS|\
					L_PTE_DIRTY_BS|L_PTE_XN_BS
#define PROT_SECT_DEVICE_BS		PMD_TYPE_SECT_BS|PMD_SECT_AP_WRITE_BS

static struct mem_types mem_types_bs[] __initdata = {
	[MT_DEVICE_BS] = {        /* Strongly ordered / ARMv6 shared device */
		.prot_pte	= PROT_PTE_DEVICE_BS | L_PTE_MT_DEV_SHARED_BS |
				  L_PTE_SHARED_BS,
		.prot_l1	= PMD_TYPE_TABLE_BS,
		.prot_sect	= PROT_SECT_DEVICE_BS | PMD_SECT_S_BS,
		.domain		= DOMAIN_IO_BS,
	},
	[MT_DEVICE_NONSHARED_BS] = { /* ARMv6 non-shared device */
		.prot_pte	= PROT_PTE_DEVICE_BS | 
				  L_PTE_MT_DEV_NONSHARED_BS,
		.prot_l1	= PMD_TYPE_TABLE_BS,
		.prot_sect	= PROT_SECT_DEVICE_BS,
		.domain		= DOMAIN_IO_BS,
	},
	[MT_DEVICE_CACHED_BS] = {    /* ioremap_cached */
		.prot_pte	= PROT_PTE_DEVICE_BS | L_PTE_MT_DEV_CACHED_BS,
		.prot_l1	= PMD_TYPE_TABLE_BS,
		.prot_sect	= PROT_SECT_DEVICE_BS | PMD_SECT_WB_BS,
		.domain		= DOMAIN_IO_BS,
	},
	[MT_DEVICE_WC_BS] = {      /* ioremap_wc */
		.prot_pte	= PROT_PTE_DEVICE_BS | L_PTE_MT_DEV_WC_BS,
		.prot_l1	= PMD_TYPE_TABLE_BS,
		.prot_sect	= PROT_SECT_DEVICE_BS,
		.domain		= DOMAIN_IO_BS,
	},
	[MT_UNCACHED_BS] = {
		.prot_pte	= PROT_PTE_DEVICE_BS,
		.prot_l1	= PMD_TYPE_TABLE_BS,
		.prot_sect	= PMD_TYPE_SECT_BS | PMD_SECT_XN_BS,
		.domain		= DOMAIN_IO_BS,
	},
	[MT_CACHECLEAN_BS] = {
		.prot_sect	= PMD_TYPE_SECT_BS | PMD_SECT_XN_BS,
		.domain		= DOMAIN_KERNEL_BS,
	},
#ifndef CONFIG_ARM_LPAE
	[MT_MINICLEAN_BS] = {
		.prot_sect 	= PMD_TYPE_SECT_BS | PMD_SECT_XN_BS | 
						  PMD_SECT_MINICACHE_BS,
		.domain		= DOMAIN_KERNEL_BS,
	},
#endif
	[MT_LOW_VECTORS_BS] = {
		.prot_pte	= L_PTE_PRESENT_BS | L_PTE_YOUNG_BS | 
				  L_PTE_DIRTY_BS | L_PTE_RDONLY_BS,
		.prot_l1	= PMD_TYPE_TABLE_BS,
		.domain		= DOMAIN_VECTORS_BS,
	},
	[MT_HIGH_VECTORS_BS] = {
		.prot_pte	= L_PTE_PRESENT_BS | L_PTE_YOUNG_BS | 
				  L_PTE_DIRTY_BS |
				  L_PTE_USER_BS | L_PTE_RDONLY_BS,
		.prot_l1	= PMD_TYPE_TABLE_BS,
		.domain		= DOMAIN_VECTORS_BS,
	},
	[MT_MEMORY_RWX_BS] = {
		.prot_pte	= L_PTE_PRESENT_BS | L_PTE_YOUNG_BS | 
				  L_PTE_DIRTY_BS,
		.prot_l1	= PMD_TYPE_TABLE_BS,
		.prot_sect	= PMD_TYPE_SECT_BS | PMD_SECT_AP_WRITE_BS,
		.domain		= DOMAIN_KERNEL_BS,
	},
	[MT_MEMORY_RW_BS] = {
		.prot_pte	= L_PTE_PRESENT_BS | L_PTE_YOUNG_BS | 
				  L_PTE_DIRTY_BS | L_PTE_XN_BS,
		.prot_l1	= PMD_TYPE_TABLE_BS,
		.prot_sect	= PMD_TYPE_SECT_BS | PMD_SECT_AP_WRITE_BS,
		.domain		= DOMAIN_KERNEL_BS,
	},
	[MT_ROM_BS] = {
		.prot_sect	= PMD_TYPE_SECT_BS,
		.domain		= DOMAIN_KERNEL_BS,
	},
	[MT_MEMORY_RWX_NONCACHED_BS] = {
		.prot_pte	= L_PTE_PRESENT_BS | L_PTE_YOUNG_BS | 
				  L_PTE_DIRTY_BS | L_PTE_MT_BUFFERABLE_BS,
		.prot_l1	= PMD_TYPE_TABLE_BS,
		.prot_sect	= PMD_TYPE_SECT_BS | PMD_SECT_AP_WRITE_BS,
		.domain		= DOMAIN_KERNEL_BS,
	},
	[MT_MEMORY_RW_DTCM_BS] = {
		.prot_pte	= L_PTE_PRESENT_BS | L_PTE_YOUNG_BS | 
				  L_PTE_DIRTY_BS | L_PTE_XN_BS,
		.prot_l1	= PMD_TYPE_TABLE_BS,
		.prot_sect	= PMD_TYPE_SECT_BS | PMD_SECT_XN_BS,
		.domain		= DOMAIN_KERNEL_BS,
	},
	[MT_MEMORY_RWX_ITCM_BS] = {
		.prot_pte	= L_PTE_PRESENT_BS | L_PTE_YOUNG_BS | 
				  L_PTE_DIRTY_BS,
		.prot_l1	= PMD_TYPE_TABLE_BS,
		.domain		= DOMAIN_KERNEL_BS,
	},
	[MT_MEMORY_RW_SO_BS] = {
		.prot_pte	= L_PTE_PRESENT_BS | L_PTE_YOUNG_BS | 
				  L_PTE_DIRTY_BS | 
				  L_PTE_MT_UNCACHED_BS | L_PTE_XN_BS,
		.prot_l1	= PMD_TYPE_TABLE_BS,
		.prot_sect	= PMD_TYPE_SECT_BS | PMD_SECT_AP_WRITE_BS | 
				  PMD_SECT_S_BS | PMD_SECT_UNCACHED_BS |
				  PMD_SECT_XN_BS,
		.domain		= DOMAIN_KERNEL_BS,
	},
	[MT_MEMORY_DMA_READY_BS] = {
		.prot_pte	= L_PTE_PRESENT_BS | L_PTE_YOUNG_BS |
				  L_PTE_DIRTY_BS | L_PTE_XN_BS,
		.prot_l1   	= PMD_TYPE_TABLE_BS,
		.domain    	= DOMAIN_KERNEL_BS,
	},
};

#ifdef CONFIG_ARM_LPAE
#define s2_policy(policy)	policy
#else
#define s2_policy(policy)	0
#endif

static struct cachepolicy cache_policies_bs[] __initdata = {
	{
		.policy		= "uncached",
		.cr_mask	= CR_W_BS | CR_C_BS,
		.pmd		= PMD_SECT_UNCACHED_BS,
		.pte		= L_PTE_MT_UNCACHED_BS,
		.pte_s2		= s2_policy(L_PTE_S2_MT_UNCACHED_BS),
	}, {
		.policy		= "buffered",
		.cr_mask	= CR_C_BS,
		.pmd		= PMD_SECT_BUFFERED_BS,
		.pte		= L_PTE_MT_BUFFERABLE_BS,
		.pte_s2		= s2_policy(L_PTE_S2_MT_UNCACHED_BS),
	}, {
		.policy		= "writethrough",
		.cr_mask	= 0,
		.pmd		= PMD_SECT_WT_BS,
		.pte		= L_PTE_MT_WRITETHROUGH_BS,
		.pte_s2		= s2_policy(L_PTE_S2_MT_WRITETHROUGH_BS),
	}, {
		.policy		= "writeback",
		.cr_mask	= 0,
		.pmd		= PMD_SECT_WB_BS,
		.pte		= L_PTE_MT_WRITEBACK_BS,
		.pte_s2		= s2_policy(L_PTE_S2_MT_WRITEBACK_BS),
	}, {
		.policy		= "writealloc",
		.cr_mask	= 0,
		.pmd		= PMD_SECT_WBWA_BS,
		.pte		= L_PTE_MT_WRITEALLOC_BS,
		.pte_s2		= s2_policy(L_PTE_S2_MT_WRITEBACK_BS),
	}
};

#ifdef CONFIG_CPU_CP15
static unsigned long initial_pmd_value_bs __initdata = 0;

/*
 * Initialise the cache_policy variable with the initial state specified
 * via the "pmd" value. This is used to ensure that on ARMv6 and later,
 * the C code sets the page tables up with the same policy as the head
 * assembly code, which avoids an illegal state where the TLBs can get
 * confused. See comments in early_cachepolicy() for more information.
 */
void __init init_default_cache_policy_bs(unsigned long pmd)
{
	int i;

	initial_pmd_value_bs = pmd;

	pmd &= PMD_SECT_CACHE_MASK_BS;

	for (i = 0; i < ARRAY_SIZE(cache_policies_bs); i++) {
		if (cache_policies_bs[i].pmd == pmd) {
			cachepolicy_bs = i;
			break;
		}
	}

	if (i == ARRAY_SIZE(cache_policies_bs))
		printk("ERROR: could not find cache policy\n");
}
#endif

/*
 * Adjust the PMD section entries according to the CPU in use.
 */
static void __init build_mem_type_table_bs(void)
{
	struct cachepolicy *cp;
	unsigned int cr = get_cr_bs();
	pteval_t_bs user_pgprot, kern_pgprot, vecs_pgprot;
	int cpu_arch = cpu_architecture_bs();
	int i;

	/*
	 * Mark the device areas according to the CPU/architecture.
	 */
	if (cpu_is_xsc3_bs() || (cpu_arch >= CPU_ARCH_ARMv6_BS && 
							(cr & CR_XP_BS))) {
		if (!cpu_is_xsc3_bs()) {
			/*
			 * Mark device regions on ARMv6+ as execute-never
			 * to prevent speculative instruction fetches.
			 */
			mem_types_bs[MT_DEVICE_BS].prot_sect |= PMD_SECT_XN_BS;
			mem_types_bs[MT_DEVICE_NONSHARED_BS].prot_sect |=
								PMD_SECT_XN_BS;
			mem_types_bs[MT_DEVICE_CACHED_BS].prot_sect |=
								PMD_SECT_XN_BS;
			mem_types_bs[MT_DEVICE_WC_BS].prot_sect |= 
								PMD_SECT_XN_BS;

			/* Also setup NX memory mapping */
			mem_types_bs[MT_MEMORY_RW_BS].prot_sect |= 
								PMD_SECT_XN_BS;
		}
		if (cpu_arch >= CPU_ARCH_ARMv7_BS && (cr & CR_TRE_BS)) {
			/*
			 * For ARMv7 with TEX remapping.
			 * - shared device is SXCB=1100
			 * - nonshared device is SXCB=0100
			 * - write combine device mem is SXCB=0001
			 * (Uncached Normal memory)
			 */
			mem_types_bs[MT_DEVICE_BS].prot_sect |= 
							PMD_SECT_TEX_BS(1);
			mem_types_bs[MT_DEVICE_NONSHARED_BS].prot_sect |=
							PMD_SECT_TEX_BS(1);
			mem_types_bs[MT_DEVICE_WC_BS].prot_sect |=
							PMD_SECT_BUFFERABLE_BS;
		}
	}

	/*
	 * Now deal with the memory-type mappings
	 */
	cp = &cache_policies_bs[cachepolicy_bs];
	vecs_pgprot = kern_pgprot = user_pgprot = cp->pte;

#ifndef CPNFIG_ARM_LPAE
	/*
	 * Checks is it with support for the PXN bit
	 * in the Short-descriptor translation table format descriptors.
	 */
	if (cpu_arch == CPU_ARCH_ARMv7_BS &&
		(read_cpuid_ext_bs(CPUID_EXT_MMFR0_BS) & 0xF) >= 4) {
		user_pmd_table_bs |= PMD_PXNTABLE_BS;
	}
#endif
	/*
	 * ARMv6 and above have extended page tables.
	 */
	if (cpu_arch >= CPU_ARCH_ARMv6_BS && (cr & CR_XP_BS)) {
#ifndef CONFIG_ARM_LPAE
		/*
		 * Mark cache clean areas and XIP ROM read only
		 * from SVC mode and no access from userspace.
		 */
		mem_types_bs[MT_ROM_BS].prot_sect |= PMD_SECT_APX_BS |
							PMD_SECT_AP_WRITE_BS;
		mem_types_bs[MT_MINICLEAN_BS].prot_sect |= PMD_SECT_APX_BS |
							PMD_SECT_AP_WRITE_BS;
		mem_types_bs[MT_CACHECLEAN_BS].prot_sect |= PMD_SECT_APX_BS |
							PMD_SECT_AP_WRITE_BS;
#endif
	}

	/*
	 * Non-cacheable Normal - intended for memory areas that must
	 * not cause dirty cache line writebacks when used
	 */
	if (cpu_arch >= CPU_ARCH_ARMv6_BS) {
		if (cpu_arch >= CPU_ARCH_ARMv7_BS && (cr & CR_TRE_BS)) {
			/* Non-cacheable Normal is XCB = 001 */
			mem_types_bs[MT_MEMORY_RWX_NONCACHED_BS].prot_sect |=
						PMD_SECT_BUFFERED_BS;
		} else {
			/* For both ARMv6 and non-TEX-remapping ARMv7 */
			mem_types_bs[MT_MEMORY_RWX_NONCACHED_BS].prot_sect |=
						PMD_SECT_TEX_BS(1);
		}
	}

	for (i = 0; i < 16; i++) {
		pteval_t_bs v = pgprot_val_bs(protection_map_bs[i]);
		protection_map_bs[i] = __pgprot_bs(v | user_pgprot);
	}

	mem_types_bs[MT_LOW_VECTORS_BS].prot_pte |= vecs_pgprot;
	mem_types_bs[MT_HIGH_VECTORS_BS].prot_pte |= vecs_pgprot;

	pgprot_user_bs = __pgprot_bs(L_PTE_PRESENT_BS | L_PTE_YOUNG_BS | 
					user_pgprot);
	pgprot_kernel_bs = __pgprot_bs(L_PTE_PRESENT_BS | L_PTE_YOUNG_BS |
				    L_PTE_DIRTY_BS | kern_pgprot);
	mem_types_bs[MT_LOW_VECTORS_BS].prot_l1 |= ecc_mask_bs;
	mem_types_bs[MT_HIGH_VECTORS_BS].prot_l1 |= ecc_mask_bs;
	mem_types_bs[MT_MEMORY_RWX_BS].prot_sect |= ecc_mask_bs | cp->pmd;
	mem_types_bs[MT_MEMORY_RWX_BS].prot_pte |= kern_pgprot;
	mem_types_bs[MT_MEMORY_RW_BS].prot_sect |= ecc_mask_bs | cp->pmd;
	mem_types_bs[MT_MEMORY_RW_BS].prot_pte |= kern_pgprot;
	mem_types_bs[MT_MEMORY_DMA_READY_BS].prot_pte |= kern_pgprot;
	mem_types_bs[MT_MEMORY_RWX_NONCACHED_BS].prot_sect |= ecc_mask_bs;
	mem_types_bs[MT_ROM_BS].prot_sect |= cp->pmd;

	switch (cp->pmd) {
	case PMD_SECT_WT_BS:
		mem_types_bs[MT_CACHECLEAN_BS].prot_sect |= PMD_SECT_WT_BS;
		break;
	case PMD_SECT_WB_BS:
	case PMD_SECT_WBWA_BS:
		mem_types_bs[MT_CACHECLEAN_BS].prot_sect |= PMD_SECT_WB_BS;
	}
	printk("Memory policy: %sData cache %s\n",
		ecc_mask_bs ? "ECC enable, " : "", cp->policy);

	for (i = 0; i < ARRAY_SIZE(mem_types_bs); i++) {
		struct mem_types *t = &mem_types_bs[i];

		if (t->prot_l1)
			t->prot_l1 |= PMD_DOMAIN_BS(t->domain);
		if (t->prot_sect)
			t->prot_sect |= PMD_DOMAIN_BS(t->domain);
	}
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
		ptep = alloc_bootmem_low_pages_bs(2 * PTRS_PER_PTE_BS *
						sizeof(pte_t_bs));

		pmdval = __pa_bs(ptep) | prot_l1;
		pmdp[0] = __pmd_bs(pmdval);
		pmdp[1] = __pmd_bs(pmdval + 256 * sizeof(pte_t_bs));
		/* FIXME: Use ARMv7 */
		flush_pmd_entry_bs(pmdp);
	}
	ptep = pte_offset_kernel_bs(pmdp, virt);

	/* FIXME: Use ARMv7 */
	set_pte_bs(ptep, pfn_pte_bs(phys >> PAGE_SHIFT_BS, prot));
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

static void __init *early_alloc_bs(unsigned long sz)
{
	return alloc_bootmem_low_pages_bs(sz);
}

static void __init __map_init_section_bs(pmd_t_bs *pmd, unsigned long addr,
			unsigned long end, phys_addr_t phys,
			const struct mem_types *type, bool ng)
{
	pmd_t_bs *p = pmd;

#ifndef CONFIG_ARM_LPAE
	if (addr & SECTION_SIZE_BS)
		pmd++;
#endif

	do {
		*pmd = __pmd_bs(phys | type->prot_sect | 
					(ng ? PMD_SECT_nG_BS : 0));
		phys += SECTION_SIZE_BS;
	} while (pmd++, addr += SECTION_SIZE_BS, addr != end);

	flush_pmd_entry_bs(p);
}

static void __init alloc_init_pmd_bs(pud_t_bs *pud, unsigned long addr,
			unsigned long end, phys_addr_t phys,
			const struct mem_types *type,
			void *(*alloc)(unsigned long sz), bool ng)
{
	pmd_t_bs *pmd = pmd_offset_bs(pud, addr);
	unsigned long next;

	do {
		/*
		 * While LPAE, we must loop over to map
		 * all the pmds for the given range.
		 */
		next = pmd_addr_end_bs(addr, end);

		/*
		 * Try a section mapping - addr, next and phys must all be
		 * aligned to a section boundary.
		 */
		if (type->prot_sect &&
			((addr | next | phys) & ~SECTION_MASK_BS) == 0) {
			__map_init_section_bs(pmd, addr, next, phys, type, ng);
		} else {
			BS_DUP();
		}

		phys += next - addr;

	} while (pmd++, addr = next, addr != end);
}

static void __init alloc_init_pud_bs(pgd_t_bs *pgd, unsigned long addr,
			unsigned long end, phys_addr_t phys,
			const struct mem_types *type,
			void *(alloc)(unsigned long sz), bool ng)
{
	pud_t_bs *pud = pud_offset_bs(pgd, addr);
	unsigned long next;

	do {
		next = pud_addr_end_bs(addr, end);
		alloc_init_pmd_bs(pud, addr, next, phys, type, alloc, ng);
		phys += next - addr;
	} while (pud++, addr = next, addr != end);
}

static void __init __create_mapping_bs(struct mm_struct_bs *mm,
		struct map_desc *md, void *(*alloc)(unsigned long sz),
		bool ng)
{
	unsigned long addr, length, end;
	phys_addr_t phys;
	const struct mem_types *type;
	pgd_t_bs *pgd;

	type = &mem_types_bs[md->type];

	addr = md->virtual & PAGE_MASK_BS;
	phys = md->physical;
	length = PAGE_ALIGN_BS(md->length + (md->virtual & ~PAGE_MASK_BS));

	if (type->prot_l1 == 0 && ((addr | phys | length) & ~SECTION_MASK_BS)) {
		printk("BUG: map for %#llx at %#lx can not be mapped "
			"using pages, ignoring.\n", 
			(long long)(md->physical), addr);
		return;
	}

	pgd = pgd_offset_bs(mm, addr);
	end = addr + length;
	do {
		unsigned long next = pgd_addr_end_bs(addr, end);

		alloc_init_pud_bs(pgd, addr, next, phys, type, alloc, ng);

		phys += next - addr;
		addr = next;
	} while (pgd++, addr != end);
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
	if (md->virtual != vectors_base_bs() && md->virtual < TASK_SIZE_BS) {
		printk("BUG: not creating mapping for %#llx at %#lx in "
				"user region\n", 
			(long long)((u64)md->physical), md->virtual);
		return;
	}

	__create_mapping_bs(&init_mm_bs, md, early_alloc_bs, false);
}

/*
 * Setup initial mappings. We use the page we allocated for zero page to
 * hold the mappings, which will get overwritten by the vectors in
 * traps_init(). The mappings must be in virtual address order.
 */
void __init memtable_init_bs(struct meminfo *mi)
{
	struct map_desc map;
	int i;

	build_mem_type_table_bs();

	for (i = 0; i < mi->nr_banks; i++) {
		phys_addr_t start, end;
		if (mi->bank[i].size == 0)
			continue;

		start = mi->bank[i].start;
		end   = start + mi->bank[i].size;

		if (start >= end)
			break;

		map.physical = start;
		map.virtual  = __phys_to_virt_bs(start);
		map.length   = mi->bank[i].size;
		map.type     = MT_MEMORY_RWX_BS;

		create_mapping_bs(&map);
	}

	flush_cache_all_bs();
	flush_tlb_all_bs();

	top_pmd_bs = pmd_off_k_bs(0xffff0000);
}

void arch_free_page_bs(struct page_bs *page, int order)
{
#ifdef CONFIG_HIGHMEM_BS
	/* FIXME: Default ARM doesn't support HighMem Zone,
	 * BiscuitOS support it. */
	struct zone_bs *zone = page_zone_bs(page);

	if (strcmp(zone->name, "HighMem") == 0) {
		struct page_bs *tmp;
		int idx;

		for (idx = 0; idx < (1 << order); idx++) {
			tmp = page + idx;
			SetPageHighMem_bs(tmp);
		}
	}
#endif
}

static pte_t_bs *__init early_pte_alloc_bs(pmd_t_bs *pmd, unsigned long addr,
				unsigned long prot)
{
	if (pmd_none_bs(*pmd)) {
		pte_t_bs *pte = 
			alloc_bootmem_bs(PTE_HWTABLE_OFF_BS + 
					 PTE_HWTABLE_SIZE_BS);
		__pmd_populate_bs(pmd, __pa_bs(pte), prot);
	}
	BUG_ON_BS(pmd_bad_bs(*pmd));
	return pte_offset_kernel_bs(pmd, addr);
}

pgprot_t_bs kmap_prot_bs;
pte_t_bs *kmap_pte_bs;

void __init kmap_init_bs(void)
{
	/* PKMAP */
	pkmap_page_table_bs = early_pte_alloc_bs(pmd_off_k_bs(PKMAP_BASE_BS),
				PKMAP_BASE_BS, _PAGE_KERNEL_TABLE_BS);
	kmap_prot_bs = PAGE_KERNEL_BS;

	/* FIXMAP */
	kmap_pte_bs = early_pte_alloc_bs(pmd_off_k_bs(FIXADDR_START_BS),
				FIXADDR_START_BS, _PAGE_KERNEL_TABLE_BS);
	
}
