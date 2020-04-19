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

/*
 * These are used to make use of C type-checking
 */
typedef struct { unsigned long pte; } pte_t_bs;
typedef struct { unsigned long pmd; } pmd_t_bs;
typedef struct { unsigned long pgd[2]; } pgd_t_bs;
typedef struct { unsigned long pgprot; } pgprot_t_bs;

typedef u32 pteval_t_bs;
typedef u32 pmdval_t_bs;

typedef struct { pgd_t_bs pgd; } pud_t_bs;

#define pte_val_bs(x)		((x).pte)
#define pmd_val_bs(x)		((x).pmd)
#define pgd_val_bs(x)		((x).pgd[0])
#define pgprot_val_bs(x)	((x).pgprot)

#define __pte_bs(x)		((pte_t_bs) { (x) } )
#define __pmd_bs(x)		((pmd_t_bs) { (x) } )
#define __pgprot_bs(x)		((pgprot_t_bs) { (x) } )

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
#define PTRS_PER_PTE		512
#define PTRS_PER_PMD		1
#define PTRS_PER_PGD		2048

#define PTE_HWTABLE_PTRS	(PTRS_PER_PTE)
#define PTE_HWTABLE_OFF		(PTE_HWTABLE_PTRS * sizeof(pte_t))
#define PTE_HWTABLE_SIZE	(PTRS_PER_PTE * sizeof(u32))

/*
 * PMD_SHIFT determines the size of the area a second-level page table can map
 * PGDIR_SHIFT determines what a third-level page table entry can map
 */
#define PMD_SHIFT		21
#define PGDIR_SHIFT		21

#define PMD_SIZE		(1UL << PMD_SHIFT)
#define PMD_MASK		(~(PMD_SIZE-1))
#define PGDIR_SIZE		(1UL << PGDIR_SHIFT)
#define PGDIR_MASK		(~(PGDIR_SIZE-1))

#define PUD_SHIFT		PGDIR_SHIFT
#define PTRS_PER_PUD		1
#define PUD_SIZE		(1UL << PUD_SHIFT)
#define PUD_MASK		(~(PUD_SIZE-1))

/*
 * section address mask and size definitions.
 */
#define SECTION_SHIFT		20
#define SECTION_SIZE		(1UL << SECTION_SHIFT)
#define SECTION_MASK		(~(SECTION_SIZE-1))

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
#define L_PTE_VALID		(_AT(pteval_t_bs, 1) << 0)         /* Valid */
#define L_PTE_PRESENT		(_AT(pteval_t_bs, 1) << 0)
#define L_PTE_YOUNG		(_AT(pteval_t_bs, 1) << 1)
#define L_PTE_DIRTY		(_AT(pteval_t_bs, 1) << 6)
#define L_PTE_RDONLY		(_AT(pteval_t_bs, 1) << 7)
#define L_PTE_USER		(_AT(pteval_t_bs, 1) << 8)
#define L_PTE_XN		(_AT(pteval_t_bs, 1) << 9)
#define L_PTE_SHARED		(_AT(pteval_t_bs, 1) << 10)        /* shared(v6), coherent(xsc3) */
#define L_PTE_NONE		(_AT(pteval_t_bs, 1) << 11)

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
#define L_PTE_MT_UNCACHED	(_AT(pteval_t_bs, 0x00) << 2)      /* 0000 */
#define L_PTE_MT_BUFFERABLE	(_AT(pteval_t_bs, 0x01) << 2)      /* 0001 */
#define L_PTE_MT_WRITETHROUGH	(_AT(pteval_t_bs, 0x02) << 2)      /* 0010 */
#define L_PTE_MT_WRITEBACK	(_AT(pteval_t_bs, 0x03) << 2)      /* 0011 */
#define L_PTE_MT_MINICACHE	(_AT(pteval_t_bs, 0x06) << 2)      /* 0110 (sa1100, xscale) */
#define L_PTE_MT_WRITEALLOC	(_AT(pteval_t_bs, 0x07) << 2)      /* 0111 */
#define L_PTE_MT_DEV_SHARED	(_AT(pteval_t_bs, 0x04) << 2)      /* 0100 */
#define L_PTE_MT_DEV_NONSHARED	(_AT(pteval_t_bs, 0x0c) << 2)      /* 1100 */
#define L_PTE_MT_DEV_WC		(_AT(pteval_t_bs, 0x09) << 2)      /* 1001 */
#define L_PTE_MT_DEV_CACHED	(_AT(pteval_t_bs, 0x0b) << 2)      /* 1011 */
#define L_PTE_MT_VECTORS	(_AT(pteval_t_bs, 0x0f) << 2)      /* 1111 */
#define L_PTE_MT_MASK		(_AT(pteval_t_bs, 0x0f) << 2)

