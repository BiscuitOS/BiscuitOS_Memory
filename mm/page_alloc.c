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
#include <linux/sched.h>
#include <linux/smp.h>
#include "biscuitos/kernel.h"
#include "biscuitos/nodemask.h"
#include "biscuitos/mmzone.h"
#include "biscuitos/bootmem.h"
#include "biscuitos/mm.h"
#include "biscuitos/swap.h"
#include "biscuitos/cpuset.h"
#include "biscuitos/page-flags.h"
#include "biscuitos/sched.h"
#include "asm-generated/percpu.h"

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

/*
 * Accumulate the page_state information across all CPUs.
 * The result is unavoidably approximate - it can change
 * during and after execution of this function.
 */
static DEFINE_PER_CPU_BS(struct page_state_bs, page_states_bs) = {0};

unsigned long __initdata nr_kernel_pages_bs;
unsigned long __initdata nr_all_pages_bs;
unsigned long totalram_pages_bs;
unsigned long totalhigh_pages_bs;

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
		BS_DUP();
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

/*
 * Builds allocation fallback zone lists.
 */
static int __init build_zonelists_node_bs(pg_data_t_bs *pgdat,
				struct zonelist_bs *zonelist, int j, int k)
{
	switch (k) {
		struct zone_bs *zone;
	default:
		BUG_BS();
	case ZONE_HIGHMEM_BS:
		zone = pgdat->node_zones + ZONE_HIGHMEM_BS;
		if (zone->present_pages) {
#ifndef CONFIG_HIGHMEM_BS
			BUG_BS();
#endif
			zonelist->zones[j++] = zone;
		}
	case ZONE_NORMAL_BS:
		zone = pgdat->node_zones + ZONE_NORMAL_BS;
		if (zone->present_pages)
			zonelist->zones[j++] = zone;
	case ZONE_DMA_BS:
		zone = pgdat->node_zones + ZONE_DMA_BS;
		if (zone->present_pages)
			zonelist->zones[j++] = zone;
	}

	return j;
}

static void __init build_zonelists_bs(pg_data_t_bs *pgdat)
{
	int i, j, k, node, local_node;

	local_node = pgdat->node_id;
	for (i = 0; i < GFP_ZONETYPES_BS; i++) {
		struct zonelist_bs *zonelist;

		zonelist = pgdat->node_zonelists + i;

		j = 0;
		k = ZONE_NORMAL_BS;
		if (i & __GFP_HIGHMEM_BS)
			k = ZONE_HIGHMEM_BS;
		if (i & __GFP_DMA_BS)
			k = ZONE_DMA_BS;

		j = build_zonelists_node_bs(pgdat, zonelist, j, k);
		/*
		 * Now we build the zonelist so that it contains the zones
		 * of all the other nodes.
		 * We don't want to pressure a particular node, so when
		 * building the zones for node N, we make sure that the
		 * zones coming right after the local ones are those from
		 * node N+1 (modulo N)
		 */
		for (node = local_node + 1; node < MAX_NUMNODES_BS; node++) {
			if (!node_online_bs(node))
				continue;

			j = build_zonelists_node_bs(NODE_DATA_BS(node), 
							zonelist, j, k);
		}
		for (node = 0; node < local_node; node++) {
			if (!node_online_bs(node))
				continue;
			j = build_zonelists_node_bs(NODE_DATA_BS(node),
					zonelist, j, k);
		}

		zonelist->zones[j] = NULL;
	}
}

void __init build_all_zonelists_bs(void)
{
	int i;

	for_each_online_node_bs(i)
		build_zonelists_bs(NODE_DATA_BS(i));
	printk("Build %i zonelists\n", num_online_nodes_bs());
}

void set_page_refs_bs(struct page_bs *page, int order)
{
#ifdef CONFIG_MMU
	set_page_count_bs(page, 1);
#else
	int i;                          
                        
	/*              
	 * We need to reference all the pages for this order, otherwise if
	 * anyone accesses one of the pages with (get/put) it will be freed.
	 * - eg: access_process_vm()
	 */     
	for (i = 0; i < (1 << order); i++)
		set_page_count_bs(page + i, 1);
#endif
}

