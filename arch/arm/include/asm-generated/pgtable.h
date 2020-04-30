#ifndef _BISCUITOS_ARM_PGTABLE_H
#define _BISCUITOS_ARM_PGTABLE_H
/*
 *  linux/include/asm-arm/pgtable.h
 *
 *  Copyright (C) 1995-2002 Russell King
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#ifndef __ASSEMBLY__
#include "asm-generated/memory.h"
#include "biscuitos/linkage.h"
#include "biscuitos/gfp.h"
#include "asm-generated/cacheflush.h"
#endif

#ifdef __ASSEMBLY__
#define _AC(X,Y)	X
#define _AT(T,X)	X
#else
#define __AC(X,Y)	(X##Y)
#define _AC(X,Y)	__AC(X,Y)
#define _AT(T,X)	((T)(X))
#endif

#ifndef __ASSEMBLY__

#undef STRICT_MM_TYPECHECKS

#ifdef STRICT_MM_TYPECHECKS

/*
 * These are used to make use of C type-checking
 */
typedef struct { unsigned long pte; } pte_t_bs;
typedef struct { unsigned long pmd; } pmd_t_bs;
typedef struct { unsigned long pgd[2]; } pgd_t_bs;
typedef struct { unsigned long pgprot; } pgprot_t_bs;

#define pte_val_bs(x)		((x).pte)
#define pmd_val_bs(x)		((x).pmd)
#define pgd_val_bs(x)		((x).pgd[0])
#define pgprot_val_bs(x)	((x).pgprot)

#define __pte_bs(x)		((pte_t_bs) { (x) } )
#define __pmd_bs(x)		((pmd_t_bs) { (x) } )
#define __pgprot_bs(x)		((pgprot_t_bs) { (x) } )

#else
/*
 * .. while these make it easier on the compiler
 */
typedef unsigned long pte_t_bs;
typedef unsigned long pmd_t_bs;
typedef unsigned long pgd_t_bs[2];
typedef unsigned long pgprot_t_bs;

#define pte_val_bs(x)		(x)
#define pmd_val_bs(x)		(x)
#define pgd_val_bs(x)		((x)[0])
#define pgprot_val_bs(x)	(x)

#define __pte_bs(x)		(x)
#define __pmd_bs(x)		(x)
#define __pgprot_bs(x)		(x)

#endif

typedef struct { pgd_t_bs pgd; } pud_t_bs;

typedef u32 pteval_t_bs;
typedef u32 pmdval_t_bs;

#endif

/*
 * Just any arbitrary offset to the start of the vmalloc VM area: the
 * current 8MB value just means that there will be a 8MB "hole" after the
 * physical memory until the kernel virtual memory starts.  That means that
 * any out-of-bounds memory accesses will hopefully be caught.
 * The vmalloc() routines leaves a hole of 4kB between each vmalloced
 * area for the same reason. ;)
 *
 * Note that platforms may override VMALLOC_START, but they must provide
 * VMALLOC_END.  VMALLOC_END defines the (exclusive) limit of this space,
 * which may not overlap IO space. 
 */
#ifndef VMALLOC_START_BS
#define VMALLOC_OFFSET_BS	(10*PAGE_SIZE_BS)
#define VMALLOC_START_BS	(((unsigned long)high_memory_bs +	\
				VMALLOC_OFFSET_BS) & ~(VMALLOC_OFFSET_BS-1))
#endif