/*
 * The pgprot_* and protection_map entries will be fixed up in runtime
 * to include the cachable and bufferable bits based on memory policy,
 * as well as any architecture dependent bits like global/ASID and SMP
 * shared mapping bits.
 */
#define _L_PTE_DEFAULT		L_PTE_PRESENT | L_PTE_YOUNG

/*
 * ARMv6 supersection address mask and size definitions.
 */
#define SUPERSECTION_SHIFT	24
#define SUPERSECTION_SIZE	(1UL << SUPERSECTION_SHIFT)
#define SUPERSECTION_MASK	(~(SUPERSECTION_SIZE-1))

/*
 * Hardware page table definitions.
 *
 * + Level 1 descriptor (PMD)
 *   - common
 */
#define PMD_TYPE_MASK		(_AT(pmdval_t_bs, 3) << 0)
#define PMD_TYPE_FAULT		(_AT(pmdval_t_bs, 0) << 0)
#define PMD_TYPE_TABLE		(_AT(pmdval_t_bs, 1) << 0)
#define PMD_TYPE_SECT		(_AT(pmdval_t_bs, 2) << 0)
#define PMD_PXNTABLE		(_AT(pmdval_t_bs, 1) << 2)     /* v7 */
#define PMD_BIT4		(_AT(pmdval_t_bs, 1) << 4)
#define PMD_DOMAIN(x)		(_AT(pmdval_t_bs, (x)) << 5)
#define PMD_DOMAIN_MASK		PMD_DOMAIN(0x0f)
#define PMD_PROTECTION		(_AT(pmdval_t_bs, 1) << 9)         /* v5 */
/*
 *   - section
 */
#define PMD_SECT_PXN		(_AT(pmdval_t_bs, 1) << 0)     /* v7 */
#define PMD_SECT_BUFFERABLE	(_AT(pmdval_t_bs, 1) << 2)
#define PMD_SECT_CACHEABLE	(_AT(pmdval_t_bs, 1) << 3)
#define PMD_SECT_XN		(_AT(pmdval_t_bs, 1) << 4)         /* v6 */
#define PMD_SECT_AP_WRITE	(_AT(pmdval_t_bs, 1) << 10)
#define PMD_SECT_AP_READ	(_AT(pmdval_t_bs, 1) << 11)
#define PMD_SECT_TEX(x)		(_AT(pmdval_t_bs, (x)) << 12)      /* v5 */
#define PMD_SECT_APX		(_AT(pmdval_t_bs, 1) << 15)        /* v6 */
#define PMD_SECT_S		(_AT(pmdval_t_bs, 1) << 16)        /* v6 */
#define PMD_SECT_nG		(_AT(pmdval_t_bs, 1) << 17)        /* v6 */
#define PMD_SECT_SUPER		(_AT(pmdval_t_bs, 1) << 18)        /* v6 */
#define PMD_SECT_AF		(_AT(pmdval_t_bs, 0))

#define PMD_SECT_UNCACHED	(_AT(pmdval_t_bs, 0))
#define PMD_SECT_BUFFERED	(PMD_SECT_BUFFERABLE)
#define PMD_SECT_WT		(PMD_SECT_CACHEABLE)
#define PMD_SECT_WB		(PMD_SECT_CACHEABLE | PMD_SECT_BUFFERABLE)
#define PMD_SECT_MINICACHE	(PMD_SECT_TEX(1) | PMD_SECT_CACHEABLE)
#define PMD_SECT_WBWA		(PMD_SECT_TEX(1) | PMD_SECT_CACHEABLE | \
				PMD_SECT_BUFFERABLE)
