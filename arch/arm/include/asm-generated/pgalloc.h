#ifndef _BISCUITOS_ARM_PGALLOC_H
#define _BISCUITOS_ARM_PGALLOC_H
/*
 *  arch/arm/include/asm/pgalloc.h
 *
 *  Copyright (C) 2000-2001 Russell King
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include "asm-generated/pgtable.h"
#include "asm-generated/domain.h"
#include "asm-generated/tlbflush.h"

#define _PAGE_USER_TABLE	(PMD_TYPE_TABLE | PMD_BIT4 | \
					PMD_DOMAIN(DOMAIN_USER))
#define _PAGE_KERNEL_TABLE	(PMD_TYPE_TABLE | PMD_BIT4 | \
					PMD_DOMAIN(DOMAIN_KERNEL))


/*              
 * Allocate one PTE table.
 *                      
 * This actually allocates two hardware PTE tables, but we wrap this up
 * into one table thus:
 *
 *  +------------+
 *  |  h/w pt 0  |
 *  +------------+ 
 *  |  h/w pt 1  |      
 *  +------------+      
 *  | Linux pt 0 |
 *  +------------+
 *  | Linux pt 1 |      
 *  +------------+
 */
static inline pte_t_bs *
pte_alloc_one_kernel_bs(struct mm_struct_bs *mm, unsigned long addr)
{               
	pte_t_bs *pte;

	pte = (pte_t_bs *)__get_free_page_bs(GFP_KERNEL_BS | __GFP_REPEAT_BS |
                                                __GFP_ZERO_BS);
	if (pte) {
		clean_dcache_area_bs(pte, sizeof(pte_t_bs) * PTRS_PER_PTE);
		pte += PTRS_PER_PTE;
	}

	return pte;
}
        
/*
 * Free one PTE table
 */
static inline void pte_free_kernel_bs(pte_t_bs *pte)
{
	if (pte) {
		pte -= PTRS_PER_PTE;
		free_page_bs((unsigned long)pte);
	}
}
        
/*      
 * Populate the pmdp entry with a pointer to the pte.  This pmd is part
 * of the mm address space.
 *
 * Ensure that we always set both PMD entries.
 */
static inline void
pmd_populate_kernel_bs(struct mm_struct_bs *mm,
				pmd_t_bs *pmdp, pte_t_bs *ptep)
{
	unsigned long pte_ptr = (unsigned long)ptep;
	unsigned long pmdval;

	BUG_ON_BS(mm != &init_mm_bs);

	/*
	 * The pmd must be loaded with the physical
	 * address of the PTE table.
	 */
	pte_ptr -= PTRS_PER_PTE * sizeof(void *);
	pmdval = __pa_bs(pte_ptr) | _PAGE_KERNEL_TABLE;
	pmdp[0] = __pmd_bs(pmdval);
	pmdp[1] = __pmd_bs(pmdval + 256 * sizeof(pte_t_bs));
	flush_pmd_entry_bs(pmdp);
}

#endif