/*
 * Hardware-wise, we have a two level page table structure, where the first
 * level has 4096 entries, and the second level has 256 entries.  Each entry
 * is one 32-bit word.  Most of the bits in the second level entry are used
 * by hardware, and there aren't any "accessed" and "dirty" bits.
 *
 * Linux on the other hand has a three level page table structure, which can
 * be wrapped to fit a two level page table structure easily - using the PGD
 * and PTE only.  However, Linux also expects one "PTE" table per page, and
 * at least a "dirty" bit.
 *
 * Therefore, we tweak the implementation slightly - we tell Linux that we
 * have 2048 entries in the first level, each of which is 8 bytes (iow, two
 * hardware pointers to the second level.)  The second level contains two
 * hardware PTE tables arranged contiguously, followed by Linux versions
 * which contain the state information Linux needs.  We, therefore, end up
 * with 512 entries in the "PTE" level.
 *
 * This leads to the page tables having the following layout:
 *
 *    pgd             pte
 * |        |
 * +--------+ +0
 * |        |-----> +------------+ +0
 * +- - - - + +4    |  h/w pt 0  |
 * |        |-----> +------------+ +1024
 * +--------+ +8    |  h/w pt 1  |
 * |        |       +------------+ +2048
 * +- - - - +       | Linux pt 0 |
 * |        |       +------------+ +3072
 * +--------+       | Linux pt 1 |
 * |        |       +------------+ +4096
 *
 * See L_PTE_xxx below for definitions of bits in the "Linux pt", and
 * PTE_xxx for definitions of bits appearing in the "h/w pt".
 *
 * PMD_xxx definitions refer to bits in the first level page table.
 *
 * The "dirty" bit is emulated by only granting hardware write permission
 * iff the page is marked "writable" and "dirty" in the Linux PTE.  This
 * means that a write to a clean page will cause a permission fault, and
 * the Linux MM layer will mark the page dirty via handle_pte_fault().
 * For the hardware to notice the permission change, the TLB entry must
 * be flushed, and ptep_establish() does that for us.
 *
 * The "accessed" or "young" bit is emulated by a similar method; we only
 * allow accesses to the page if the "young" bit is set.  Accesses to the
 * page will cause a fault, and handle_pte_fault() will set the young bit
 * for us as long as the page is marked present in the corresponding Linux
 * PTE entry.  Again, ptep_establish() will ensure that the TLB is up to
 * date.
 *
 * However, when the "young" bit is cleared, we deny access to the page
 * by clearing the hardware PTE.  Currently Linux does not flush the TLB
 * for us in this case, which means the TLB will retain the transation
 * until either the TLB entry is evicted under pressure, or a context
 * switch which changes the user space mapping occurs.
 */
#define PTRS_PER_PTE_BS		512
#define PTRS_PER_PMD_BS		1
#define PTRS_PER_PGD_BS		2048

#define PTE_HWTABLE_PTRS_BS	(PTRS_PER_PTE_BS)
#define PTE_HWTABLE_OFF_BS	(PTE_HWTABLE_PTRS_BS * sizeof(pte_t))
#define PTE_HWTABLE_SIZE_BS	(PTRS_PER_PTE_BS * sizeof(u32))

/*
 * PMD_SHIFT determines the size of the area a second-level page table can map
 * PGDIR_SHIFT determines what a third-level page table entry can map
 */
#define PMD_SHIFT_BS		21
#define PGDIR_SHIFT_BS		21

#define PMD_SIZE_BS		(1UL << PMD_SHIFT_BS)
#define PMD_MASK_BS		(~(PMD_SIZE_BS-1))
#define PGDIR_SIZE_BS		(1UL << PGDIR_SHIFT_BS)
#define PGDIR_MASK_BS		(~(PGDIR_SIZE_BS-1))

#define PUD_SHIFT_BS		PGDIR_SHIFT_BS
#define PTRS_PER_PUD_BS		1
#define PUD_SIZE_BS		(1UL << PUD_SHIFT_BS)
#define PUD_MASK_BS		(~(PUD_SIZE_BS-1))

/*
 * section address mask and size definitions.
 */
#define SECTION_SHIFT_BS	20
#define SECTION_SIZE_BS		(1UL << SECTION_SHIFT_BS)
#define SECTION_MASK_BS		(~(SECTION_SIZE_BS-1))