/*
 * Temporary debugging check for pages not lying within a given zone.
 */
static int bad_range_bs(struct zone_bs *zone, struct page_bs *page)
{
	if (page_to_pfn_bs(page) >= 
				zone->zone_start_pfn + zone->spanned_pages)
		return 1;
	if (page_to_pfn_bs(page) < zone->zone_start_pfn)
		return 1;
#ifdef CONFIG_HOLES_IN_ZONE
	if (!pfn_valid_bs(page_to_pfn_bs(page)))
		return 1;
#endif
	if (zone != page_zone_bs(page))
		return 1;
	return 0;
}

void __mod_page_state_bs(unsigned offset, unsigned long delta)
{
	unsigned long flags;
	void *ptr;

	local_irq_save(flags);
	ptr = &__get_cpu_var_bs(page_states_bs);
	*(unsigned long *)(ptr + offset) += delta;
	local_irq_restore(flags);
}
EXPORT_SYMBOL_GPL(__mod_page_state_bs);

static void bad_page_bs(const char *function, struct page_bs *page)
{
	printk(KERN_EMERG "Bad page state at %s (in process '%s', page %p)\n",
			function, current->comm, page);
	printk(KERN_EMERG "flags:0x%0*lx mapping:%p mapcount:%d count:%d\n",
			(int)(2*sizeof(page_flags_t_bs)), 
			(unsigned long)page->flags,
			page->mapping,
			page_mapcount_bs(page),
			page_count_bs(page));
	printk(KERN_EMERG "BiscuitOS Backtrace:\n");
	dump_stack();
	printk(KERN_EMERG "Trying to fix it up, but a reboot is needed\n");
	page->flags &= ~(1 << PG_private_bs		|
			1 << PG_locked_bs		|
			1 << PG_lru_bs			|
			1 << PG_active_bs		|
			1 << PG_dirty_bs		|
			1 << PG_swapcache_bs		|
			1 << PG_writeback_bs);
	set_page_count_bs(page, 0);
	reset_page_mapcount_bs(page);
	page->mapping = NULL;
}

static inline void free_pages_check_bs(const char *function,
						struct page_bs *page)
{
	if (	page_mapcount_bs(page) ||
		page->mapping != NULL ||
		page_count_bs(page) != 0 ||
		(page->flags & (
			1 << PG_lru_bs		|
			1 << PG_private_bs	|
			1 << PG_locked_bs	|
			1 << PG_active_bs	|
			1 << PG_reclaim_bs	|
			1 << PG_slab_bs		|
			1 << PG_swapcache_bs	|
			1 << PG_writeback_bs	)))
		bad_page_bs(function, page);
	if (PageDirty_bs(page))
		ClearPageDirty_bs(page);
}

#ifndef CONFIG_HUGETLB_PAGE_BS
#define prep_compound_page_bs(page, order)	do { } while (0)
#define destroy_compound_page_bs(page, order)	do { } while (0)
#endif

/*
 * function for dealing with page's order in buddy system.
 * zone->lock is already acquired when we use these.
 * So, we don't need atomic page->flags operations here.
 */
static inline unsigned long page_order_bs(struct page_bs *page)
{
	return page->private;
}

static inline void set_page_order_bs(struct page_bs *page, int order)
{
	page->private = order;
	__SetPagePrivate_bs(page);
}

static inline void rmv_page_order_bs(struct page_bs *page)
{
	__ClearPagePrivate_bs(page);
	page->private = 0;
}

/*
 * This function checks whether a page is free && is the buddy
 * we can do coalesce a page and its buddy if
 * (a) the buddy is free &&
 * (b) the buddy is on the buddy system &&
 * (c) a page and its buddy have the same order.
 * for recording page's order, we use page->private and PG_private.
 *
 */