#define PMD_SECT_CACHE_MASK	(PMD_SECT_TEX(1) | PMD_SECT_CACHEABLE | \
				PMD_SECT_BUFFERABLE)
#define PMD_SECT_NONSHARED_DEV	(PMD_SECT_TEX(2))

/*
 *   - coarse table (not used)
 */

/*
 * + Level 2 descriptor (PTE)
 *   - common
 */
#define PTE_TYPE_MASK		(_AT(pteval_t_bs, 3) << 0)
#define PTE_TYPE_FAULT		(_AT(pteval_t_bs, 0) << 0)
#define PTE_TYPE_LARGE		(_AT(pteval_t_bs, 1) << 0)
#define PTE_TYPE_SMALL		(_AT(pteval_t_bs, 2) << 0)
#define PTE_TYPE_EXT		(_AT(pteval_t_bs, 3) << 0)         /* v5 */
#define PTE_BUFFERABLE		(_AT(pteval_t_bs, 1) << 2)
#define PTE_CACHEABLE		(_AT(pteval_t_bs, 1) << 3)

/*
 *   - extended small page/tiny page
 */
#define PTE_EXT_XN		(_AT(pteval_t_bs, 1) << 0)         /* v6 */
#define PTE_EXT_AP_MASK		(_AT(pteval_t_bs, 3) << 4)
#define PTE_EXT_AP0		(_AT(pteval_t_bs, 1) << 4)
#define PTE_EXT_AP1		(_AT(pteval_t_bs, 2) << 4)
#define PTE_EXT_AP_UNO_SRO	(_AT(pteval_t_bs, 0) << 4)
#define PTE_EXT_AP_UNO_SRW	(PTE_EXT_AP0)
#define PTE_EXT_AP_URO_SRW	(PTE_EXT_AP1)
#define PTE_EXT_AP_URW_SRW	(PTE_EXT_AP1|PTE_EXT_AP0)
#define PTE_EXT_TEX(x)		(_AT(pteval_t_bs, (x)) << 6)       /* v5 */
#define PTE_EXT_APX		(_AT(pteval_t_bs, 1) << 9)         /* v6 */
#define PTE_EXT_COHERENT	(_AT(pteval_t_bs, 1) << 9)         /* XScale3 */
#define PTE_EXT_SHARED		(_AT(pteval_t_bs, 1) << 10)        /* v6 */
#define PTE_EXT_NG		(_AT(pteval_t_bs, 1) << 11)        /* v6 */

/*
 *   - small page
 */
#define PTE_SMALL_AP_MASK	(_AT(pteval_t_bs, 0xff) << 4)
#define PTE_SMALL_AP_UNO_SRO	(_AT(pteval_t_bs, 0x00) << 4)
#define PTE_SMALL_AP_UNO_SRW	(_AT(pteval_t_bs, 0x55) << 4)
#define PTE_SMALL_AP_URO_SRW	(_AT(pteval_t_bs, 0xaa) << 4)
#define PTE_SMALL_AP_URW_SRW	(_AT(pteval_t_bs, 0xff) << 4)

#define PHYS_MASK		(~0UL)

#ifndef __ASSEMBLY__
extern pgprot_t_bs		pgprot_kernel_bs;
#endif

#define __PAGE_NONE		__pgprot_bs(_L_PTE_DEFAULT | L_PTE_RDONLY | \
						L_PTE_XN | L_PTE_NONE) 
#define __PAGE_SHARED		__pgprot_bs(_L_PTE_DEFAULT | L_PTE_USER | \
						L_PTE_XN)
#define __PAGE_SHARED_EXEC	__pgprot_bs(_L_PTE_DEFAULT | L_PTE_USER)
#define __PAGE_COPY		__pgprot_bs(_L_PTE_DEFAULT | L_PTE_USER | \
						L_PTE_RDONLY | L_PTE_XN)
#define __PAGE_COPY_EXEC	__pgprot_bs(_L_PTE_DEFAULT | L_PTE_USER | \
						L_PTE_RDONLY)