/*
 * "Linux" PTE definitions.
 *
 * We keep two set of PTEs - the hardware and the linux version.
 * This allow greater flexibility in the way we map the Linux bits
 * onto the hardware tables, and allows us to have YOUNG and DIRTY
 * bits.
 *
 * The PTE table pointer refers to the hardware entries; the "Linux"
 * entries are stored 1024 bytes below.
 */
#define L_PTE_VALID_BS		(_AT(pteval_t_bs, 1) << 0)         /* Valid */
#define L_PTE_PRESENT_BS	(_AT(pteval_t_bs, 1) << 0)
#define L_PTE_YOUNG_BS		(_AT(pteval_t_bs, 1) << 1)
#define L_PTE_DIRTY_BS		(_AT(pteval_t_bs, 1) << 6)
#define L_PTE_RDONLY_BS		(_AT(pteval_t_bs, 1) << 7)
#define L_PTE_USER_BS		(_AT(pteval_t_bs, 1) << 8)
#define L_PTE_XN_BS		(_AT(pteval_t_bs, 1) << 9)
#define L_PTE_SHARED_BS		(_AT(pteval_t_bs, 1) << 10)        /* shared(v6), coherent(xsc3) */
#define L_PTE_NONE_BS		(_AT(pteval_t_bs, 1) << 11)

/*
 * These are the memory types, defined to be compatible with
 * pre-ARMv6 CPUs cacheable and bufferable bits: n/a,n/a,C,B
 * ARMv6+ without TEX remapping, they are a table index.
 * ARMv6+ with TEX remapping, they correspond to n/a,TEX(0),C,B
 *
 * MT type              Pre-ARMv6       ARMv6+ type / cacheable status
 * UNCACHED             Uncached        Strongly ordered
 * BUFFERABLE           Bufferable      Normal memory / non-cacheable
 * WRITETHROUGH         Writethrough    Normal memory / write through
 * WRITEBACK            Writeback       Normal memory / write back, read alloc
 * MINICACHE            Minicache       N/A
 * WRITEALLOC           Writeback       Normal memory / write back, write alloc
 * DEV_SHARED           Uncached        Device memory (shared)
 * DEV_NONSHARED        Uncached        Device memory (non-shared) 
 * DEV_WC               Bufferable      Normal memory / non-cacheable
 * DEV_CACHED           Writeback       Normal memory / write back, read alloc
 * VECTORS              Variable        Normal memory / variable
 *
 * All normal memory mappings have the following properties:
 * - reads can be repeated with no side effects
 * - repeated reads return the last value written
 * - reads can fetch additional locations without side effects
 * - writes can be repeated (in certain cases) with no side effects
 * - writes can be merged before accessing the target
 * - unaligned accesses can be supported
 *
 * All device mappings have the following properties:
 * - no access speculation
 * - no repetition (eg, on return from an exception)
 * - number, order and size of accesses are maintained
 * - unaligned accesses are "unpredictable"
 */
#define L_PTE_MT_UNCACHED_BS	(_AT(pteval_t_bs, 0x00) << 2)      /* 0000 */
#define L_PTE_MT_BUFFERABLE_BS	(_AT(pteval_t_bs, 0x01) << 2)      /* 0001 */
#define L_PTE_MT_WRITETHROUGH_BS (_AT(pteval_t_bs, 0x02) << 2)      /* 0010 */
#define L_PTE_MT_WRITEBACK_BS	(_AT(pteval_t_bs, 0x03) << 2)      /* 0011 */
#define L_PTE_MT_MINICACHE_BS	(_AT(pteval_t_bs, 0x06) << 2)      /* 0110 (sa1100, xscale) */
#define L_PTE_MT_WRITEALLOC_BS	(_AT(pteval_t_bs, 0x07) << 2)      /* 0111 */
#define L_PTE_MT_DEV_SHARED_BS	(_AT(pteval_t_bs, 0x04) << 2)      /* 0100 */
#define L_PTE_MT_DEV_NONSHARED_BS (_AT(pteval_t_bs, 0x0c) << 2)      /* 1100 */
#define L_PTE_MT_DEV_WC_BS	(_AT(pteval_t_bs, 0x09) << 2)      /* 1001 */
#define L_PTE_MT_DEV_CACHED_BS	(_AT(pteval_t_bs, 0x0b) << 2)      /* 1011 */
#define L_PTE_MT_VECTORS_BS	(_AT(pteval_t_bs, 0x0f) << 2)      /* 1111 */
#define L_PTE_MT_MASK_BS	(_AT(pteval_t_bs, 0x0f) << 2)