static inline int page_is_buddy_bs(struct page_bs *page, int order)
{
	if (PagePrivate_bs(page)		&&
	   (page_order_bs(page) == order)	&&
	   !PageReserved_bs(page)		&&
	   page_count_bs(page) == 0)
		return 1;
	return 0;
}

/*
 * Locate the struct page for both the matching buddy in our
 * pair (buddy1) and the combined O(n+1) page they form (page).
 *
 * 1) Any buddy B1 will have an order O twin B2 which satisfies
 * the following equation:
 *     B2 = B1 ^ (1 << O)
 * For example, if the starting buddy (buddy2) is #8 its order
 * 1 buddy is #10:
 *     B2 = 8 ^ (1 << 1) = 8 ^ 2 = 10
 *
 * 2) Any buddy B will have an order O+1 parent P which
 * satisfies the following equation:
 *     P = B & ~(1 << O)
 *
 * Assumption: *_mem_map is contigious at least up to MAX_ORDER
 */
static inline struct page_bs *
__page_find_buddy_bs(struct page_bs *page, unsigned long page_idx, 
							unsigned int order)
{
	unsigned long buddy_idx = page_idx ^ (1 << order);

	return page + (buddy_idx - page_idx);
}

static inline unsigned long
__find_combined_index_bs(unsigned long page_idx, unsigned int order)
{
	return (page_idx & ~(1 << order));
}

/*
 * Freeing function for a buddy system allocator.
 *
 * The concept of a buddy system is to maintain direct-mapped table
 * (containing bit values) for memory blocks of various "orders".
 * The bottom level table contains the map for the smallest allocatable
 * units of memory (here, pages), and each level above it describes
 * pairs of units from the levels below, hence, "buddies".
 * At a high level, all that happens here is marking the table entry
 * at the bottom level available, and propagating the changes upward
 * as necessary, plus some accounting needed to play nicely with other
 * parts of the VM system.
 * At each level, we keep a list of pages, which are heads of continuous
 * free pages of length of (1 << order) and marked with PG_Private.Page's
 * order is recorded in page->private field.
 * So when we are allocating or freeing one, we can derive the state of the
 * other.  That is, if we allocate a small block, and both were   
 * free, the remainder of the region must be split into blocks.   
 * If a block is freed, and its buddy is also free, then this
 * triggers coalescing into a block of larger size.            
 *
 * -- wli
 */

static inline void __free_pages_bulk_bs(struct page_bs *page,
		struct zone_bs *zone, unsigned int order)
{
	unsigned long page_idx;
	int order_size = 1 << order;

	if (unlikely(order))
		destroy_compound_page_bs(page, order);

	page_idx = page_to_pfn_bs(page) & ((1 << MAX_ORDER_BS) - 1);

	BUG_ON_BS(page_idx & (order_size - 1));
	BUG_ON_BS(bad_range_bs(zone, page));

	zone->free_pages += order_size;
	while (order < MAX_ORDER_BS-1) {
		unsigned long combined_idx;
		struct free_area_bs *area;
		struct page_bs *buddy;

		combined_idx = __find_combined_index_bs(page_idx, order);
		buddy = __page_find_buddy_bs(page, page_idx, order);

		if (bad_range_bs(zone, buddy))
			break;
		if (!page_is_buddy_bs(buddy, order))
			break;	/* Move the buddy up one level */
		list_del(&buddy->lru);
		area = zone->free_area + order;
		area->nr_free--;
		rmv_page_order_bs(buddy);
		page = page + (combined_idx - page_idx);
		page_idx = combined_idx;
		order++;
	}
	set_page_order_bs(page, order);
	list_add(&page->lru, &zone->free_area[order].free_list);
	zone->free_area[order].nr_free++;
}

/*
 * Frees a list of pages. 
 * Assumes all pages on list are in same zone, and of same order.
 * count is the number of pages to free, or 0 for all on the list.
 *
 * If the zone was previously in an "all pages pinned" state then look to
 * see if this freeing clears that state.
 *
 * And clear the zone's pages_scanned counter, to hold off the "all pages are
 * pinned" detection logic.
 */
