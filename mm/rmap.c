/*
 * mm/rmap.c - physical to virtual reverse mappings
 *
 * Copyright 2001, Rik van Riel <riel@conectiva.com.br>
 * Released under the General Public License (GPL).
 *
 * Simple, low overhead reverse mapping scheme.
 * Please try to keep this thing as modular as possible.
 *
 * Provides methods for unmapping each kind of mapped page:
 * the anon methods track anonymous pages, and
 * the file methods track pages belonging to an inode.
 *
 * Original design by Rik van Riel <riel@conectiva.com.br> 2001
 * File methods by Dave McCracken <dmccr@us.ibm.com> 2003, 2004
 * Anonymous methods by Andrea Arcangeli <andrea@suse.de> 2004
 * Contributions by Hugh Dickins <hugh@veritas.com> 2003, 2004
 */

/*
 * Lock ordering in mm:
 *
 * inode->i_sem (while writing or truncating, not reading or faulting)
 *   inode->i_alloc_sem
 *
 * When a page fault occurs in writing from user to file, down_read
 * of mmap_sem nests within i_sem; in sys_msync, i_sem nests within
 * down_read of mmap_sem; i_sem and down_write of mmap_sem are never
 * taken together; in truncation, i_sem is taken outermost.
 *
 * mm->mmap_sem
 *   page->flags PG_locked (lock_page)
 *     mapping->i_mmap_lock
 *       anon_vma->lock
 *         mm->page_table_lock
 *           zone->lru_lock (in mark_page_accessed)
 *           swap_list_lock (in swap_free etc's swap_info_get)
 *             mmlist_lock (in mmput, drain_mmlist and others)
 *             swap_device_lock (in swap_duplicate, swap_info_get)
 *             mapping->private_lock (in __set_page_dirty_buffers)
 *             inode_lock (in set_page_dirty's __mark_inode_dirty)
 *               sb_lock (within inode_lock in fs/fs-writeback.c)
 *               mapping->tree_lock (widely used, in set_page_dirty,
 *                         in arch-dependent flush_dcache_mmap_lock,
 *                         within inode_lock in __sync_single_inode)
 */

#include <linux/kernel.h>
#include <linux/mm.h>
#include "biscuitos/kernel.h"
#include "biscuitos/init.h"
#include "biscuitos/mm.h"
#include "biscuitos/swap.h"
#include "biscuitos/rmap.h"
#include "biscuitos/pagemap.h"
#include "asm-generated/pgtable.h"
#include "asm-generated/tlbflush.h"

/*
 * Getting a lock on a stable anon_vma from a page off the LRU is
 * tricky: page_lock_anon_vma rely on RCU to guard against the races.
 */
static struct anon_vma_bs *page_lock_anon_vma_bs(struct page_bs *page)
{
	struct anon_vma_bs *anon_vma = NULL;
	unsigned long anon_mapping;

	rcu_read_lock();
	anon_mapping = (unsigned long) page->mapping;
	if (!(anon_mapping & PAGE_MAPPING_ANON_BS))
		goto out;
	if (!page_mapped_bs(page))
		goto out;

	anon_vma = (struct anon_vma_bs *)(anon_mapping - PAGE_MAPPING_ANON_BS);
	spin_lock(&anon_vma->lock);
out:
	rcu_read_unlock();
	return anon_vma;
}

/*
 * At what user virtual address is page expected in vma?
 */
static inline unsigned long
vma_address_bs(struct page_bs *page, struct vm_area_struct *vma)
{
	pgoff_t pgoff = page->index << (PAGE_CACHE_SHIFT_BS - PAGE_SHIFT_BS);
	unsigned long address;

	address = vma->vm_start + ((pgoff - vma->vm_pgoff) << PAGE_SHIFT_BS);
	if (unlikely(address < vma->vm_start || address >= vma->vm_end)) {
		/* page should be within any vma from prio_tree_next */
		BUG_ON_BS(!PageAnon_bs(page));
		return -EFAULT;
	}
	return address;
}

/*
 * Check that @page is mapped at @address into @mm.
 *
 * On success returns with mapped pte and locked mm->page_table_lock.
 */