/*
 * The pgprot_* and protection_map entries will be fixed up in runtime
 * to include the cachable and bufferable bits based on memory policy,
 * as well as any architecture dependent bits like global/ASID and SMP
 * shared mapping bits.
 */
#define _L_PTE_DEFAULT_BS	L_PTE_PRESENT_BS | L_PTE_YOUNG_BS

/*
 * ARMv6 supersection address mask and size definitions.
 */
#define SUPERSECTION_SHIFT_BS	24
#define SUPERSECTION_SIZE_BS	(1UL << SUPERSECTION_SHIFT_BS)
#define SUPERSECTION_MASK_BS	(~(SUPERSECTION_SIZE_BS-1))

/*
 * Hardware page table definitions.
 *
 * + Level 1 descriptor (PMD)
 *   - common
 */
#define PMD_TYPE_MASK_BS		(_AT(pmdval_t_bs, 3) << 0)
#define PMD_TYPE_FAULT_BS		(_AT(pmdval_t_bs, 0) << 0)
#define PMD_TYPE_TABLE_BS		(_AT(pmdval_t_bs, 1) << 0)
#define PMD_TYPE_SECT_BS		(_AT(pmdval_t_bs, 2) << 0)
#define PMD_PXNTABLE_BS			(_AT(pmdval_t_bs, 1) << 2)     /* v7 */
#define PMD_BIT4_BS			(_AT(pmdval_t_bs, 1) << 4)
#define PMD_DOMAIN_BS(x)		(_AT(pmdval_t_bs, (x)) << 5)
#define PMD_DOMAIN_MASK_BS		PMD_DOMAIN_BS(0x0f)
#define PMD_PROTECTION_BS		(_AT(pmdval_t_bs, 1) << 9)     /* v5 */
/*
 *   - section
 */
#define PMD_SECT_PXN_BS		(_AT(pmdval_t_bs, 1) << 0)     /* v7 */
#define PMD_SECT_BUFFERABLE_BS	(_AT(pmdval_t_bs, 1) << 2)
#define PMD_SECT_CACHEABLE_BS	(_AT(pmdval_t_bs, 1) << 3)
#define PMD_SECT_XN_BS		(_AT(pmdval_t_bs, 1) << 4)         /* v6 */
#define PMD_SECT_AP_WRITE_BS	(_AT(pmdval_t_bs, 1) << 10)
#define PMD_SECT_AP_READ_BS	(_AT(pmdval_t_bs, 1) << 11)
#define PMD_SECT_TEX_BS(x)	(_AT(pmdval_t_bs, (x)) << 12)      /* v5 */
#define PMD_SECT_APX_BS		(_AT(pmdval_t_bs, 1) << 15)        /* v6 */
#define PMD_SECT_S_BS		(_AT(pmdval_t_bs, 1) << 16)        /* v6 */
#define PMD_SECT_nG_BS		(_AT(pmdval_t_bs, 1) << 17)        /* v6 */
#define PMD_SECT_SUPER_BS	(_AT(pmdval_t_bs, 1) << 18)        /* v6 */
#define PMD_SECT_AF_BS		(_AT(pmdval_t_bs, 0))

