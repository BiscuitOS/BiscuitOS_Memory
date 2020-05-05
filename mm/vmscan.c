/*
 *  linux/mm/vmscan.c
 *
 *  Copyright (C) 1991, 1992, 1993, 1994  Linus Torvalds
 *
 *  Swap reorganised 29.12.95, Stephen Tweedie.
 *  kswapd added: 7.1.96  sct
 *  Removed kswapd_ctl limits, and swap out as many pages as needed
 *  to bring the system back to freepages.high: 2.4.97, Rik van Riel.
 *  Zone aware kswapd started 02/00, Kanoj Sarcar (kanoj@sgi.com).
 *  Multiqueue VM started 5.8.00, Rik van Riel.
 */

#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/kthread.h>
#include <linux/swap.h>
#include "biscuitos/kernel.h"
#include "biscuitos/init.h"
#include "biscuitos/mmzone.h"
#include "biscuitos/swap.h"
#include "biscuitos/sched.h"
#include "biscuitos/cpuset.h"
#include "biscuitos/gfp.h"
#include "biscuitos/page-flags.h"
#include "biscuitos/mm.h"
#include "biscuitos/pagevec.h"
#include "biscuitos/rmap.h"

struct scan_control_bs {
	/* Ask refill_inactive_zone, or shrink_cache to scan this many pages */
	unsigned long nr_to_scan;

	/* Incremented by the number of inactive pages that were scanned */
	unsigned long nr_scanned;

	/* Incremented by the number of pages reclaimed */
	unsigned long nr_reclaimed;

	unsigned long nr_mapped;        /* From page_state */

	/* How many pages shrink_cache() should reclaim */
	int nr_to_reclaim;

	/* Ask shrink_caches, or shrink_zone to scan at this priority */
	unsigned int priority;

	/* This context's GFP mask */
	unsigned int gfp_mask;

	int may_writepage;

	/* Can pages be swapped as part of reclaim? */
	int may_swap;

	/* This context's SWAP_CLUSTER_MAX. If freeing memory for
	 * suspend, we effectively ignore SWAP_CLUSTER_MAX.
	 * In this context, it doesn't matter that we scan the
	 * whole list at once. */
	int swap_cluster_max;
};

#define lru_to_page_bs(_head)	(list_entry((_head)->prev, struct page_bs, lru))

#ifdef ARCH_HAS_PREFETCHW
#define prefetchw_prev_lru_page_bs(_page, _base, _field)		\
	do {								\
		if ((_page)->lru.prev != _base) {			\
			struct page_bs *prev;				\
									\
			prev = lru_to_page_bs(&(_page->lru));		\
			prefetchw(&prev->_field);			\
		}							\
	} while (0)
#else
#define prefetchw_prev_lru_page_bs(_page, _base, _field) do { } while (0)
#endif

/*
 * From 0 .. 100.  Higher means more swappy.
 */
int vm_swappiness_bs = 60;
static long total_memory_bs;

/*
 * zone->lru_lock is heavily contended.  Some of the functions that
 * shrink the lists perform better by taking out a batch of pages
 * and working on them outside the LRU lock.
 *
 * For pagecache intensive workloads, this function is the hottest
 * spot in the kernel (apart from copy_*_user functions).
 *
 * Appropriate locks must be held before calling this function.
 *
 * @nr_to_scan: The number of pages to look through on the list.
 * @src:        The LRU list to pull pages off.
 * @dst:        The temp list to put pages on to.
 * @scanned:    The number of pages that were scanned.
 *
 * returns how many pages were moved onto *@dst.
 */
static int isolate_lru_pages_bs(int nr_to_scan, struct list_head *src,
				struct list_head *dst, int *scanned)
{
	int nr_taken = 0;
	struct page_bs *page;
	int scan = 0;