#define __PAGE_READONLY		__pgprot_bs(_L_PTE_DEFAULT | L_PTE_USER | \
						L_PTE_RDONLY | L_PTE_XN)
#define __PAGE_READONLY_EXEC	__pgprot_bs(_L_PTE_DEFAULT | L_PTE_USER | \
						L_PTE_RDONLY)

/*
 * The table below defines the page protection levels that we insert into our
 * Linux page table version.  These get translated into the best that the
 * architecture can perform.  Note that on most ARM hardware:
 *  1) We cannot do execute protection
 *  2) If we could do execute protection, then read is implied
 *  3) write implies read permissions 
 */
#define __P000	__PAGE_NONE
#define __P001	__PAGE_READONLY
#define __P010	__PAGE_COPY
#define __P011	__PAGE_COPY
#define __P100	__PAGE_READONLY_EXEC
#define __P101	__PAGE_READONLY_EXEC
#define __P110	__PAGE_COPY_EXEC
#define __P111	__PAGE_COPY_EXEC

#define __S000	__PAGE_NONE
#define __S001	__PAGE_READONLY
#define __S010	__PAGE_SHARED
#define __S011	__PAGE_SHARED
#define __S100	__PAGE_READONLY_EXEC
#define __S101	__PAGE_READONLY_EXEC
#define __S110	__PAGE_SHARED_EXEC
#define __S111	__PAGE_SHARED_EXEC

#ifndef __ASSEMBLY__
extern phys_addr_t swapper_pg_dir_bs;
/* FIXME: How to fix these header on here? */
#include "biscuitos/mm_types.h"
extern struct mm_struct_bs init_mm_bs;
extern void cpu_v7_set_pte_ext_bs(pte_t_bs *, pte_t_bs, unsigned int);

#define pmd_none_bs(pmd)	(!pmd_val_bs(pmd))

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

	ptr = pmd_val_bs(pmd) & ~(PTRS_PER_PTE * sizeof(void *) - 1);
	ptr += PTRS_PER_PTE * sizeof(void *);

	return __va_bs(ptr);
}

/* we don't need complex calculations here as the pmd is folded into the pgd */
#define pmd_addr_end_bs(addr,end)	(end)

/* to find an entry in a page-table-directory */
#define pgd_index_bs(addr)		((addr) >> PGDIR_SHIFT)

#define pgd_offset_bs(mm, addr)		((mm)->pgd+pgd_index_bs(addr))

/* to find an entry in a kernel page-table-directory */
#define pgd_offset_k_bs(addr)		pgd_offset_bs(&init_mm_bs, addr)

static inline pud_t_bs *pud_offset_bs(pgd_t_bs *pgd, unsigned long address)
{
	return (pud_t_bs *)pgd;
}

#define pud_addr_end_bs(addr, end)		(end)

/* Find an entry in the second-level page table.. */
#define pmd_offset_bs(dir, addr)	((pmd_t_bs *)(dir))

/* Find an entry in the third-level page table.. */
#define __pte_index_bs(addr)		(((addr) >> PAGE_SHIFT_BS) & \
						(PTRS_PER_PTE - 1))

#define pte_offset_kernel_bs(dir,addr)	(pmd_page_kernel_bs(*(dir)) + \
					__pte_index_bs(addr))
#define pfn_pte_bs(pfn,prot)		(__pte_bs(((pfn) << PAGE_SHIFT_BS) | \
					pgprot_val_bs(prot)))

#define set_pte_bs(ptep, pte)	cpu_v7_set_pte_ext_bs(ptep, pte, 0)

typedef void (*cpu_set_pte_ext_t)(pte_t_bs *, pte_t_bs, unsigned int);

/*
 * When walking page tables, get the address of the next boundary,
 * or the end address of the range if that comes earlier.  Although no
 * vma end wraps to 0, rounded up __boundary may wrap to 0 throughout.
 */

#define pgd_addr_end_bs(addr, end)					\
({	unsigned long __boundary = ((addr) + PGDIR_SIZE) & PGDIR_MASK;	\
	(__boundary - 1 < (end) - 1)? __boundary: (end);		\
})


#endif

#endif