#define PMD_SECT_UNCACHED_BS	(_AT(pmdval_t_bs, 0))
#define PMD_SECT_BUFFERED_BS	(PMD_SECT_BUFFERABLE_BS)
#define PMD_SECT_WT_BS		(PMD_SECT_CACHEABLE_BS)
#define PMD_SECT_WB_BS		(PMD_SECT_CACHEABLE_BS | PMD_SECT_BUFFERABLE_BS)
#define PMD_SECT_MINICACHE_BS	(PMD_SECT_TEX_BS(1) | PMD_SECT_CACHEABLE_BS)
#define PMD_SECT_WBWA_BS	(PMD_SECT_TEX_BS(1) | PMD_SECT_CACHEABLE_BS | \
				PMD_SECT_BUFFERABLE_BS)
#define PMD_SECT_CACHE_MASK_BS	(PMD_SECT_TEX_BS(1) | PMD_SECT_CACHEABLE_BS | \
				PMD_SECT_BUFFERABLE_BS)
#define PMD_SECT_NONSHARED_DEV_BS (PMD_SECT_TEX_BS(2))

/*
 *   - coarse table (not used)
 */

/*
 * + Level 2 descriptor (PTE)
 *   - common
 */
#define PTE_TYPE_MASK_BS		(_AT(pteval_t_bs, 3) << 0)
#define PTE_TYPE_FAULT_BS		(_AT(pteval_t_bs, 0) << 0)
#define PTE_TYPE_LARGE_BS		(_AT(pteval_t_bs, 1) << 0)
#define PTE_TYPE_SMALL_BS		(_AT(pteval_t_bs, 2) << 0)
#define PTE_TYPE_EXT_BS			(_AT(pteval_t_bs, 3) << 0)     /* v5 */
#define PTE_BUFFERABLE_BS		(_AT(pteval_t_bs, 1) << 2)
#define PTE_CACHEABLE_BS		(_AT(pteval_t_bs, 1) << 3)

/*
 *   - extended small page/tiny page
 */
#define PTE_EXT_XN_BS		(_AT(pteval_t_bs, 1) << 0)         /* v6 */
#define PTE_EXT_AP_MASK_BS	(_AT(pteval_t_bs, 3) << 4)
#define PTE_EXT_AP0_BS		(_AT(pteval_t_bs, 1) << 4)
#define PTE_EXT_AP1_BS		(_AT(pteval_t_bs, 2) << 4)
#define PTE_EXT_AP_UNO_SRO_BS	(_AT(pteval_t_bs, 0) << 4)
#define PTE_EXT_AP_UNO_SRW_BS	(PTE_EXT_AP0_BS)
#define PTE_EXT_AP_URO_SRW_BS	(PTE_EXT_AP1_BS)
#define PTE_EXT_AP_URW_SRW_BS	(PTE_EXT_AP1_BS|PTE_EXT_AP0_BS)
#define PTE_EXT_TEX_BS(x)	(_AT(pteval_t_bs, (x)) << 6)       /* v5 */
#define PTE_EXT_APX_BS		(_AT(pteval_t_bs, 1) << 9)         /* v6 */
#define PTE_EXT_COHERENT_BS	(_AT(pteval_t_bs, 1) << 9)         /* XScale3 */
#define PTE_EXT_SHARED_BS	(_AT(pteval_t_bs, 1) << 10)        /* v6 */
#define PTE_EXT_NG_BS		(_AT(pteval_t_bs, 1) << 11)        /* v6 */

/*
 *   - small page
 */
#define PTE_SMALL_AP_MASK_BS	(_AT(pteval_t_bs, 0xff) << 4)
#define PTE_SMALL_AP_UNO_SRO_BS	(_AT(pteval_t_bs, 0x00) << 4)
#define PTE_SMALL_AP_UNO_SRW_BS	(_AT(pteval_t_bs, 0x55) << 4)
#define PTE_SMALL_AP_URO_SRW_BS	(_AT(pteval_t_bs, 0xaa) << 4)
#define PTE_SMALL_AP_URW_SRW_BS	(_AT(pteval_t_bs, 0xff) << 4)

#define PHYS_MASK_BS		(~0UL)

#ifndef __ASSEMBLY__
extern pgprot_t_bs		pgprot_kernel_bs;
#endif

