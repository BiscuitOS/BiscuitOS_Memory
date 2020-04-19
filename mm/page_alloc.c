/*
 * page_alloc
 *
 * (C) 2020.04.14 BuddyZhang1 <buddy.zhang@aliyun.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/kernel.h>
#include "biscuitos/nodemask.h"
#include "biscuitos/mmzone.h"
#include "biscuitos/bootmem.h"
#include "biscuitos/mm.h"
#include "biscuitos/page-flags.h"

nodemask_t_bs node_online_map_bs = { { [0] = 1UL } };

static bootmem_data_t_bs contig_bootmem_data_bs;
struct pglist_data_bs contig_page_data_bs = { 
	.bdata = &contig_bootmem_data_bs 
};
EXPORT_SYMBOL_GPL(contig_page_data_bs);
struct pglist_data_bs *pgdat_list_bs;

/*
 * Used by page_zone() to look up the address of the struct zone whose
 * id is encoded in the upper bits of page->flags
 */
struct zone_bs *zone_table_bs[1 << (ZONES_SHIFT_BS + NODES_SHIFT_BS)];
EXPORT_SYMBOL_GPL(zone_table_bs);

unsigned long __initdata nr_kernel_pages_bs;
unsigned long __initdata nr_all_pages_bs;

static char *zone_names_bs[MAX_NR_ZONES_BS] = { "DMA", "Normal", "HighMem" };

static void __init calculate_zone_totalpages_bs(struct pglist_data_bs *pgdat,
	unsigned long *zones_size, unsigned long *zholes_size)
{
	unsigned long realtotalpages, totalpages = 0;
	int i;

	for (i = 0; i < MAX_NR_ZONES_BS; i++)
		totalpages += zones_size[i];
	pgdat->node_spanned_pages = totalpages;

	realtotalpages = totalpages;
	if (zholes_size)
		for (i = 0; i < MAX_NR_ZONES_BS; i++)
			realtotalpages -= zholes_size[i];
	pgdat->node_present_pages = realtotalpages;
	printk(KERN_DEBUG "On node %d totalpages: %lu\n", pgdat->node_id,
			realtotalpages);
}

static void __init alloc_node_mem_map_bs(struct pglist_data_bs *pgdat)
{
	unsigned long size;

	/* Skip empty nodes */
	if (!pgdat->node_spanned_pages)
		return;

	/* ia64 gets its own node_mem_map, before this, without bootmem */
	if (!pgdat->node_mem_map) {
		size = (pgdat->node_spanned_pages + 1) * sizeof(struct page_bs);
		pgdat->node_mem_map = alloc_bootmem_node_bs(pgdat, size);
	}
#ifndef CONFIG_DISCONTIGMEM
	/*
	 * With no DISCONTIG, the global mem_map is just set as node 0's
	 */
	if (pgdat == NODE_DATA_BS(0))
		mem_map_bs = NODE_DATA_BS(0)->node_mem_map;
#endif
}

/*
 * Helper functions to size the waitqueue hash table.
 * Essentially these want to choose hash table sizes sufficiently
 * large so that collisions trying to wait on pages are rare.
 * But in fact, the number of active page waitqueues on typical
 * systems is ridiculously low, less than 200. So this is even
 * conservative, even though it seems large.
 *
 * The constant PAGES_PER_WAITQUEUE specifies the ratio of pages to
 * waitqueues, i.e. the size of the waitq table given the number of pages.
 */
#define PAGES_PER_WAITQUEUE_BS		256

static inline unsigned long wait_table_size_bs(unsigned long pages)
{
	unsigned long size = 1;

	pages /= PAGES_PER_WAITQUEUE_BS;

	while (size < pages)
		size <<= 1;

	/*
	 * Once we have dozens or even hundreds of threads sleeping
	 * on IO we've got bigger problems than wait queue collision.
	 * Limit the size of the wait table to a reasonable size.
	 */
	size = min(size, 4096UL);

	return max(size, 4UL);
}

/*
 * This is an integer logarithm so that shifts can be used later
 * to extract the more random high bits from the multiplicative
 * hash function before the remainder is taken.
 */
static inline unsigned long wait_table_bits_bs(unsigned long size)
{
	return ffz(~size);
}

/*
 * Initially all pages are reserved - free ones are freed
 * up by free_all_bootmem() once the early boot process is
 * done. Non-atomic initialization, single-pass.
 */
void __init memmap_init_zone_bs(unsigned long size, int nid, 
			unsigned long zone, unsigned long start_pfn)
{
	struct page_bs *start = pfn_to_page_bs(start_pfn);
	struct page_bs *page;

	for (page = start; page < (start + size); page++) {
		set_page_zone_bs(page, NODEZONE_BS(nid, zone));
		set_page_count_bs(page, 0);
		reset_page_mapcount_bs(page);
		SetPageReserved_bs(page);
		INIT_LIST_HEAD(&page->lru);
		start_pfn++;
#ifdef WAIT_PAGE_VIRTUAL
		BUG();
#endif
	}
}

#ifndef __HAVE_ARCH_MEMMAP_INIT
#define memmap_init_bs(size, nid, zone, start_pfn) \
	memmap_init_zone_bs((size), (nid), (zone), (start_pfn))
#endif

void zone_init_free_lists_bs(struct pglist_data_bs *pgdat, 
			struct zone_bs *zone, unsigned long size)
{
	int order;