static pte_t_bs *page_check_address_bs(struct page_bs *page, 
			struct mm_struct *mm, unsigned long address)
{
	pgd_t_bs *pgd;
	pud_t_bs *pud;
	pmd_t_bs *pmd;
	pte_t_bs *pte;

	/*
	 * We need the page_table_lock to protect us from page faults,
	 * munmap, fork, etc...
	 */
	spin_lock(&mm->page_table_lock);
	pgd = pgd_offset_bs((struct mm_struct_bs *)mm, address);
	if (likely(pgd_present_bs(*pgd))) {
		pud = pud_offset_bs(pgd, address);
		if (likely(pud_present_bs(*pud))) {
			pmd = pmd_offset_bs(pud, address);
			if (likely(pmd_present_bs(*pmd))) {
				pte = pte_offset_map_bs(pmd, address);
				if (likely(pte_present_bs(*pte) &&
					page_to_pfn_bs(page) == 
							pte_pfn_bs(*pte)))
					return pte;
				pte_unmap_bs(pte);
			}
		}
	}
	spin_unlock(&mm->page_table_lock);
	return ERR_PTR(-ENOENT);
}

/*
 * Subfunctions of page_referenced: page_referenced_one called
 * repeatedly from either page_referenced_anon or page_referenced_file.
 */
static int page_referenced_one_bs(struct page_bs *page,
		struct vm_area_struct *vma, unsigned int *mapcount,
		int ignore_token)
{
	struct mm_struct *mm = vma->vm_mm;
	unsigned long address;
	pte_t_bs *pte;
	int referenced = 0;

	//if (!get_mm_counter(mm, rss))
	//	goto out;

	address = vma_address_bs(page, vma);
	if (address == -EFAULT)
		goto out;

	pte = page_check_address_bs(page, mm, address);
	if (!IS_ERR(pte)) {
		if (ptep_clear_flush_young_bs(vma, address, pte))
			referenced++;

		if (mm != current->mm && !ignore_token &&
						has_swap_token_bs(mm))
			referenced++;

		(*mapcount)--;
		pte_unmap_bs(pte);
		spin_unlock(&mm->page_table_lock);
	}

out:
	return referenced;
}

static int page_referenced_anon_bs(struct page_bs *page, int ignore_token)
{
	unsigned int mapcount;
	struct anon_vma_bs *anon_vma;
	struct vm_area_struct *vma;
	int referenced = 0;

	anon_vma = page_lock_anon_vma_bs(page);
	if (!anon_vma)
		return referenced;

	mapcount = page_mapcount_bs(page);
	list_for_each_entry(vma, &anon_vma->head, anon_vma_chain) {
		referenced += page_referenced_one_bs(page, vma, &mapcount,
							ignore_token);
		if (!mapcount)
			break;
	}
	spin_unlock(&anon_vma->lock);
	return referenced;
}

/**
 * page_referenced_file - referenced check for object-based rmap
 * @page: the page we're checking references on.
 *
 * For an object-based mapped page, find all the places it is mapped and
 * check/clear the referenced flag.  This is done by following the page->mapping
 * pointer, then walking the chain of vmas it holds.  It returns the number
 * of references it found.
 *
 * This function is only called from page_referenced for object-based pages.
 */
static int page_referenced_file_bs(struct page_bs *page, int ignore_token)
{
	BS_DUP();
	return 0;
}

/**
 * page_referenced - test if the page was referenced
 * @page: the page to test
 * @is_locked: caller holds lock on the page
 *              
 * Quick test_and_clear_referenced for all mappings to a page,
 * returns the number of ptes which referenced the page.
 */
int page_referenced_bs(struct page_bs *page, int is_locked, int ignore_token)
{
	int referenced = 0;

	if (!swap_token_default_timeout_bs)
		ignore_token = 1;

	if (page_test_and_clear_young_bs(page))
		referenced++;

	if (TestClearPageReferenced_bs(page))
		referenced++;

	if (page_mapped_bs(page) && page->mapping) {
		if (PageAnon_bs(page))
			referenced += 
				page_referenced_anon_bs(page, ignore_token);
		else if (is_locked)
			referenced +=
				page_referenced_file_bs(page, ignore_token);
		else if (TestSetPageLocked_bs(page))
			referenced++;
		else {
			BS_DUP();
		}
	}
	return referenced;
}

