#define _MOD_PROT_BS(p, b)	__pgprot_bs(pgprot_val_bs(p) | (b))

#define PAGE_KERNEL_BS		_MOD_PROT_BS(pgprot_kernel_bs, L_PTE_XN_BS)
#define __PAGE_NONE_BS		__pgprot_bs(_L_PTE_DEFAULT_BS | \
				L_PTE_RDONLY_BS | \
				L_PTE_XN_BS | L_PTE_NONE_BS) 
#define __PAGE_SHARED_BS	__pgprot_bs(_L_PTE_DEFAULT_BS | \
				L_PTE_USER_BS | L_PTE_XN_BS)
#define __PAGE_SHARED_EXEC_BS	__pgprot_bs(_L_PTE_DEFAULT_BS | L_PTE_USER_BS)
#define __PAGE_COPY_BS		__pgprot_bs(_L_PTE_DEFAULT_BS | \
				L_PTE_USER_BS | \
				L_PTE_RDONLY_BS | L_PTE_XN_BS)
#define __PAGE_COPY_EXEC_BS	__pgprot_bs(_L_PTE_DEFAULT_BS | \
				L_PTE_USER_BS | L_PTE_RDONLY_BS)
#define __PAGE_READONLY_BS	__pgprot_bs(_L_PTE_DEFAULT_BS | \
				L_PTE_USER_BS | \
				L_PTE_RDONLY_BS | L_PTE_XN_BS)
#define __PAGE_READONLY_EXEC_BS	__pgprot_bs(_L_PTE_DEFAULT_BS | \
				L_PTE_USER_BS | L_PTE_RDONLY_BS)

/*
 * The table below defines the page protection levels that we insert into our
 * Linux page table version.  These get translated into the best that the
 * architecture can perform.  Note that on most ARM hardware:
 *  1) We cannot do execute protection
 *  2) If we could do execute protection, then read is implied
 *  3) write implies read permissions 
 */
#define __P000_BS	__PAGE_NONE_BS
#define __P001_BS	__PAGE_READONLY_BS
#define __P010_BS	__PAGE_COPY_BS
#define __P011_BS	__PAGE_COPY_BS
#define __P100_BS	__PAGE_READONLY_EXEC_BS
#define __P101_BS	__PAGE_READONLY_EXEC_BS
#define __P110_BS	__PAGE_COPY_EXEC_BS
#define __P111_BS	__PAGE_COPY_EXEC_BS

#define __S000_BS	__PAGE_NONE_BS
#define __S001_BS	__PAGE_READONLY_BS
#define __S010_BS	__PAGE_SHARED_BS
#define __S011_BS	__PAGE_SHARED_BS
#define __S100_BS	__PAGE_READONLY_EXEC_BS
#define __S101_BS	__PAGE_READONLY_EXEC_BS
#define __S110_BS	__PAGE_SHARED_EXEC_BS
#define __S111_BS	__PAGE_SHARED_EXEC_BS

#ifndef __ASSEMBLY__
extern phys_addr_t swapper_pg_dir_bs;
extern void cpu_v7_set_pte_ext_bs(pte_t_bs *, pte_t_bs, unsigned int);
#include "biscuitos/mm_types.h"
extern struct mm_struct_bs init_mm_bs;

#define pmd_none_bs(pmd)	(!pmd_val_bs(pmd))
#define pmd_bad_bs(pmd)		(pmd_val_bs(pmd) & 2)
#define pmd_present_bs(pmd)	(pmd_val_bs(pmd))

/* FIXME: Host function */

#define pmd_clear_bs(pmdp)						\
	do {								\
		pmdp[0] = __pmd_bs(0);					\
		pmdp[1] = __pmd_bs(0);					\
		/* FIXME: */						\
		/* clean_pmd_entry(pmdp); */				\
	} while (0)

static inline pte_t_bs *pmd_page_kernel_bs(pmd_t_bs pmd)
{
	unsigned long ptr;

	ptr = pmd_val_bs(pmd) & ~(PTRS_PER_PTE_BS * sizeof(void *) - 1);
	ptr += PTRS_PER_PTE_BS * sizeof(void *);

	return __va_bs(ptr);
}