	while (scan++ < nr_to_scan && !list_empty(src)) {
		BS_DUP();
		page = lru_to_page_bs(src);
		prefetchw_prev_lru_page_bs(page, src, flags);

		if (!TestClearPageLRU_bs(page))
			BUG_BS();
		list_del(&page->lru);
		if (get_page_testone_bs(page)) {
			/*
			 * It is being freed elsewhere
			 */
			__put_page_bs(page);
			SetPageLRU_bs(page);
			list_add(&page->lru, src);
			continue;
		} else {
			list_add(&page->lru, dst);
			nr_taken++;
		}
	}

	*scanned = scan;
	return nr_taken;
}

/*
 * This moves pages from the active list to the inactive list.
 *
 * We move them the other way if the page is referenced by one or more
 * processes, from rmap.
 *
 * If the pages are mostly unmapped, the processing is fast and it is
 * appropriate to hold zone->lru_lock across the whole operation.  But if
 * the pages are mapped, the processing is slow (page_referenced()) so we
 * should drop zone->lru_lock around each page.  It's impossible to balance
 * this, so instead we remove the pages from the LRU while processing them.
 * It is safe to rely on PG_active against the non-LRU pages in here because
 * nobody will play with that bit on a non-LRU page.
 *
 * The downside is that we have to touch page->_count against each page.
 * But we had to alter page->flags anyway.
 */
static void
refill_inactive_zone_bs(struct zone_bs *zone, struct scan_control_bs *sc)
{
	int pgmoved;
	int pgscanned;
	int nr_pages = sc->nr_to_scan;
	LIST_HEAD(l_hold);	/* The pages which were snipped off */
	LIST_HEAD(l_inactive);	/* Pages to go onto the inactive_list */
	LIST_HEAD(l_active);	/* Pages to go onto the active_list */
	struct page_bs *page;
	int reclaim_mapped = 0;
	long mapped_ration;
	long distress;
	long swap_tendency;

	lru_add_drain_bs();
	spin_lock_irq(&zone->lru_lock);
	pgmoved = isolate_lru_pages_bs(nr_pages, &zone->active_list,
				&l_hold, &pgscanned);
	zone->pages_scanned += pgscanned;
	zone->nr_active -= pgmoved;
	spin_unlock_irq(&zone->lru_lock);

	/*
	 * `distress' is a measure of how much trouble we're having reclaiming
	 * pages.  0 -> no problems.  100 -> great trouble.
	 */
	distress = 100 >> zone->prev_priority;

	/* 
	 * The point of this algorithm is to decide when to start reclaiming
	 * mapped memory instead of just pagecache.  Work out how much memory
	 * is mapped.
	 */
	mapped_ration = (sc->nr_mapped * 100) / total_memory_bs;

	/*
	 * Now decide how much we really want to unmap some pages.  The mapped
	 * ratio is downgraded - just because there's a lot of mapped memory
	 * doesn't necessarily mean that page reclaim isn't succeeding.
	 *
	 * The distress ratio is important - we don't want to start going oom.
	 *
	 * A 100% value of vm_swappiness overrides this algorithm altogether.
	 */
	swap_tendency = mapped_ration / 2 + distress + vm_swappiness_bs;

        /*
         * Now use this metric to decide whether to start moving mapped memory
         * onto the inactive list.
         */
	if (swap_tendency >= 100)
		reclaim_mapped = 1;

	BS_DUP();
	while (!list_empty(&l_hold)) {
		cond_resched();
		page = lru_to_page_bs(&l_hold);
		list_del(&page->lru);
		if (page_mapped_bs(page)) {
			if (!reclaim_mapped ||
			   (total_swap_pages_bs == 0 && PageAnon_bs(page)) ||
			    page_referenced_bs(page, 0, sc->priority <= 0)) {
				list_add(&page->lru, &l_active);
				continue;
			}
		}
	}
}

/*
 * This is a basic per-zone page freer.  Used by both kswapd and direct reclaim.
 */
