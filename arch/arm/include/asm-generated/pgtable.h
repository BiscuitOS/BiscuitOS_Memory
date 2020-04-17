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
#define L_PTE_VALID		(1 << 0)
#define L_PTE_PRESENT		(1 << 0)
#define L_PTE_FILE		(1 << 1)	/* only when !PRESENT */
#define L_PTE_YOUNG		(1 << 1)
#define L_PTE_BUFFERABLE	(1 << 2)	/* matches PTE */
#define L_PTE_CACHEABLE		(1 << 3)	/* matches PTE */
#define L_PTE_USER		(1 << 8)
#define L_PTE_WRITE		(1 << 5)
#define L_PTE_EXEC		(1 << 6)
#define L_PTE_DIRTY		(1 << 6)
#define L_PTE_RDONLY		(1 << 7)
#define L_PTE_XN		(1 << 9)
#define L_PTE_NONE		(1 << 11)

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
#define PMD_TYPE_MASK		(3 << 0)
#define PMD_TYPE_FAULT		(0 << 0)
#define PMD_TYPE_TABLE		(1 << 0)
#define PMD_TYPE_SECT		(2 << 0)
#define PMD_BIT4		(1 << 4)
#define PMD_DOMAIN(x)		((x) << 5)
#define PMD_PROTECTION		(1 << 9)	/* v5 */
/*
 *   - section
 */
#define PMD_SECT_BUFFERABLE	(1 << 2)
#define PMD_SECT_CACHEABLE	(1 << 3)
#define PMD_SECT_AP_WRITE	(1 << 10)
#define PMD_SECT_AP_READ	(1 << 11)
#define PMD_SECT_TEX(x)		((x) << 12)	/* v5 */
#define PMD_SECT_APX		(1 << 15)	/* v6 */
#define PMD_SECT_S		(1 << 16)	/* v6 */
#define PMD_SECT_nG		(1 << 17)	/* v6 */
#define PMD_SECT_SUPER		(1 << 18)	/* v6 */

#define PMD_SECT_UNCACHED	(0)
#define PMD_SECT_BUFFERED	(PMD_SECT_BUFFERABLE)
#define PMD_SECT_WT		(PMD_SECT_CACHEABLE)
#define PMD_SECT_WB		(PMD_SECT_CACHEABLE | PMD_SECT_BUFFERABLE)
#define PMD_SECT_MINICACHE	(PMD_SECT_TEX(1) | PMD_SECT_CACHEABLE)
#define PMD_SECT_WBWA		(PMD_SECT_TEX(1) | PMD_SECT_CACHEABLE | \
				 PMD_SECT_BUFFERABLE)

/*
 * PMD_SHIFT determines the size of the area a second-level page table can
 * map PGDIR_SHIFT determines what a third-level page table entry can map
 */
#define PMD_SHIFT		21
#define PGDIR_SHIFT		21

#define PMD_SIZE		(1UL << PMD_SHIFT)
#define PMD_MASK		(~(PMD_SIZE-1))
#define PGDIR_SIZE		(1UL << PGDIR_SHIFT)
#define PGDIR_MASK		(~(PGDIR_SIZE-1))

/*
 * + Level 2 descriptor (PTE)
 *   - common
 */
#define PTE_TYPE_MASK		(3 << 0)
#define PTE_TYPE_FAULT		(0 << 0)
#define PTE_TYPE_LARGE		(1 << 0)
#define PTE_TYPE_SMALL		(2 << 0)
#define PTE_TYPE_EXT		(3 << 0)	/* v5 */
#define PTE_BUFFERABLE		(1 << 2)
#define PTE_CACHEABLE		(1 << 3)

/*
 * - extended small page/tiny page
 */
#define PTE_EXT_XN		(1 << 0)
#define PTE_EXT_AP0		(1 << 4)
#define PTE_EXT_AP1		(2 << 4)
#define PTE_EXT_TEX(x)		((x) << 6)
#define PTE_EXT_APX		(1 << 9)