/*
 * The "pgd_xxx()" functions here are trivial for a folded two-level
 * setup: the pgd is never bad, and a pmd always exists (as it's folded
 * into the pgd entry)
 */
#define pgd_none_bs(pgd)	(0)
#define pgd_bad_bs(pgd)		(0)
#define pgd_present_bs(pgd)	(1)
#define pgd_clear_bs(pgdp)	do { } while (0)
#define set_pgd_bs(pgd,pgdp)	do { } while (0)
extern void pgd_clear_bad_bs(pgd_t_bs *pgd);
#define pgd_ERROR_bs(pgd)					\
			printk("%s %d %ld", __FILE__,__LINE__,pgd_val_bs(pgd))

static inline int pgd_none_or_clear_bad_bs(pgd_t_bs *pgd)
{
	if (pgd_none_bs(*pgd))
		return 1;
	if (unlikely(pgd_bad_bs(*pgd))) {
		pgd_clear_bad_bs(pgd);
		return 1;
	}
	return 0;
}

/* we don't need complex calculations here as the pmd is folded into the pgd */
#define pmd_addr_end_bs(addr,end)	(end)

/* to find an entry in a page-table-directory */
#define pgd_index_bs(addr)		((addr) >> PGDIR_SHIFT_BS)

#define pgd_offset_bs(mm, addr)		((mm)->pgd+pgd_index_bs(addr))

/* to find an entry in a kernel page-table-directory */
#define pgd_offset_k_bs(addr)		pgd_offset_bs(&init_mm_bs, addr)

static inline pud_t_bs *pud_offset_bs(pgd_t_bs *pgd, unsigned long address)
{
	return (pud_t_bs *)pgd;
}

#define pud_none_bs(pud)			0
#define pud_present_bs(pud)			1
#define pud_bad_bs(pud)				0
#define pud_ERROR_bs(pud)			do { } while (0)
#define pud_clear_bs(pud)			pgd_clear_bs(pud)
#define pud_alloc_bs(mm, pgd, address)		(pud_t_bs *)(pgd)
#define pud_addr_end_bs(addr, end)		(end)
extern void pud_clear_bad_bs(pud_t_bs *pud);

static inline int pud_none_or_clear_bad_bs(pud_t_bs *pud)
{
	if (pud_none_bs(*pud))
		return 1;
	if (unlikely(pud_bad_bs(*pud))) {
		pud_clear_bad_bs(pud);
		return 1;
	}
	return 0;
}

/* Find an entry in the second-level page table.. */
#define pmd_offset_bs(dir, addr)	((pmd_t_bs *)(dir))

extern pmd_t_bs fastcall_bs *__pmd_alloc_bs(struct mm_struct_bs *mm, 
					pud_t_bs *pud, unsigned long address);

#define pmd_alloc_bs(mm, pud, address)					\
({									\
	pmd_t_bs *ret;							\
	if (pgd_none_bs(*pud))						\
		ret = __pmd_alloc_bs(mm, pud, address);			\
	else								\
		ret = pmd_offset_bs(pud, address);			\
	ret;								\
})

static inline pmd_t_bs *pmd_off_k_bs(unsigned long virt)
{
	return pmd_offset_bs(pud_offset_bs(pgd_offset_k_bs(virt), virt), virt);
}

#define pte_none_bs(pte)		(!pte_val_bs(pte))

/*      
 * Conversion functions: convert a page and protection to a page entry,
 * and a page entry and page directory to the page they refer to.
 */
#define mk_pte_bs(page, prot)		pfn_pte_bs(page_to_pfn_bs(page),prot)

/* Find an entry in the third-level page table.. */
#define __pte_index_bs(addr)		(((addr) >> PAGE_SHIFT_BS) & \
						(PTRS_PER_PTE_BS - 1))