static void
shrink_zone_bs(struct zone_bs *zone, struct scan_control_bs *sc)
{
	unsigned long nr_active;
	unsigned long nr_inactive;

        /*
         * Add one to `nr_to_scan' just to make sure that the kernel will
         * slowly sift through the active list.
         */
	zone->nr_scan_active += (zone->nr_active >> sc->priority) + 1;
	nr_active = zone->nr_scan_active;
	if (nr_active >= sc->swap_cluster_max)
		zone->nr_scan_active = 0;
	else
		nr_active = 0;

	zone->nr_scan_inactive += (zone->nr_inactive >> sc->priority) + 1;
	nr_inactive = zone->nr_scan_inactive;
	if (nr_inactive >= sc->swap_cluster_max)
		zone->nr_scan_inactive = 0;
	else
		nr_inactive = 0;

	sc->nr_to_reclaim = sc->swap_cluster_max;

	while (nr_active || nr_inactive) {
		if (nr_active) {
			sc->nr_to_scan = min(nr_active,
					(unsigned long)sc->swap_cluster_max);
			nr_active -= sc->nr_to_scan;
			refill_inactive_zone_bs(zone, sc);
		}
	}
}

/*
 * This is the direct reclaim path, for page-allocating processes.  We only
 * try to reclaim pages from zones which will satisfy the caller's allocation
 * request.
 *
 * We reclaim from a zone even if that zone is over pages_high.  Because:
 * a) The caller may be trying to free *extra* pages to satisfy a higher-order
 *    allocation or
 * b) The zones may be over pages_high but they must go *over* pages_high to
 *    satisfy the `incremental min' zone defense algorithm.
 *
 * Returns the number of reclaimed pages.
 *
 * If a zone is deemed to be full of pinned pages then just give it a light
 * scan then give up on it.
 */
static void
shrink_caches_bs(struct zone_bs **zones, struct scan_control_bs *sc)
{
	int i;

	for (i = 0; zones[i] != NULL; i++) {
		struct zone_bs *zone = zones[i];

		if (zone->present_pages == 0)
			continue;

		if (!cpuset_zone_allowed_bs(zone, __GFP_HARDWALL_BS))
			continue;

		zone->temp_priority = sc->priority;
		if (zone->prev_priority > sc->priority)
			zone->prev_priority = sc->priority;

		if (zone->all_unreclaimable && sc->priority != DEF_PRIORITY_BS)
			continue; /* Let kswapd poll it */

		shrink_zone_bs(zone, sc);
	}
}

/*
 * This is the main entry point to direct page reclaim.
 *
 * If a full scan of the inactive list fails to free enough memory then we
 * are "out of memory" and something needs to be killed.
 *
 * If the caller is !__GFP_FS then the probability of a failure is reasonably
 * high - the zone may be full of dirty or under-writeback pages, which this
 * caller can't do much about.  We kick pdflush and take explicit naps in the
 * hope that some of these pages can be written.  But if the allocating task
 * holds filesystem locks which prevent writeout this might not work, and the
 * allocation attempt will fail.
 */
int try_to_free_pages_bs(struct zone_bs **zones, unsigned int gfp_mask)
{
	int priority;
	struct scan_control_bs sc;
	unsigned long lru_pages = 0;
	int i;

	sc.gfp_mask = gfp_mask;
	sc.may_writepage = 0;
	sc.may_swap = 1;

	inc_page_state_bs(allocstall);	

	for (i = 0; zones[i] != NULL; i++) {
		struct zone_bs *zone = zones[i];

		if (!cpuset_zone_allowed_bs(zone, __GFP_HARDWALL_BS))
			continue;

		zone->temp_priority = DEF_PRIORITY_BS;
		lru_pages += zone->nr_active + zone->nr_inactive;
	}

	for (priority = DEF_PRIORITY_BS; priority >= 0; priority--) {
		sc.nr_mapped = read_page_state_bs(nr_mapped);
		sc.nr_scanned = 0;
		sc.nr_reclaimed = 0;
		sc.priority = priority;
		sc.swap_cluster_max = SWAP_CLUSTER_MAX_BS;
		shrink_caches_bs(zones, &sc);
		BS_DUP();
	}
	return 0;
}