static int
free_pages_bulk_bs(struct zone_bs *zone, int count,
                        struct list_head *list, unsigned int order)
{
	unsigned long flags; 
	struct page_bs *page = NULL;
	int ret = 0;

	spin_lock_irqsave(&zone->lock, flags);
	zone->all_unreclaimable = 0;
	zone->pages_scanned = 0; 
	while (!list_empty(list) && count--) {
		page = list_entry(list->prev, struct page_bs, lru);
		/* have to delete it as _free_pages_bulk_bs list manipulates */
		list_del(&page->lru);
		__free_pages_bulk_bs(page, zone, order);
		ret++;
	}
	spin_unlock_irqrestore(&zone->lock, flags);
	return ret;
}

void __free_pages_ok_bs(struct page_bs *page, unsigned int order)
{
	LIST_HEAD(list);
	int i;

	arch_free_page_bs(page, order);

	mod_page_state_bs(pgfree, 1 << order);

	for (i = 0; i < (1 << order); ++i)
		free_pages_check_bs(__FUNCTION__, page + i);
	list_add(&page->lru, &list);
	kernel_map_pages_bs(page, 1 << order, 0);
	free_pages_bulk_bs(page_zone_bs(page), 1, &list, order);
}

/*
 * Free a 0-order page
 */
static void FASTCALL_BS(free_hot_cold_page_bs(struct page_bs *page, int cold));
static void fastcall_bs free_hot_cold_page_bs(struct page_bs *page, int cold)
{
	struct zone_bs *zone = page_zone_bs(page);
	struct per_cpu_pages_bs *pcp;
	unsigned long flags;

	arch_free_page_bs(page, 0);

	kernel_map_pages_bs(page, 1, 0);
	inc_page_state_bs(pgfree);
	if (PageAnon_bs(page))
		page->mapping = NULL;
	free_pages_check_bs(__FUNCTION__, page);
	pcp = &zone->pageset[get_cpu()].pcp[cold];
	local_irq_save(flags);
	if (pcp->count >= pcp->high)
		pcp->count -= free_pages_bulk_bs(zone, pcp->batch, 
							&pcp->list, 0);
	list_add(&page->lru, &pcp->list);
	pcp->count++;
	local_irq_restore(flags);
	put_cpu();
}

void fastcall_bs free_hot_page_bs(struct page_bs *page)
{
	free_hot_cold_page_bs(page, 0);
}

void __free_pages_bs(struct page_bs *page, unsigned int order)
{
	if (!PageReserved_bs(page) && put_page_testzero_bs(page)) {
		if (order == 0)
			free_hot_page_bs(page);
		else
			__free_pages_ok_bs(page, order);
	}
}
EXPORT_SYMBOL_GPL(__free_pages_bs);

/*
 * Total amount of free (allocatable) RAM:
 */
unsigned int nr_free_pages_bs(void)
{
	unsigned int sum = 0;
	struct zone_bs *zone;

	for_each_zone_bs(zone)
		sum += zone->free_pages;

	return sum;
}
EXPORT_SYMBOL_GPL(nr_free_pages_bs);

/*
 * Return 1 if free pages are above 'mark'. This takes into account the order
 * of the allocation.
 */
int zone_watermark_ok_bs(struct zone_bs *z, int order, unsigned long mark,
		int classzone_idx, int can_try_harder, int gfp_high)
{
	/* free_pages my go negative - that's OK */
	long min = mark, free_pages = z->free_pages - (1 << order) + 1;
	int o;

	if (gfp_high)
		min  -= min / 2;
	if (can_try_harder)
		min -= min / 4;

	if (free_pages <= min + z->lowmem_reserve[classzone_idx])
		return 0;

	for (o = 0; o < order; o++) {
		/* At the next order, this order's pages become unavailable */
		free_pages -= z->free_area[o].nr_free << o;

		/* Require fewer higher order pages to be free */
		min >>= 1;

		if (free_pages <= min)
			return 0;
	}
	return 1;
}

