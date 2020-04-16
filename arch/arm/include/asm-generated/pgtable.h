#ifndef _BISCUITOS_ARM_PGTABLE_H
#define _BISCUITOS_ARM_PGTABLE_H

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
#define L_PTE_PRESENT		(1 << 0)
#define L_PTE_FILE		(1 << 1)	/* only when !PRESENT */
#define L_PTE_YOUNG		(1 << 1)
#define L_PTE_BUFFERABLE	(1 << 2)	/* matches PTE */
#define L_PTE_CACHEABLE		(1 << 3)	/* matches PTE */
#define L_PTE_USER		(1 << 4)
#define L_PTE_WRITE		(1 << 5)
#define L_PTE_EXEC		(1 << 6)
#define L_PTE_DIRTY		(1 << 7)

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

/*
 * The following macros handle the cache and bufferable bits...
 */
#define _L_PTE_DEFAULT		L_PTE_PRESENT | L_PTE_YOUNG | \
				L_PTE_CACHEABLE | L_PTE_BUFFERABLE
#define _L_PTE_READ		L_PTE_USER | L_PTE_EXEC

extern pgprot_t_bs		pgprot_kernel_bs;

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

extern phys_addr_t swapper_pg_dir_bs;
/* FIXME: How to fix these header on here? */
#include "biscuitos/mm_types.h"
extern struct mm_struct_bs init_mm_bs;

#define pmd_clear_bs(pmdp)						\
	do {								\
		pmdp[0] = __pmd_bs(0);					\
		pmdp[1] = __pmd_bs(0);					\
	} while (0)

/* to find an entry in a page-table-directory */
#define pgd_index_bs(addr)		((addr) >> PGDIR_SHIFT)

#define pgd_offset_bs(mm, addr)		((mm)->pgd+pgd_index_bs(addr))

/* to find an entry in a kernel page-table-directory */
#define pgd_offset_k_bs(addr)		pgd_offset_bs(&init_mm_bs, addr)


#endif