	for (order = 0; order < MAX_ORDER_BS; order++) {
		INIT_LIST_HEAD(&zone->free_area[order].free_list);
		zone->free_area[order].nr_free = 0;
	}
}

/*
 * Set up the zone data structures:
 *   - mark all pages reserved
 *   - mark all memory queues empty
 *   - clear the memory bitmap
 */
static void __init free_area_init_core_bs(struct pglist_data_bs *pgdat,
		unsigned long *zones_size, unsigned long *zholes_size)
{
	unsigned long i, j;
	const unsigned long zone_required_alignment = 1 << (MAX_ORDER_BS-1);
	int cpu, nid = pgdat->node_id;
	unsigned long zone_start_pfn = pgdat->node_start_pfn;

	pgdat->nr_zones = 0;
	init_waitqueue_head(&pgdat->kswapd_wait);
	pgdat->kswapd_max_order = 0;

	for (j = 0; j < MAX_NR_ZONES_BS; j++) {
		struct zone_bs *zone = pgdat->node_zones + j;
		unsigned long size, realsize;
		unsigned long batch;

		zone_table_bs[NODEZONE_BS(nid, j)] = zone;
		realsize = size = zones_size[j];
		if (zholes_size)
			realsize -= zholes_size[j];

		if (j == ZONE_DMA_BS || j == ZONE_NORMAL_BS)
			nr_kernel_pages_bs += realsize;
		nr_all_pages_bs += realsize;

		zone->spanned_pages = size;
		zone->present_pages = realsize;
		zone->name = zone_names_bs[j];
		spin_lock_init(&zone->lock);
		spin_lock_init(&zone->lru_lock);
		zone->zone_pgdat = pgdat;
		zone->free_pages = 0;

		zone->temp_priority = zone->prev_priority = DEF_PRIORITY_BS;

		/*
		 * The per-cpu-pages pools are set to around 1000th of the
		 * size of the zone. But no more than 1/4 of a meg - there's
		 * no point in going beyond the size of L2 cache.
		 *
		 * OK, so we don't know big the cache is. So guess.
		 */
		batch = zone->present_pages / 1024;
		if (batch * PAGE_SIZE > 256 * 1024)
			batch = (256 * 1024) / PAGE_SIZE;
		batch /= 4;	/* We effectively *= 4 below */
		if (batch < 1)
			batch = 1;

		/*
		 * Clamp the batch to a 2^n - 1value. Having a power
		 * of 2 value was found to be more likely to have
		 * suboptimal cache aliasing properties in some cases.
		 *
		 * For example if 2 tasks are alternately allocating
		 * batches of apges, one task can end up with a lot
		 * of pages of one half of the possible page colors
		 * and the other with pages of the other colors.
		 */
		batch = (1 << fls(batch + batch/2)) - 1;

		for (cpu = 0; cpu < NR_CPUS_BS; cpu++) {
			struct per_cpu_pages_bs *pcp;

			pcp = &zone->pageset[cpu].pcp[0];	/* hot */
			pcp->count = 0;
			pcp->low = 2 * batch;
			pcp->high = 6 * batch;
			pcp->batch = 1 * batch;
			INIT_LIST_HEAD(&pcp->list);

			pcp = &zone->pageset[cpu].pcp[1];	/* cold */
			pcp->count = 0;
			pcp->low = 0;
			pcp->high = 2 * batch;
			pcp->batch = 1 * batch;
			INIT_LIST_HEAD(&pcp->list);
		}
		printk(KERN_DEBUG "  %s zone: %lu pages, LIFO batch:%lu\n",
				zone_names_bs[j], realsize, batch);
		INIT_LIST_HEAD(&zone->active_list);
		INIT_LIST_HEAD(&zone->inactive_list);
		zone->nr_scan_active = 0;
		zone->nr_scan_inactive = 0;
		zone->nr_inactive = 0;
		if (!size)
			continue;

		/*
		 * The per-cpage waitqueu mechanism uses hashed waitqueues
		 * per zone.
		 */
		zone->wait_table_size = wait_table_size_bs(size);
		zone->wait_table_bits =
			wait_table_bits_bs(zone->wait_table_size);
		zone->wait_table = (wait_queue_head_t *)
			alloc_bootmem_node_bs(pgdat, zone->wait_table_size * 
					sizeof(wait_queue_head_t));

		for (i = 0; i < zone->wait_table_size; ++i)
			init_waitqueue_head(zone->wait_table + i);

		pgdat->nr_zones = j+1;

		zone->zone_mem_map = pfn_to_page_bs(zone_start_pfn);
		zone->zone_start_pfn = zone_start_pfn;

		if ((zone_start_pfn) & (zone_required_alignment-1))
			printk(KERN_CRIT "BUG: wrong zone alignment, it will "
					"crash\n");
		memmap_init_bs(size, nid, j, zone_start_pfn);

		zone_start_pfn += size;
		zone_init_free_lists_bs(pgdat, zone, zone->spanned_pages);
	}
}

void __init free_area_init_node_bs(int nid, struct pglist_data_bs *pgdat,
		unsigned long *zones_size, unsigned long node_start_pfn,
		unsigned long *zhole_size)
{
	pgdat->node_id = nid;
	pgdat->node_start_pfn = node_start_pfn;
	calculate_zone_totalpages_bs(pgdat, zones_size, zhole_size);

	alloc_node_mem_map_bs(pgdat);

	free_area_init_core_bs(pgdat, zones_size, zhole_size);
}