#define pte_offset_kernel_bs(dir,addr)	(pmd_page_kernel_bs(*(dir)) + \
					__pte_index_bs(addr))
#define pfn_pte_bs(pfn,prot)		(__pte_bs(((pfn) << PAGE_SHIFT_BS) | \
					pgprot_val_bs(prot)))
#define pte_pfn_bs(pte)			(pte_val_bs(pte) >> PAGE_SHIFT)

#define pte_offset_map_bs(dir,addr)	(pmd_page_kernel_bs(*(dir)) + \
						__pte_index_bs(addr))
#define pte_unmap_bs(pte)		do { } while (0)
#define pte_page_bs(pte)		(pfn_to_page_bs(pte_pfn_bs(pte)))
#define set_pte_bs(ptep, pte)	cpu_v7_set_pte_ext_bs(ptep, pte, 0)
#define set_pte_at_bs(mm,addr,ptep,pteval)	set_pte_bs(ptep,pteval)
#define pte_clear_bs(mm,addr,ptep)					\
				set_pte_at_bs((mm),(addr),(ptep), __pte_bs(0))
/*
 * The following only work if pte_present() is true.
 * Undefined behaviour if not..
 */
#define pte_present_bs(pte)	(pte_val_bs(pte) & L_PTE_PRESENT_BS)
#define pte_read_bs(pte)	(pte_val_bs(pte) & L_PTE_USER_BS)
#define pte_write_bs(pte)	(pte_val_bs(pte) & L_PTE_WRITE_BS)
#define pte_exec_bs(pte)	(pte_val_bs(pte) & L_PTE_EXEC_BS)
#define pte_dirty_bs(pte)	(pte_val_bs(pte) & L_PTE_DIRTY_BS)
#define pte_young_bs(pte)	(pte_val_bs(pte) & L_PTE_YOUNG_BS)

#define ptep_get_and_clear_bs(__mm, __address, __ptep)			\
({									\
	pte_t_bs __pte = *(__ptep);					\
	pte_clear_bs((__mm), (__address), (__ptep));			\
	__pte;								\
})

typedef void (*cpu_set_pte_ext_t)(pte_t_bs *, pte_t_bs, unsigned int);

/*
 * When walking page tables, get the address of the next boundary,
 * or the end address of the range if that comes earlier.  Although no
 * vma end wraps to 0, rounded up __boundary may wrap to 0 throughout.
 */

#define pgd_addr_end_bs(addr, end)					\
({	unsigned long __boundary = ((addr) + PGDIR_SIZE_BS) & PGDIR_MASK_BS; \
	(__boundary - 1 < (end) - 1)? __boundary: (end);		\
})


#endif

/* Needs to be defined here and not in linux/mm.h, as it is arch dependent */
/* FIXME: this is not correct */
#define kern_addr_valid_bs(addr)	(1)
#define pte_mkold_bs(x)			(0)

#ifndef __HAVE_ARCH_PAGE_TEST_AND_CLEAR_YOUNG
#define page_test_and_clear_young_bs(page)	(0)
#endif

#ifndef __HAVE_ARCH_PTEP_TEST_AND_CLEAR_YOUNG
#define ptep_test_and_clear_young_bs(__vma, __address, __ptep)\
({\
	pte_t_bs __pte = *(__ptep);					\
	int r = 1;							\
	if (!pte_young_bs(__pte))					\
		r = 0;							\
	else								\
		set_pte_at_bs((__vma)->vm_mm, (__address),		\
			   (__ptep), pte_mkold_bs(__pte));		\
	r;								\
})              
#endif

#ifndef __HAVE_ARCH_PTEP_CLEAR_YOUNG_FLUSH
#define ptep_clear_flush_young_bs(__vma, __address, __ptep)		\
({									\
	int __young;							\
	__young = ptep_test_and_clear_young_bs(__vma, __address, __ptep); \
	if (__young)							\
		flush_tlb_page_bs(__vma, __address);			\
	__young;							\
})
#endif

#endif