/*
 * For kswapd, balance_pgdat() will work across all this node's zones until
 * they are all at pages_high.
 *
 * If `nr_pages' is non-zero then it is the number of pages which are to be
 * reclaimed, regardless of the zone occupancies.  This is a software suspend
 * special.
 *
 * Returns the number of pages which were actually freed.
 *
 * There is special handling here for zones which are full of pinned pages.
 * This can happen if the pages are all mlocked, or if they are all used by
 * device drivers (say, ZONE_DMA).  Or if they are all in use by hugetlb.
 * What we do is to detect the case where all pages in the zone have been
 * scanned twice and there has been zero successful reclaim.  Mark the zone as
 * dead and from now on, only perform a short scan.  Basically we're polling
 * the zone for when the problem goes away.
 *
 * kswapd scans the zones in the highmem->normal->dma direction.  It skips
 * zones which have free_pages > pages_high, but once a zone is found to have
 * free_pages <= pages_high, we scan that zone and the lower zones regardless
 * of the number of free pages in the lower zones.  This interoperates with
 * the page allocator fallback scheme to ensure that aging of pages is balanced
 * across the zones.
 */
static int balance_pgdat_bs(pg_data_t_bs *pgdat, int nr_pages, int order)
{
	int all_zones_ok;
	int priority;
	int i;
	int total_scanned, total_reclaimed;
	struct scan_control_bs sc;

	total_scanned = 0;
	total_reclaimed = 0;
	sc.gfp_mask = GFP_KERNEL_BS;
	sc.may_writepage = 0;
	sc.nr_mapped = read_page_state_bs(nr_mapped);

	inc_page_state_bs(pageoutrun);

	for (i = 0; i < pgdat->nr_zones; i++) {
		struct zone_bs *zone = pgdat->node_zones + i;

		zone->temp_priority = DEF_PRIORITY_BS;
	}

	for (priority = DEF_PRIORITY_BS; priority >= 0; priority--) {
		int end_zone = 0;
		unsigned long lru_pages = 0;

		all_zones_ok = 1;

		if (nr_pages == 0) {
			/*
			 * Scan in the highmem->dma direction for the highest
			 * zone which needs scanning
			 */
			for (i = pgdat->nr_zones - 1; i >= 0; i--) {
				struct zone_bs *zone = pgdat->node_zones + i;

				if (zone->present_pages == 0)
					continue;

				if (zone->all_unreclaimable &&
						priority != DEF_PRIORITY_BS)
					continue;

				if (!zone_watermark_ok_bs(zone, order,
						zone->pages_high, 0, 0, 0)) {
					end_zone = i;
					goto scan;
				}
			}
			goto out;
		} else {
			end_zone = pgdat->nr_zones - 1;
		}
scan:
		for (i = 0; i <= end_zone; i++) {
			struct zone_bs *zone = pgdat->node_zones + i;

			lru_pages += zone->nr_active + zone->nr_inactive;
		}

		/*
		 * Now scan the zone in the dma->highmem direction, stopping
		 * at the last zone which needs scanning.
		 *
		 * We do this because the page allocator works in the opposite
		 * direction.  This prevents the page allocator from allocating
		 * pages behind kswapd's direction of progress, which would
		 * cause too much scanning of the lower zones.
		 */
		for (i = 0; i <= end_zone; i++) {
			struct zone_bs *zone = pgdat->node_zones + i;

			if (zone->present_pages == 0)
				continue;

			if (zone->all_unreclaimable && priority != 
							DEF_PRIORITY_BS)
				continue;

			if (nr_pages == 0) { /* Not software suspend */
				if (!zone_watermark_ok_bs(zone, order,
					zone->pages_high, end_zone, 0, 0))
					all_zones_ok = 0;
			}
			zone->temp_priority = priority;
			if (zone->prev_priority > priority)
				zone->prev_priority = priority;
			sc.nr_scanned = 0;
			sc.nr_reclaimed = 0;
			sc.priority = priority;
			sc.swap_cluster_max = nr_pages ? nr_pages :
							SWAP_CLUSTER_MAX_BS;
			shrink_zone_bs(zone, &sc);
		}
	} 
out:
	return 0;
}