/*
 * The order of subdivision here is critical for the IO subsystem.
 * Please do not alter this order without good reasons and regression
 * testing. Specifically, as large blocks of memory are subdivided,
 * the order in which smaller blocks are delivered depends on the order
 * they're subdivided in this function. This is the primary factor
 * influencing the order in which pages are delivered to the IO
 * subsystem according to empirical testing, and this is also justified
 * by considering the behavior of a buddy system containing a single
 * large block of memory acted on by a series of small allocations.
 * This behavior is a critical factor in sglist merging's success.
 *
 * -- wli
 */
static inline struct page_bs *
expand_bs(struct zone_bs *zone, struct page_bs *page,
		int low, int high, struct free_area_bs *area)
{
	unsigned long size = 1 << high;

	while (high > low) {
		area--;
		high--;
		size >>= 1;
		BUG_ON_BS(bad_range_bs(zone, &page[size]));
		list_add(&page[size].lru, &area->free_list);
		area->nr_free++;
		set_page_order_bs(&page[size], high);
	}
	return page;
}

/* 
 * Do the hard work of removing an element from the buddy allocator.
 * Call me with the zone->lock already held.
 */
static struct page_bs *__rmqueue_bs(struct zone_bs *zone, unsigned int order)
{
	struct free_area_bs *area;
	unsigned int current_order;
	struct page_bs *page;

	for (current_order = order; current_order < MAX_ORDER_BS; 
							++current_order) {
		area = zone->free_area + current_order;
		if (list_empty(&area->free_list))
			continue;

		page = list_entry(area->free_list.next, struct page_bs, lru);
		list_del(&page->lru);
		rmv_page_order_bs(page);
		area->nr_free--;
		zone->free_pages -= 1UL << order;
		return expand_bs(zone, page, order, current_order, area);
	}
	return NULL;
}

/* 
 * Obtain a specified number of elements from the buddy allocator, all under
 * a single hold of the lock, for efficiency.  Add them to the supplied list.
 * Returns the number of new pages which were placed at *list.
 */
static int rmqueue_bulk_bs(struct zone_bs *zone, unsigned int order,
		unsigned long count, struct list_head *list)
{
	unsigned long flags;
	int i;
	int allocated = 0;
	struct page_bs *page;

	spin_lock_irqsave(&zone->lock, flags);
	for (i = 0; i < count; ++i) {
		page = __rmqueue_bs(zone, order);
		if (page == NULL)
			break;
		allocated++;
		list_add_tail(&page->lru, list);
	}
	spin_unlock_irqrestore(&zone->lock, flags);
	return allocated;
}

/*
 * This page is about to be returned from the apge allocator
 */
static void prep_new_page_bs(struct page_bs *page, int order)
{
	if (page->mapping || page_mapcount_bs(page) ||
	   (page->flags & (
			1 << PG_private_bs	|
			1 << PG_locked_bs	|
			1 << PG_lru_bs		|
			1 << PG_active_bs	|
			1 << PG_dirty_bs	|
			1 << PG_reclaim_bs	|
			1 << PG_swapcache_bs	|
			1 << PG_writeback_bs)))
		bad_page_bs(__FUNCTION__, page);

	page->flags &= ~(1 << PG_uptodate_bs | 1 << PG_error_bs |
			 1 << PG_referenced_bs | 1 << PG_arch_1_bs |
			 1 << PG_checked_bs | 1 << PG_mappedtodisk_bs);
	page->private = 0;
	set_page_refs_bs(page, order);
	kernel_map_pages_bs(page, 1 << order, 1);
}

static inline void prep_zero_page_bs(struct page_bs *page, int order,
			unsigned int __nocast gfp_flags)
{
	int i;

	BUG_ON_BS((gfp_flags & (__GFP_WAIT_BS | __GFP_HIGHMEM_BS)) == 
							__GFP_HIGHMEM_BS);
	for (i = 0; i < (1 << order); i++)
		BS_DUP();
}

