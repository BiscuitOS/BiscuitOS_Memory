/*
 *  linux/mm/swap.c
 *
 *  Copyright (C) 1991, 1992, 1993, 1994  Linus Torvalds
 */

/*
 * This file contains the default values for the opereation of the
 * Linux VM subsystem. Fine-tuning documentation can be found in
 * Documentation/sysctl/vm.txt.
 * Started 18.12.91
 * Swap aging added 23.2.95, Stephen Tweedie.
 * Buffermem limits added 12.3.98, Rik van Riel.
 */
#include <linux/kernel.h>
#include <linux/cpu.h>
#include <linux/notifier.h>
#include "biscuitos/kernel.h"
#include "biscuitos/mm.h"
#include "biscuitos/percpu.h"
#include "biscuitos/pagevec.h"
#include "biscuitos/mm_inline.h"
#include "asm-generated/percpu.h"

/* How many pages do we try to swap or page in/out together? */
int page_cluster_bs;

/**
 * lru_cache_add: add a page to the page lists
 * @page: the page to add
 */
static DEFINE_PER_CPU_BS(struct pagevec_bs, lru_add_pvecs_bs) = { 0, };
static DEFINE_PER_CPU_BS(struct pagevec_bs, lru_add_active_pvecs_bs) = { 0, };

/*
 * Batched page_cache_release().  Decrement the reference count on all the
 * passed pages.  If it fell to zero then remove the page from the LRU and
 * free it.
 *
 * Avoid taking zone->lru_lock if possible, but if it is taken, retain it
 * for the remainder of the operation.
 *
 * The locking in this function is against shrink_cache(): we recheck the
 * page count inside the lock to see whether shrink_cache grabbed the page
 * via the LRU.  If it did, give up: shrink_cache will free it.
 */
void release_pages_bs(struct page_bs **pages, int nr, int cold)
{
	int i;
	struct pagevec_bs pages_to_free;
	struct zone_bs *zone = NULL;

	pagevec_init_bs(&pages_to_free, cold);
	for (i = 0; i < nr; i++) {
		struct page_bs *page = pages[i];
		struct zone_bs *pagezone;

		if (PageReserved_bs(page) || !put_page_testzero_bs(page))
			continue;

		pagezone = page_zone_bs(page);
		if (pagezone != zone) {
			if (zone)
				spin_unlock_irq(&zone->lru_lock);
			zone = pagezone;
			spin_lock_irq(&zone->lru_lock);
		}
		if (TestClearPageLRU_bs(page))
			del_page_from_lru_bs(zone, page);
		if (page_count_bs(page) == 0) {
			if (!pagevec_add_bs(&pages_to_free, page)) {
				spin_unlock_irq(&zone->lru_lock);
				__pagevec_free_bs(&pages_to_free);
				pagevec_reinit_bs(&pages_to_free);
				zone = NULL;	/* No lock is held */
			}
		}
	}
	if (zone)
		spin_unlock_irq(&zone->lru_lock);

	pagevec_free_bs(&pages_to_free);
}

void __pagevec_lru_add_active_bs(struct pagevec_bs *pvec)
{
	int i;
	struct zone_bs *zone = NULL;

	for (i = 0; i < pagevec_count_bs(pvec); i++) {
		struct page_bs *page = pvec->pages[i];
		struct zone_bs *pagezone = page_zone_bs(page);

		if (pagezone != zone) {
			if (zone)
				spin_unlock_irq(&zone->lru_lock);
			zone = pagezone;
			spin_lock_irq(&zone->lru_lock);
		}
		if (TestSetPageLRU_bs(page))
			BUG_BS();
		if (TestSetPageActive_bs(page))
			BUG_BS();
		add_page_to_active_list_bs(zone, page);
	}
	if (zone)
		spin_unlock_irq(&zone->lru_lock);
	release_pages_bs(pvec->pages, pvec->nr, pvec->cold);
	pagevec_reinit_bs(pvec);
}

/*
 * Add the passed pages to the LRU, then drop the caller's refcount
 * on them.  Reinitialises the caller's pagevec.
 */
void __pagevec_lru_add_bs(struct pagevec_bs *pvec)
{
	int i;
	struct zone_bs *zone = NULL;

	for (i = 0; i < pagevec_count_bs(pvec); i++) {
		struct page_bs *page = pvec->pages[i];
		struct zone_bs *pagezone = page_zone_bs(page);

		if (pagezone != zone) {
			if (zone)
				spin_unlock_irq(&zone->lru_lock);
			zone = pagezone;
			spin_lock_irq(&zone->lru_lock);
		}
		if (TestSetPageLRU_bs(page))
			BUG_BS();
		add_page_to_inactive_list_bs(zone, page);
	}
	if (zone)
		spin_unlock_irq(&zone->lru_lock);
	release_pages_bs(pvec->pages, pvec->nr, pvec->cold);
	pagevec_reinit_bs(pvec);
}
EXPORT_SYMBOL_GPL(__pagevec_lru_add_bs);

void lru_add_drain_bs(void)
{
	struct pagevec_bs *pvec = &get_cpu_var_bs(lru_add_pvecs_bs);

	if (pagevec_count_bs(pvec))
		__pagevec_lru_add_bs(pvec);
	pvec = &__get_cpu_var_bs(lru_add_active_pvecs_bs);
	if (pagevec_count_bs(pvec))
		__pagevec_lru_add_active_bs(pvec);
	put_cpu_var_bs(lru_add_pvecs_bs);
}

/* Drop the CPU's cached committed space back into the central pool */
static int __unused cpu_swap_callback_bs(struct notifier_block *nfb,
			unsigned long action, void *hcpu)
{
	return 0;
}

/*
 * Perform any setup for the swap system
 */
void __init swap_setup_bs(void)
{
	unsigned long megs = num_physpages_bs >> (20 - PAGE_SHIFT_BS);

	/* Use a smaller cluster for small-memory machines */
	if (megs < 16)
		page_cluster_bs = 2;
	else
		page_cluster_bs = 3;

	/*
	 * Right now other parts of the system means that we
	 * _really_ don't want to cluster much more.
	 */
	//hotcpu_notifier(cpu_swap_callback_bs, 0);
}