#ifndef __ASSEMBLY__

/*
 * These are used to make use of C type-checking
 */
typedef struct { unsigned long pte; } pte_t_bs;
typedef struct { unsigned long pmd; } pmd_t_bs;
typedef struct { unsigned long pgd[2]; } pgd_t_bs;
typedef struct { unsigned long pgprot; } pgprot_t_bs;

#endif

#define pte_val_bs(x)		((x).pte)
#define pmd_val_bs(x)		((x).pmd)
#define pgd_val_bs(x)		((x).pgd[0])
#define pgprot_val_bs(x)	((x).pgprot)

#define __pte_bs(x)		((pte_t_bs) { (x) } )
#define __pmd_bs(x)		((pmd_t_bs) { (x) } )
#define __pgprot_bs(x)		((pgprot_t_bs) { (x) } )

/*
 * The following macros handle the cache and bufferable bits...
 */
#define _L_PTE_DEFAULT		L_PTE_PRESENT | L_PTE_YOUNG | \
				L_PTE_CACHEABLE | L_PTE_BUFFERABLE
#define _L_PTE_READ		L_PTE_USER | L_PTE_EXEC

#ifndef __ASSEMBLY__
extern pgprot_t_bs		pgprot_kernel_bs;
#endif

#define PAGE_NONE		__pgprot_bs(_L_PTE_DEFAULT)
#define PAGE_COPY		__pgprot_bs(_L_PTE_DEFAULT | _L_PTE_READ)
#define PAGE_SHARED		__pgprot_bs(_L_PTE_DEFAULT | _L_PTE_READ | \
							      L_PTE_WRITE)
#define PAGE_READONLY		__pgprot_bs(_L_PTE_DEFAULT | _L_PTE_READ)
#define PAGE_KERNEL		pgprot_kernel_bs

/*
 * The table below defines the page protection levels that we insert into our
 * Linux page table version.  These get translated into the best that the
 * architecture can perform.  Note that on most ARM hardware:
 *  1) We cannot do execute protection
 *  2) If we could do execute protection, then read is implied
 *  3) write implies read permissions 
 */
#define __P000	PAGE_NONE
#define __P001	PAGE_READONLY
#define __P010	PAGE_COPY
#define __P011	PAGE_COPY
#define __P100	PAGE_READONLY
#define __P101	PAGE_READONLY
#define __P110	PAGE_COPY
#define __P111	PAGE_COPY

#define __S000	PAGE_NONE
#define __S001	PAGE_READONLY
#define __S010	PAGE_SHARED
#define __S011	PAGE_SHARED
#define __S100	PAGE_READONLY
#define __S101	PAGE_READONLY
#define __S110	PAGE_SHARED
#define __S111	PAGE_SHARED

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

/* to find an entry in a page-table-directory */
#define pgd_index_bs(addr)		((addr) >> PGDIR_SHIFT)

#define pgd_offset_bs(mm, addr)		((mm)->pgd+pgd_index_bs(addr))

/* to find an entry in a kernel page-table-directory */
#define pgd_offset_k_bs(addr)		pgd_offset_bs(&init_mm_bs, addr)

/* Find an entry in the second-level page table.. */
#define pmd_offset_bs(dir, addr)	((pmd_t_bs *)(dir))

/* Find an entry in the third-level page table.. */
#define __pte_index_bs(addr)		(((addr) >> PAGE_SHIFT) & \
						(PTRS_PER_PTE - 1))

#define pte_offset_kernel_bs(dir,addr)	(pmd_page_kernel_bs(*(dir)) + \
					__pte_index_bs(addr))
#define pfn_pte_bs(pfn,prot)		(__pte_bs(((pfn) << PAGE_SHIFT) | \
					pgprot_val_bs(prot)))

#define set_pte_bs(ptep, pte)	cpu_v7_set_pte_ext_bs(ptep, pte, 0)

typedef void (*cpu_set_pte_ext_t)(pte_t_bs *, pte_t_bs, unsigned int);

#endif
#endif
