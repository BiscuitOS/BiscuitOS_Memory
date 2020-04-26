/*
 * mm/memory.c
 *
 * (C) 2020.04.14 BuddyZhang1 <buddy.zhang@aliyun.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/kernel.h>
#include "biscuitos/mm.h"
#include "biscuitos/kernel.h"
#include "biscuitos/linkage.h"
#include "asm-generated/pgalloc.h"
#include "asm-generated/pgtable.h"

#ifndef CONFIG_DISCONTIGMEM
/* use the per-pgdat data instead for discontigmem - mbligh */
unsigned long max_mapnr_bs;
struct page_bs *mem_map_bs;

EXPORT_SYMBOL_GPL(max_mapnr_bs);
EXPORT_SYMBOL_GPL(mem_map_bs);
#endif

unsigned long num_physpages_bs;
/*
 * A number of key systems in x86 including ioremap() relay on the assumption
 * that high_memory defines the upper bound on direct map memory, then end
 * of ZONE_NORMAL. Under CONFIG_DISCONTIG this means that max_low_pfn and
 * highstart_pfn mut be the same; there must be no gap between ZONE_NORMAL
 * and ZONE_HIGHMEM.
 */
void *high_memory_bs;
EXPORT_SYMBOL_GPL(high_memory_bs);
EXPORT_SYMBOL_GPL(num_physpages_bs);

/*
 * Allocate page middle directory.
 *
 * We've already handled the fast-path in-line, and we own the
 * page table lock.
 */
pmd_t_bs fastcall_bs *__pmd_alloc_bs(struct mm_struct_bs *mm, 
				pud_t_bs *pud, unsigned long address)
{
	BS_DUP();
	return (pmd_t_bs *)pud;
}

pte_t_bs fastcall_bs *pte_alloc_kernel_bs(struct mm_struct_bs *mm,
				pmd_t_bs *pmd, unsigned long address)
{
	if (!pmd_present_bs(*pmd)) {
		pte_t_bs *new;

		spin_unlock(&mm->page_table_lock);
		new = pte_alloc_one_kernel_bs(mm, address);
		spin_lock(&mm->page_table_lock);
		if (!new)
			return NULL;

		/*
		 * Because we dropped the lock, we should re-check the 
		 * entry, as somebody else could have populated it..
		 */
		if (pmd_present_bs(*pmd)) {
			pte_free_kernel_bs(new);
		}
		pmd_populate_kernel_bs(mm, pmd, new);
	}
	return pte_offset_kernel_bs(pmd, address);
}

/*
 * If a p?d_bad entry is found while walking page tables, report
 * the error, before resetting entry to p?d_none.  Usually (but
 * very seldom) called out from the p?d_none_or_clear_bad macros.
 */

void pgd_clear_bad_bs(pgd_t_bs *pgd)
{
	pgd_ERROR_bs(*pgd);
	pgd_clear_bs(pgd);
}

void pud_clear_bad_bs(pud_t_bs *pud)
{
	pud_ERROR_bs(*pud);
	pud_clear_bs(pud);
}