/*
 * The background pageout daemon, started as a kernel thread
 * from the init process. 
 *
 * This basically trickles out pages so that we have _some_
 * free memory available even if there is no other activity
 * that frees anything up. This is needed for things like routing
 * etc, where we otherwise might have all activity going on in
 * asynchronous contexts that cannot page things out.
 *
 * If there are applications that are active memory-allocators
 * (most normal use), this basically shouldn't matter.
 */
static int kswapd_bs(void *p)
{
	unsigned long order;
	pg_data_t_bs *pgdat = (pg_data_t_bs *)p;
	struct task_struct *tsk = current;
	DEFINE_WAIT(wait);
	struct reclaim_state reclaim_state = {
		.reclaimed_slab = 0,
	};
	current->reclaim_state = &reclaim_state;

	/*
	 * Tell the memory management that we're a "memory allocator",
	 * and that if we need more memory we should get access to it
	 * regardless (see "__alloc_pages()"). "kswapd" should
	 * never get caught in the normal page freeing logic.
	 *
	 * (Kswapd normally doesn't need memory anyway, but sometimes
	 * you need a small amount of memory in order to be able to
	 * page out something else, and this flag essentially protects
	 * us from recursively trying to free more memory as we're
	 * trying to free the first piece of memory in the first place).
	 */
	tsk->flags |= PF_MEMALLOC_BS | PF_KSWAPD_BS;
	order = 0;
	for ( ; ; ) {
		unsigned long new_order;

		if (current->flags & PF_FREEZE_BS)
			refrigerator_bs(PF_FREEZE_BS);

		prepare_to_wait(&pgdat->kswapd_wait, &wait, TASK_INTERRUPTIBLE);
		new_order = pgdat->kswapd_max_order;
		pgdat->kswapd_max_order = 0;
		if (order < new_order) {
			/*
			 * Don't sleep if someone wants a larger 'order'
			 * allocation
			 */
			order = new_order;
		} else {
			schedule();
			order = pgdat->kswapd_max_order;
		}
		finish_wait(&pgdat->kswapd_wait, &wait);

		balance_pgdat_bs(pgdat, 0, order);
	}

	return 0;
}

/*
 * Try to free up some pages from this zone through reclaim.
 */
int zone_reclaim_bs(struct zone_bs *zone, unsigned int gfp_mask, 
							unsigned int order)
{
	/* The reclaim may sleep, so don't do it if sleep isn't allowed */
	if (!(gfp_mask & __GFP_WAIT_BS))
		return 0;
	if (zone->all_unreclaimable)
		return 0;
	BS_DUP();
	return 0;
}

/*
 * A zone is low on free memory, so wake its kswapd task to service it.
 */
void wakeup_kswapd_bs(struct zone_bs *zone, int order)
{
	pg_data_t_bs *pgdat;

	if (zone->present_pages == 0)
		return;

	pgdat = zone->zone_pgdat;
	if (zone_watermark_ok_bs(zone, order, zone->pages_low, 0, 0, 0))
		return;
	if (pgdat->kswapd_max_order < order)
		pgdat->kswapd_max_order = order;
	if (!cpuset_zone_allowed_bs(zone, __GFP_HARDWALL_BS))
		return;
	if (!waitqueue_active(&zone->zone_pgdat->kswapd_wait))
		return;
	wake_up_interruptible(&zone->zone_pgdat->kswapd_wait);
}
EXPORT_SYMBOL_GPL(wakeup_kswapd_bs);

static int __init kswapd_init_bs(void)
{
	pg_data_t_bs *pgdat;
	swap_setup_bs();
	for_each_pgdat_bs(pgdat)
		pgdat->kswapd = kthread_run(kswapd_bs, pgdat, "kswapd-bs%d", 0);
	total_memory_bs = nr_free_pagecache_pages_bs();
	return 0;
}
module_initcall_bs(kswapd_init_bs);