/*
 * Really, prep_compound_page() should be called from __rmqueue_bulk().  But
 * we cheat by calling it from here, in the order > 0 path.  Saves a branch
 * or two.
 */
static struct page_bs *
buffered_rmqueue_bs(struct zone_bs *zone, int order, 
				unsigned int __nocast gfp_flags)
{
	unsigned long flags;
	struct page_bs *page = NULL;
	int cold = !!(gfp_flags & __GFP_COLD_BS);

	if (order == 0) {
		struct per_cpu_pages_bs *pcp;

		pcp = &zone->pageset[get_cpu()].pcp[cold];
		local_irq_save(flags);
		if (pcp->count >= pcp->low)
			pcp->count += rmqueue_bulk_bs(zone, 0,
				pcp->batch, &pcp->list);
		if (pcp->count) {
			page = list_entry(pcp->list.next, struct page_bs, lru);
			list_del(&page->lru);
			pcp->count--;
		}
		local_irq_restore(flags);
		put_cpu();
	}

	if (page == NULL) {
		spin_lock_irqsave(&zone->lock, flags);
		page = __rmqueue_bs(zone, order);
		spin_unlock_irqrestore(&zone->lock, flags);
	}

	if (page != NULL) {
		BUG_ON_BS(bad_range_bs(zone, page));
		mod_page_state_zone_bs(zone, pgalloc, 1 << order);
		prep_new_page_bs(page, order);

		if (gfp_flags & __GFP_ZERO_BS)
			prep_zero_page_bs(page, order, gfp_flags);

		if (order && (gfp_flags & __GFP_COMP_BS))
			prep_compound_page_bs(page, order);
	}
	return page;
}

static void zone_statistics_bs(struct zonelist_bs *zonelist, 
						struct zone_bs *z)
{
}

/*
 * This is the 'heart' of the zoned buddy allocator.
 */
struct page_bs *fastcall_bs
__alloc_pages_bs(unsigned int __nocast gfp_mask, unsigned int order,
		struct zonelist_bs *zonelist)
{
	const int wait = gfp_mask & __GFP_WAIT_BS;
	struct zone_bs **zones, *z;
	struct page_bs *page;
	struct reclaim_state_bs reclaim_state;
	struct task_struct *p = current;
	int i;
	int classzone_idx;
	int do_retry;
	int can_try_harder;
	int did_some_progress;

	might_sleep_if(wait);

	/*
	 * The caller may dip into page reserves a bit more if the caller
	 * cannot run direct reclaim, or is the caller has realtime scheduling
	 * policy.
	 */
	can_try_harder = (unlikely(rt_task_bs(p)) && !in_interrupt()) || !wait;

	zones = zonelist->zones; /* the list of zones suitable for gfp_mask */

	if (unlikely(zones[0] == NULL)) {
		/* Should this ever happen?? */
		return NULL;
	}

	classzone_idx = zone_idx_bs(zones[0]);

restart:
	/* Go through the zonelist once, looking for a zone with enough free */
	for (i = 0; (z = zones[i]) != NULL; i++) {
		if (!zone_watermark_ok_bs(z, order, z->pages_low,
					classzone_idx, 0, 0))
			continue;

		if (wait && !cpuset_zone_allowed_bs(z))
			continue;

		page = buffered_rmqueue_bs(z, order, gfp_mask);
		if (page)
			goto got_pg;
	}

	BS_DUP();
got_pg:
	zone_statistics_bs(zonelist, z);
	return page;
}
EXPORT_SYMBOL_GPL(__alloc_pages_bs);

fastcall_bs void free_pages_bs(unsigned long addr, unsigned int order)
{
	if (addr != 0) {
		BUG_ON_BS(!virt_addr_valid_bs((void *)addr));
		__free_pages_bs(virt_to_page_bs((void *)addr), order);
	}
}
EXPORT_SYMBOL_GPL(free_pages_bs);
