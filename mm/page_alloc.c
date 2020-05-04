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
#include <linux/seq_file.h>
#include "biscuitos/kernel.h"
#include "biscuitos/nodemask.h"
#include "biscuitos/mmzone.h"
#include "biscuitos/bootmem.h"
#include "biscuitos/init.h"
#include "biscuitos/mm.h"
#include "biscuitos/swap.h"
#include "biscuitos/cpuset.h"
#include "biscuitos/page-flags.h"
#include "biscuitos/sched.h"
#include "biscuitos/slab.h"
#include "biscuitos/vmalloc.h"
#include "biscuitos/highmem.h"
#include "biscuitos/swap.h"
#include "biscuitos/pagevec.h"
#include "asm-generated/percpu.h"

nodemask_t_bs node_online_map_bs = { { [0] = 1UL } };

#ifndef CONFIG_NEED_MULTIPLE_NODES
static bootmem_data_t_bs contig_bootmem_data_bs;
struct pglist_data_bs contig_page_data_bs = { 
	.bdata = &contig_bootmem_data_bs 
};
EXPORT_SYMBOL_GPL(contig_page_data_bs);
#endif
struct pglist_data_bs *pgdat_list_bs;
long nr_swap_pages_bs;

EXPORT_SYMBOL_GPL(nr_swap_pages_bs);

/*
 * Used by page_zone() to look up the address of the struct zone whose
 * id is encoded in the upper bits of page->flags
 */
struct zone_bs *zone_table_bs[1 << ZONETABLE_SHIFT_BS];
EXPORT_SYMBOL_GPL(zone_table_bs);

/*
 * Accumulate the page_state information across all CPUs.
 * The result is unavoidably approximate - it can change
 * during and after execution of this function.
 */
static DEFINE_PER_CPU_BS(struct page_state_bs, page_states_bs) = {0};

/*
 * results with 256, 32 in the lowmem_reserve sysctl:
 *      1G machine -> (16M dma, 800M-16M normal, 1G-800M high)
 *      1G machine -> (16M dma, 784M normal, 224M high)
 *      NORMAL allocation will leave 784M/256 of ram reserved in the ZONE_DMA
 *      HIGHMEM allocation will leave 224M/32 of ram reserved in ZONE_NORMAL
 *      HIGHMEM allocation will (224M+784M)/256 of ram reserved in ZONE_DMA
 */
int sysctl_lowmem_reserve_ratio_bs[MAX_NR_ZONES_BS-1] = { 256, 32 };

unsigned long __initdata nr_kernel_pages_bs;
unsigned long __initdata nr_all_pages_bs;
unsigned long totalram_pages_bs;
unsigned long totalhigh_pages_bs;

static char *zone_names_bs[MAX_NR_ZONES_BS] = { "DMA", "Normal", "HighMem" };
int min_free_kbytes_bs = 1024;

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
	/* Skip empty nodes */
	if (!pgdat->node_spanned_pages)
		return;

#ifdef CONFIG_FLAT_NODE_MEM_MAP
	/* ia64 gets its own node_mem_map, before this, without bootmem */
	if (!pgdat->node_mem_map) {
		unsigned long size;
		struct page_bs *map;

		size = (pgdat->node_spanned_pages + 1) * sizeof(struct page_bs);
		map = alloc_remap_bs(pgdat->node_id, size);
		if (!map)
			map = alloc_bootmem_node_bs(pgdat, size);
		pgdat->node_mem_map = map;
	}
#ifdef CONFIG_FLATMEM
	/*
	 * With no DISCONTIG, the global mem_map is just set as node 0's
	 */
	if (pgdat == NODE_DATA_BS(0))
		mem_map_bs = NODE_DATA_BS(0)->node_mem_map;
#endif
#endif /* CONFIG_FLAT_NODE_MEM_MAP */
}

#ifdef CONFIG_NUMA
static void show_node_bs(struct zone *zone)
{
	printk("Node %d ", zone->zone_pgdat->node_id);
}
#else
#define show_node_bs(zone)	do { } while (0)
#endif

void __get_page_state_bs(struct page_state_bs *ret, int nr);

void get_page_state_bs(struct page_state_bs *ret)
{
	int nr;

	nr = offsetof(struct page_state_bs, GET_PAGE_STATE_LAST_BS);
	nr /= sizeof(unsigned long);

	__get_page_state_bs(ret, nr + 1);
}

void __get_zone_counts_bs(unsigned long *active, unsigned long *inactive,
		unsigned long *free, struct pglist_data_bs *pgdat)
{
	struct zone_bs *zones = pgdat->node_zones;
	int i;

	*active = 0;
	*inactive = 0;
	*free = 0;

	for (i = 0; i < MAX_NR_ZONES_BS; i++) {
		*active += zones[i].nr_active;
		*inactive += zones[i].nr_inactive;
		*free += zones[i].free_pages;
	}
}

void get_zone_counts_bs(unsigned long *active,
		unsigned long *inactive, unsigned long *free)
{
	struct pglist_data_bs *pgdat;

	*active = 0;
	*inactive = 0;
	*free = 0;

	for_each_pgdat_bs(pgdat) {
		unsigned long l, m, n;

		__get_zone_counts_bs(&l, &m, &n, pgdat);
		*active += l;
		*inactive += m;
		*free += n;
	}
}

#ifdef CONFIG_HIGHMEM_BS
unsigned int nr_free_highpages_bs(void)
{
	pg_data_t_bs *pgdat;
	unsigned int pages = 0;

	for_each_pgdat_bs(pgdat)
		pages += pgdat->node_zones[ZONE_HIGHMEM_BS].free_pages;

	return pages;
}
#endif

#define K_BS(x)	((x) << (PAGE_SHIFT_BS-10))

/*
 * Show free area list (used inside shift_scroll-lock stuff)
 * We also calculate the percentage fragmentation. We do this by counting the
 * memory on each free list with the exception of the first item on the list.
 */
void show_free_areas_bs(void)
{
	struct page_state_bs ps;
	int cpu, temperature;
	unsigned long active;
	unsigned long inactive;
	unsigned long free;
	struct zone_bs *zone;

	for_each_zone_bs(zone) {
		show_node_bs(zone);
		printk("%s per-cpu:", zone->name);

		if (!zone->present_pages) {
			printk(" empty\n");
			continue;
		} else
			printk("\n");

		for (cpu = 0; cpu < NR_CPUS_BS; ++cpu) {
			struct per_cpu_pageset_bs *pageset;

			if (!cpu_possible(cpu))
				continue;

			pageset = zone_pcp_bs(zone, cpu);

			for (temperature = 0; temperature < 2; temperature++)
				printk("cpu %d %s: low %d, high %d, batch %d "
					"used: %d\n",
					cpu,
					temperature ? "cold" : "hot",
					pageset->pcp[temperature].low,
					pageset->pcp[temperature].high,
					pageset->pcp[temperature].batch,
					pageset->pcp[temperature].count);
		}
	}
	get_page_state_bs(&ps);
	get_zone_counts_bs(&active, &inactive, &free);

	printk("Free pages: %11ukB (%ukB HighMem)\n",
			K_BS(nr_free_pages_bs()),
			K_BS(nr_free_highpages_bs()));

	printk("Active:%lu inactive:%lu dirty:%lu writeback:%lu "
		"unstable:%lu free:%u slab:%lu mapped:%lu pagetables:%lu\n",
		active,
		inactive,
		ps.nr_dirty,
		ps.nr_writeback,
		ps.nr_unstable,
		nr_free_pages_bs(),
		ps.nr_slab,
		ps.nr_mapped,
		ps.nr_page_table_pages);

	for_each_zone_bs(zone) {
		int i;

		show_node_bs(zone);
		printk("%s"
			" free:%lukB"
			" min:%lukB"
			" low:%lukB"
			" high:%lukB"
			" active:%lukB"
			" inactive:%lukB"
			" present:%lukB"
			" pages_scanned:%lukB"
			" all_unreclaimable? %s"
			"\n",
			zone->name,
			K_BS(zone->free_pages),
			K_BS(zone->pages_min),
			K_BS(zone->pages_low),
			K_BS(zone->pages_high),
			K_BS(zone->nr_active),
			K_BS(zone->nr_inactive),
			K_BS(zone->present_pages),
			zone->pages_scanned,
			(zone->all_unreclaimable ? "yes" : "no")
			);
		printk("lomem_reserve[]:");
		for (i = 0; i < MAX_NR_ZONES_BS; i++)
			printk(" %lu", zone->lowmem_reserve[i]);
		printk("\n");
	}

	for_each_zone_bs(zone) {
		unsigned long nr, flags, order, total = 0;

		show_node_bs(zone);
		printk("%s: ", zone->name);
		if (!zone->present_pages) {
			printk("empty\n");
			continue;
		}

		spin_lock_irqsave(&zone->lock, flags);
		for (order = 0; order < MAX_ORDER_BS; order++) {
			nr = zone->free_area[order].nr_free;
			total += nr << order;
			printk("%lu*%lukB ", nr, K_BS(1UL) << order);
		}
		spin_unlock_irqrestore(&zone->lock, flags);
		printk("= %lukB\n", K_BS(total));
	}
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
	struct page_bs *page;
	unsigned long end_pfn = start_pfn + size;
	unsigned long pfn;

	for (pfn = start_pfn; pfn < end_pfn; pfn++, page++) {
		if (!early_pfn_valid_bs(pfn))
			continue;
		if (!early_pfn_in_nid_bs(pfn, nid))
			continue;
		page = pfn_to_page_bs(pfn);
		set_page_links_bs(page, zone, nid, pfn);
		set_page_count_bs(page, 0);
		reset_page_mapcount_bs(page);
		SetPageReserved_bs(page);
		INIT_LIST_HEAD(&page->lru);
#ifdef WAIT_PAGE_VIRTUAL
		/* This shift won't overflow because ZONE_NORMAL is below 4G. */
		if (!is_highmem_idx_bs(zone))
			set_page_address_bs(page, 
					__va_bs(pfn << PAGE_SHIFT_BS));
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

#define ZONETABLE_INDEX_BS(x, zone_nr)	((x << ZONES_SHIFT_BS) | zone_nr)
void zonetable_add_bs(struct zone_bs *zone, int nid, int zid, 
				unsigned long pfn, unsigned long size)
{
	unsigned long snum = pfn_to_section_nr_bs(pfn);
	unsigned long end = pfn_to_section_nr_bs(pfn + size);

	if (FLAGS_HAS_NODE_BS)
		zone_table_bs[ZONETABLE_INDEX_BS(nid, zid)] = zone;
	else
		for (; snum <= end; snum++)
			zone_table_bs[ZONETABLE_INDEX_BS(snum, zid)] = zone;
}

inline void setup_pageset_bs(struct per_cpu_pageset_bs *p, unsigned long batch)
{
	struct per_cpu_pages_bs *pcp;

	pcp = &p->pcp[0];		/* hot */
	pcp->count = 0;
	pcp->low = 2 * batch;
	pcp->high = 6 * batch;
	pcp->batch = max(1UL, 1 * batch);
	INIT_LIST_HEAD(&pcp->list);

	pcp = &p->pcp[1];		/* cold*/
	pcp->count = 0;
	pcp->low = 0;
	pcp->high = 2 * batch;
	pcp->batch = max(1UL, 1 * batch);
	INIT_LIST_HEAD(&pcp->list);
}

static int __devinit zone_batchsize_bs(struct zone_bs *zone)
{
	int batch;

	/*
	 * The per-cpu-pages pools are set to around 1000th of the
	 * size of the zone.  But no more than 1/4 of a meg - there's
	 * no point in going beyond the size of L2 cache.
	 *
	 * OK, so we don't know how big the cache is.  So guess.
	 */
	batch = zone->present_pages / 1024;
	if (batch * PAGE_SIZE_BS > 256 * 1024)
		batch = (256 * 1024) / PAGE_SIZE_BS;
	batch /= 4;		/* We effectively *= 4 below */
	if (batch < 1)
		batch = 1;

	/*
	 * Clamp the batch to a 2^n - 1 value. Having a power
	 * of 2 value was found to be more likely to have
	 * suboptimal cache aliasing properties in some cases.
	 *
	 * For example if 2 tasks are alternately allocating
	 * batches of pages, one task can end up with a lot
	 * of pages of one half of the possible page colors
	 * and the other with pages of the other colors.
	 */
	batch = (1 << fls(batch + batch/2)) - 1;
	return batch;
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
	int cpu, nid = pgdat->node_id;
	unsigned long zone_start_pfn = pgdat->node_start_pfn;

	pgdat->nr_zones = 0;
	init_waitqueue_head(&pgdat->kswapd_wait);
	pgdat->kswapd_max_order = 0;

	for (j = 0; j < MAX_NR_ZONES_BS; j++) {
		struct zone_bs *zone = pgdat->node_zones + j;
		unsigned long size, realsize;
		unsigned long batch;

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

		batch = zone_batchsize_bs(zone);

		for (cpu = 0; cpu < NR_CPUS_BS; cpu++) {
#ifdef CONFIG_NUMA
			/* Early boot. Slab allocator not functional yet */
			zone->pageset[cpu] = &boot_pageset_bs[cpu];
			setup_pageset_bs(&boot_pageset_bs[cpu],0);
#else
			setup_pageset_bs(zone_pcp_bs(zone,cpu), batch);
#endif
		}
		printk(KERN_DEBUG "  %s zone: %lu pages, LIFO batch:%lu\n",
				zone_names_bs[j], realsize, batch);
		INIT_LIST_HEAD(&zone->active_list);
		INIT_LIST_HEAD(&zone->inactive_list);
		zone->nr_scan_active = 0;
		zone->nr_scan_inactive = 0;
		zone->nr_inactive = 0;
		atomic_set(&zone->reclaim_in_progress, -1);
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

		memmap_init_bs(size, nid, j, zone_start_pfn);

		zonetable_add_bs(zone, nid, j, zone_start_pfn, size);
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

unsigned long __read_page_state_bs(unsigned long offset)
{
	unsigned long ret = 0;
	int cpu;

	for_each_online_cpu(cpu) {
		unsigned long in;

		in = (unsigned long)&per_cpu_bs(page_states_bs, cpu) + offset;
		ret += *((unsigned long *)in);
	}
	return ret;
}

void __mod_page_state_bs(unsigned long offset, unsigned long delta)
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
	page->flags &= ~(1 << PG_lru_bs			|
			1 << PG_private_bs		|
			1 << PG_locked_bs		|
			1 << PG_active_bs		|
			1 << PG_dirty_bs		|
			1 << PG_reclaim_bs		|
			1 << PG_slab_bs			|
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
	pcp = &zone_pcp_bs(zone, get_cpu())->pcp[cold];
	local_irq_save(flags);
	list_add(&page->lru, &pcp->list);
	pcp->count++;
	if (pcp->count >= pcp->high)
		pcp->count -= free_pages_bulk_bs(zone, pcp->batch, 
							&pcp->list, 0);
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
EXPORT_SYMBOL_GPL(zone_watermark_ok_bs);

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
	if (	page_mapcount_bs(page)	||
		page->mapping != NULL ||
		page_count_bs(page) != 0 ||
		(page->flags & (
			1 << PG_lru_bs		|
			1 << PG_private_bs	|
			1 << PG_locked_bs	|
			1 << PG_active_bs	|
			1 << PG_dirty_bs	|
			1 << PG_reclaim_bs	|
			1 << PG_slab_bs		|
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
		clear_highpage_bs(page + i);
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

		pcp = &zone_pcp_bs(zone, get_cpu())->pcp[cold];
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

static inline int
should_reclaim_zone_bs(struct zone_bs *z, unsigned int gfp_mask)
{
	if (!z->reclaim_pages)
		return 0;
	if (gfp_mask & __GFP_NORECLAIM_BS)
		return 0;
	return 1;
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
		int do_reclaim = should_reclaim_zone_bs(z, gfp_mask);

		if (wait && !cpuset_zone_allowed_bs(z))
			continue;

		/*
		 * If the zone is to attempt early page reclaim then this loop
		 * will try to reclaim pages and check the watermark a second
		 * time before giving up and falling back to the next zone.
		 */
zone_reclaim_retry:
		if (!zone_watermark_ok_bs(z, order, z->pages_low,
				       classzone_idx, 0, 0)) {
			if (!do_reclaim)
				continue;
			else {
				zone_reclaim_bs(z, gfp_mask, order);
				/* Only try reclaim once */
				do_reclaim = 0;
				goto zone_reclaim_retry;
			}
		}

		page = buffered_rmqueue_bs(z, order, gfp_mask);
		if (page)
			goto got_pg;
	}

	for (i = 0; (z = zones[i]) != NULL; i++)
		wakeup_kswapd_bs(z, order);

	/*
	 * Go through the zonelist again. Let __GFP_HIGH and allocations
	 * coming from realtime tasks to go deeper into reserves
	 *
	 * This is the last chance, in general, before the goto nopage.
	 * Ignore cpuset if GFP_ATOMIC (!wait) rather than fail alloc.
	 */
	for (i = 0; (z = zones[i]) != NULL; i++) {
		if (!zone_watermark_ok_bs(z, order, z->pages_min,
					classzone_idx, can_try_harder,
					gfp_mask & __GFP_HIGH_BS))
			continue;

		if (wait && !cpuset_zone_allowed_bs(z))
			continue;

		page = buffered_rmqueue_bs(z, order, gfp_mask);
		if (page)
			goto got_pg;
	}

	/* This allocation should allow future memory freeing */

	if (((p->flags & PF_MEMALLOC_BS) || 
			unlikely(test_thread_flag(TIF_MEMDIE))) &&
			!in_interrupt()) {
		if (!(gfp_mask & __GFP_NOMEMALLOC_BS)) {
			/* go through the zonelist yet again, ignore mins */
			for (i = 0; (z = zones[i]) != NULL; i++) {
				if (!cpuset_zone_allowed_bs(z))
					continue;

				page = buffered_rmqueue_bs(z, order, gfp_mask);
				if (page)
					goto got_pg;
			}
		}
		goto nopage;
	}

	/* Atomic allocations - we can't balance anything */
	if (!wait)
		goto nopage;

rebalance:
	cond_resched();

	/* We now go into synchronous reclaim */
	p->flags |= PF_MEMALLOC_BS;
	reclaim_state.reclaimed_slab = 0;
	p->reclaim_state = (struct reclaim_state *)&reclaim_state;

	did_some_progress = try_to_free_pages_bs(zones, gfp_mask);

	p->reclaim_state = NULL;
	p->flags &= ~PF_MEMALLOC_BS;

	cond_resched();

	if (likely(did_some_progress)) {
		for (i = 0; (z = zones[i]) != NULL; i++) {
			if (!zone_watermark_ok_bs(z, order, z->pages_min,
					classzone_idx, can_try_harder,
					gfp_mask & __GFP_HIGH))
				continue;

			if (!cpuset_zone_allowed_bs(z))
				continue;

			page = buffered_rmqueue_bs(z, order, gfp_mask);
			if (page)
				goto got_pg;
		}
	} else if ((gfp_mask & __GFP_FS_BS) && 
					!(gfp_mask & __GFP_NORETRY_BS)) {
		/*
		 * Go through the zonelist yet one more time, keep
		 * very high watermark here, this is only to catch
		 * a parallel oom killing, we must fail if we're still
		 * under heavy pressure.
		 */
		for (i = 0; (z = zones[i]) != NULL; i++) {
			if (!zone_watermark_ok_bs(z, order, z->pages_high,
					classzone_idx, 0, 0))
				continue;

			if (!cpuset_zone_allowed_bs(z))
				continue;

			page = buffered_rmqueue_bs(z, order, gfp_mask);
			if (page)
				goto got_pg;
		}
		out_of_memory_bs(gfp_mask, order);
		goto restart;
	}

        /*
         * Don't let big-order allocations loop unless the caller explicitly
         * requests that.  Wait for some write requests to complete then retry.
         *
         * In this implementation, __GFP_REPEAT means __GFP_NOFAIL for order
         * <= 3, but that may not be true in other implementations.
         */
	do_retry = 0;
	if (!(gfp_mask & __GFP_NORETRY_BS)) {
		if ((order <= 3) || (gfp_mask & __GFP_REPEAT_BS))
			do_retry = 1;
		if (gfp_mask & __GFP_NOFAIL_BS)
			do_retry = 1;
	}
	if (do_retry) {
		BS_DUP();
		goto rebalance;
	}

nopage:
	if (!(gfp_mask & __GFP_NOWARN_BS)) {
		printk(KERN_INFO "%s: page allocation failure."
				" order:%d, mode:0x%x\n",
			p->comm, order, gfp_mask);
		dump_stack();
		show_mem_bs();
	}
got_pg:
	zone_statistics_bs(zonelist, z);
	return page;
}
EXPORT_SYMBOL_GPL(__alloc_pages_bs);

fastcall_bs unsigned long get_zeroed_page_bs(unsigned int __nocast gfp_mask)
{
	struct page_bs *page;

	/*
	 * get_zeroed_page() returns a 32-bit address, which cannot represent
	 * a highmem page.
	 */
	BUG_ON_BS(gfp_mask & __GFP_HIGHMEM);

	page = alloc_pages_bs(gfp_mask | __GFP_ZERO_BS, 0);
	if (page)
		return (unsigned long) page_address_bs(page);
	return 0;
}

void __pagevec_free_bs(struct pagevec_bs *pvec)
{
	int i = pagevec_count_bs(pvec);

	while (--i >= 0)
		free_hot_cold_page_bs(pvec->pages[i], pvec->cold);
}
EXPORT_SYMBOL_GPL(__pagevec_free_bs);

fastcall_bs void free_pages_bs(unsigned long addr, unsigned int order)
{
	if (addr != 0) {
		BUG_ON_BS(!virt_addr_valid_bs((void *)addr));
		__free_pages_bs(virt_to_page_bs((void *)addr), order);
	}
}
EXPORT_SYMBOL_GPL(free_pages_bs);

/*
 * Common helper functions
 */
fastcall_bs unsigned long __get_free_pages_bs(unsigned int __nocast gfp_mask,
						unsigned int order)
{
	struct page_bs *page;

	page = alloc_pages_bs(gfp_mask, order);
	if (!page)
		return 0;
	return (unsigned long)page_address_bs(page);
}

void __get_page_state_bs(struct page_state_bs *ret, int nr)
{
	int cpu = 0;

	memset(ret, 0, sizeof(*ret));

	while (cpu < NR_CPUS_BS) {
		unsigned long *in, *out, off;

		in = (unsigned long *)&per_cpu_bs(page_states_bs, cpu);

		cpu++;

		if (cpu < NR_CPUS_BS)
			prefetch(&per_cpu_bs(page_states_bs, cpu));

		out = (unsigned long *)ret;
		for (off = 0; off < nr; off++)
			*out++ += *in++;
	}
	
}

void get_full_page_state_bs(struct page_state_bs *ret)
{
	__get_page_state_bs(ret, sizeof(*ret) / sizeof(unsigned long));
}

static void *frag_start_bs(struct seq_file *m, loff_t *pos)
{
	pg_data_t_bs *pgdat;
	loff_t node = *pos;

	for (pgdat = pgdat_list_bs; pgdat && node; pgdat = pgdat->pgdat_next)
		--node;

	return pgdat;
}

static void *frag_next_bs(struct seq_file *m, void *arg, loff_t *pos)
{
	pg_data_t_bs *pgdat = (pg_data_t_bs *)arg;

	(*pos)++;
	return pgdat->pgdat_next;
}

static void frag_stop_bs(struct seq_file *m, void *arg)
{
}

/*
 * This walks the free areas for each zone.
 */
static int frag_show_bs(struct seq_file *m, void *arg)
{
	pg_data_t_bs *pgdat = (pg_data_t_bs *)arg;
	struct zone_bs *zone;
	struct zone_bs *node_zones = pgdat->node_zones;
	unsigned long flags;
	int order;

	for (zone = node_zones; zone - node_zones < MAX_NR_ZONES_BS; ++zone) {
		if (!zone->present_pages)
			continue;

		spin_lock_irqsave(&zone->lock, flags);
		seq_printf(m, "Node %d, zone %8s ", 
					pgdat->node_id, zone->name);
		for (order = 0; order < MAX_ORDER_BS; ++order)
			seq_printf(m, "%6lu ", zone->free_area[order].nr_free);
		spin_unlock_irqrestore(&zone->lock, flags);
		seq_putc(m, '\n');
	}
	return 0;
}

struct seq_operations fragmentation_op_bs = {
	.start	= frag_start_bs,
	.next	= frag_next_bs,
	.stop	= frag_stop_bs,
	.show	= frag_show_bs,
};

/*
 * Output information about zones in @pgdat.
 */
static int zoneinfo_show_bs(struct seq_file *m, void *arg)
{
	pg_data_t_bs *pgdat = arg;
	struct zone_bs *zone;
	struct zone_bs *node_zones = pgdat->node_zones;
	unsigned long flags;

	for (zone = node_zones; zone - node_zones < MAX_NR_ZONES_BS; zone++) {
		int i;

		if (!zone->present_pages)
			continue;

		spin_lock_irqsave(&zone->lock, flags);
		seq_printf(m, "Node %d, zone %8s", pgdat->node_id, zone->name);
		seq_printf(m,
			   "\n  pages free     %lu"
			   "\n        min      %lu"
			   "\n        low      %lu"
			   "\n        high     %lu"
			   "\n        active   %lu"
			   "\n        inactive %lu"
			   "\n        scanned  %lu (a: %lu i: %lu)"
			   "\n        spanned  %lu"
			   "\n        present  %lu",
			   zone->free_pages,
			   zone->pages_min,
			   zone->pages_low,
			   zone->pages_high,
			   zone->nr_active,
			   zone->nr_inactive,
			   zone->pages_scanned,
			   zone->nr_scan_active, zone->nr_scan_inactive,
			   zone->spanned_pages,
			   zone->present_pages);
		seq_printf(m,
			   "\n        protection: (%lu",
			   zone->lowmem_reserve[0]);
		for (i = 1; i < ARRAY_SIZE(zone->lowmem_reserve); i++)
			seq_printf(m, ", %lu", zone->lowmem_reserve[i]);
		seq_printf(m,
			   ")"
			   "\n  pagesets");
		for (i = 0; i < ARRAY_SIZE(zone->pageset); i++) {
			struct per_cpu_pageset_bs *pageset;
			int j;

			pageset = zone_pcp_bs(zone, i);
			for (j = 0; j < ARRAY_SIZE(pageset->pcp); j++) {
				if (pageset->pcp[j].count)
					break;
			}
			if (j == ARRAY_SIZE(pageset->pcp))
				continue;
			for (j = 0; j < ARRAY_SIZE(pageset->pcp); j++) {
				seq_printf(m,
					   "\n    cpu: %i pcp: %i"
					   "\n              count: %i"
					   "\n              low:   %i"
					   "\n              high:  %i"
					   "\n              batch: %i",
					   i, j,
					   pageset->pcp[j].count,
					   pageset->pcp[j].low,
					   pageset->pcp[j].high,
					   pageset->pcp[j].batch);
			}
#ifdef CONFIG_NUMA
			seq_printf(m,
				   "\n            numa_hit:       %lu"
				   "\n            numa_miss:      %lu"
				   "\n            numa_foreign:   %lu"
				   "\n            interleave_hit: %lu"
				   "\n            local_node:     %lu"
				   "\n            other_node:     %lu",
				   pageset->numa_hit,
				   pageset->numa_miss,
				   pageset->numa_foreign,
				   pageset->interleave_hit,
				   pageset->local_node,
				   pageset->other_node);
#endif
		}
		seq_printf(m,
			   "\n  all_unreclaimable: %u"
			   "\n  prev_priority:     %i"
			   "\n  temp_priority:     %i"
			   "\n  start_pfn:         %lu",
			   zone->all_unreclaimable,
			   zone->prev_priority,
			   zone->temp_priority,
			   zone->zone_start_pfn);
		spin_unlock_irqrestore(&zone->lock, flags);
		seq_putc(m, '\n');
	}
	return 0;
}

struct seq_operations zoneinfo_op_bs = {
	.start	= frag_start_bs, /* iterate over all zones. The same as in
			          * fragmentation. */
	.next	= frag_next_bs,
	.stop	= frag_stop_bs,
	.show	= zoneinfo_show_bs,
};

static char *vmstat_text_bs[] = {
	"nr_dirty",
	"nr_writeback",
	"nr_unstable",
	"nr_page_table_pages",
	"nr_mapped",
	"nr_slab",

	"pgpgin",
	"pgpgout",
	"pswpin",
	"pswpout",
	"pgalloc_high",

	"pgalloc_normal",
	"pgalloc_dma",
	"pgfree",
	"pgactivate",
	"pgdeactivate",

	"pgfault",
	"pgmajfault",
	"pgrefill_high",
	"pgrefill_normal",
	"pgrefill_dma",

	"pgsteal_high",
	"pgsteal_normal",
	"pgsteal_dma",
	"pgscan_kswapd_high",
	"pgscan_kswapd_normal",

	"pgscan_kswapd_dma",
	"pgscan_direct_high",
	"pgscan_direct_normal",
	"pgscan_direct_dma",
	"pginodesteal",

	"slabs_scanned",
	"kswapd_steal",
	"kswapd_inodesteal",
	"pageoutrun",
	"allocstall",

	"pgrotated",
	"nr_bounce",
};

static void *vmstat_start_bs(struct seq_file *m, loff_t *pos)
{
	struct page_state_bs *ps;

	if (*pos >= ARRAY_SIZE(vmstat_text_bs))
		return NULL;

	ps = kmalloc_bs(sizeof(*pos), GFP_KERNEL_BS);
	m->private = ps;
	if (!ps)
		return ERR_PTR(-ENOMEM);
	get_full_page_state_bs(ps);
	ps->pgpgin /= 2;
	ps->pgpgin /= 2;
	return (unsigned long *)ps + *pos;
}

static void *vmstat_next_bs(struct seq_file *m, void *arg, loff_t *pos)
{
	(*pos)++;
	if (*pos >= ARRAY_SIZE(vmstat_text_bs))
		return NULL;
	return (unsigned long *)m->private + *pos;
}

static int vmstat_show_bs(struct seq_file *m, void *arg)
{
	unsigned long *l = arg;
	unsigned long off = l - (unsigned long *)m->private;

	seq_printf(m, "%s %lu\n", vmstat_text_bs[off], *l);
	return 0;
}

static void vmstat_stop_bs(struct seq_file *m, void *arg)
{
	kfree_bs(m->private);
	m->private = NULL;
}

struct seq_operations vmstat_op_bs = {
	.start	= vmstat_start_bs,
	.next	= vmstat_next_bs,
	.stop	= vmstat_stop_bs,
	.show	= vmstat_show_bs,
};

/*
 * setup_per_zone_pages_min - called when min_free_kbytes changes. Ensures
 *      that the pages_{min,low,high} values for each zone are set correctly
 *      with respect to min_free_kbytes.
 */
static void setup_per_zone_pages_min_bs(void)
{
	unsigned long pages_min = min_free_kbytes_bs >> (PAGE_SHIFT_BS - 10);
	unsigned long lowmem_pages = 0;
	struct zone_bs *zone;
	unsigned long flags;

	/* Calculate total number of !ZONE_HIGHMEM pages */
	for_each_zone_bs(zone) {
		if (!is_highmem_bs(zone))
			lowmem_pages += zone->present_pages;
	}

	for_each_zone_bs(zone) {
		spin_lock_irqsave(&zone->lru_lock, flags);
		if (is_highmem_bs(zone)) {
			/*
			 * Often, highmem doesn't need to reserve any pages.
			 * But the pages_min/low/high values are also used
			 * for batching up page reclaim activity so we need
			 * a decent value here.
			 */
			int min_pages;

			min_pages = zone->present_pages / 1024;
			if (min_pages < SWAP_CLUSTER_MAX_BS)
				min_pages = SWAP_CLUSTER_MAX_BS;
			if (min_pages > 128)
				min_pages = 128;
			zone->pages_min = min_pages;
		} else {
			/* if it's a lowmem zone, reserve a number of pages
			 * proportionate to the zone's size.
			 */
			zone->pages_min = (pages_min * zone->present_pages) /
			                   lowmem_pages;
		}

		/*
		 * When interpreting these watermarks, just keep in mind that:
		 * zone->pages_min == (zone->pages_min * 4) / 4;
		 */
		zone->pages_low = (zone->pages_min * 5) / 4;
		zone->pages_high = (zone->pages_min * 6) / 4;
		spin_unlock_irqrestore(&zone->lru_lock, flags);
	}
}

/*
 * setup_per_zone_lowmem_reserve - called whenever
 *      sysctl_lower_zone_reserve_ration changes. Ensure that each zone
 *      has a correct pages resrved value, so an adequate number of
 *      pages are left in the zone after a successful __alloc_pages().
 */
static void setup_per_zone_lowmem_reserve_bs(void)
{
	struct pglist_data_bs *pgdat;
	int j, idx;

	for_each_pgdat_bs(pgdat) {
		for (j = 0; j < MAX_NR_ZONES_BS; j++) {
			struct zone_bs *zone = pgdat->node_zones + j;
			unsigned long present_pages = zone->present_pages;

			zone->lowmem_reserve[j] = 0;

			for (idx = j-1; idx >= 0; idx--) {
				struct zone_bs *lower_zone;

				if (sysctl_lowmem_reserve_ratio_bs[idx] < 1)
					sysctl_lowmem_reserve_ratio_bs[idx] = 1;

				lower_zone = pgdat->node_zones + idx;
				lower_zone->lowmem_reserve[j] = present_pages / 
					sysctl_lowmem_reserve_ratio_bs[idx];
				present_pages += lower_zone->present_pages;
			}
		}
	}
}

static unsigned int nr_free_zone_pages_bs(int offset)
{
	/* Just pick one node, since fallback list is circular */
	pg_data_t_bs *pgdat = NODE_DATA_BS(0);
	unsigned int sum = 0;

	struct zonelist_bs *zonelist = pgdat->node_zonelists + offset;
	struct zone_bs **zonep = zonelist->zones;
	struct zone_bs *zone;

	for (zone = *zonep++; zone; zone = *zonep++) {
		unsigned long size = zone->present_pages;
		unsigned long high = zone->pages_high;
		if (size > high)
			sum += size - high;
	}
	return sum;
}

/*
 * Amount of free RAM allocatable within ZONE_DMA and ZONE_NORMAL
 */
unsigned int nr_free_buffer_pages_bs(void)
{
	return nr_free_zone_pages_bs(GFP_USER_BS & GFP_ZONEMASK_BS);
}

/*
 * Amount of free RAM allocation within zones
 */
unsigned int nr_free_pagecache_pages_bs(void)
{
	return nr_free_zone_pages_bs(GFP_HIGHUSER_BS & GFP_ZONEMASK_BS);
}
EXPORT_SYMBOL_GPL(nr_free_pagecache_pages_bs);

/*
 * Initialise min_free_kbytes.
 *
 * For small machines we want it small (128k min).  For large machines
 * we want it large (64MB max).  But it is not linear, because network
 * bandwidth does not increase linearly with machine size.  We use
 *
 *      min_free_kbytes = 4 * sqrt(lowmem_kbytes), for better accuracy:
 *      min_free_kbytes = sqrt(lowmem_kbytes * 16)
 *
 * which yields
 *
 * 16MB:        512k
 * 32MB:        724k
 * 64MB:        1024k
 * 128MB:       1448k
 * 256MB:       2048k
 * 512MB:       2896k
 * 1024MB:      4096k
 * 2048MB:      5792k
 * 4096MB:      8192k
 * 8192MB:      11584k
 * 16384MB:     16384k
 */
static int __init init_per_zone_pages_min_bs(void)
{
	unsigned long lowmem_kbytes;

	lowmem_kbytes = nr_free_buffer_pages_bs() * (PAGE_SIZE_BS >> 10);

	min_free_kbytes_bs = int_sqrt(lowmem_kbytes * 16);
	if (min_free_kbytes_bs < 128)
		min_free_kbytes_bs = 128;
	if (min_free_kbytes_bs > 65536)
		min_free_kbytes_bs = 65536;
	setup_per_zone_pages_min_bs();
	setup_per_zone_lowmem_reserve_bs();
	return 0;
}
module_initcall_bs(init_per_zone_pages_min_bs);

__initdata int hashdist_bs = HASHDIST_DEFAULT_BS;

#ifdef CONFIG_NUMA
static int __init set_hashdist_bs(char *str)
{
	if (!str)
		return 0;
	hashdist_bs = simple_strtoul(str, &str, 0);
	return 1;
}
__setup_bs("hashdist_bs=", set_hashdist_bs);
#endif

/*
 * allocate a large system hash table from bootmem
 * - it is assumed that the hash table must contain an exact power-of-2
 *   quantity of entries
 * - limit is the number of hash buckets, not the total allocation size
 */
void *__init alloc_large_system_hash_bs(const char *tablename,
				unsigned long bucketsize,
				unsigned long numentries,
				int scale,
				int flags,
				unsigned int *_hash_shift,
				unsigned int *_hash_mask,
				unsigned long limit)
{
	unsigned long long max = limit;
	unsigned long log2qty, size;
	void *table = NULL;

	/* alloc the kernel cmdline to have a say */
	if (!numentries) {
		/* round applicable memory size up to nearest megabyte */
		numentries = (flags & HASH_HIGHMEM_BS) ? 
				nr_all_pages_bs : nr_kernel_pages_bs;
		numentries += (1UL << (20 - PAGE_SHIFT_BS)) - 1;
		numentries >>= 20 - PAGE_SHIFT_BS;
		numentries <<= 20 - PAGE_SHIFT_BS;

		/* limit to 1 bucket per 2^scale bytes of low memory */
		if (scale > PAGE_SHIFT_BS)
			numentries >>= (scale - PAGE_SHIFT_BS);
		else
			numentries <<= (PAGE_SHIFT_BS - scale);
	}

	/* limit allocation size to 1/16 total memory by default */
	if (max == 0) {
		max = ((unsigned long long)nr_all_pages_bs << 
							PAGE_SHIFT_BS) >> 4;
		do_div(max, bucketsize);
	}

	if (numentries > max)
		numentries = max;

	log2qty = long_log2_bs(numentries);

	do {
		size = bucketsize << log2qty;
		if (flags & HASH_EARLY_BS)
			table = alloc_bootmem_bs(size);
		else if (hashdist_bs)
			table = __vmalloc_bs(size, 
					GFP_ATOMIC_BS, PAGE_KERNEL_BS);
		else {
			unsigned long order;

			for (order = 0; ((1UL << order) << 
					PAGE_SHIFT_BS) < size; order++)
				;
			table = (void *)__get_free_pages_bs(GFP_ATOMIC_BS, 
									order);
		}
	} while (!table && size > PAGE_SIZE_BS && --log2qty);

	if (!table)
		panic("Failed to allocate %s hash table\n", tablename);

	printk("%s hash table entries: %d (order: %d, %lu bytes)\n",
		tablename,
		(1U << log2qty),
		long_log2_bs(size) - PAGE_SHIFT_BS,
		size);

	if (_hash_shift)
		*_hash_shift = log2qty;
	if (_hash_mask)
		*_hash_mask = (1 << log2qty) - 1;

	return table;
}

void si_meminfo_bs(struct sysinfo_bs *val)
{
	val->totalram = totalram_pages_bs;
	val->sharedram = 0;
	val->freeram = nr_free_pages_bs();
	val->bufferram = 0;
#ifdef CONFIG_HIGHMEM_BS
	val->totalhigh = totalhigh_pages_bs;
	val->freehigh = nr_free_highpages_bs();
#else
	val->totalhigh = 0;
	val->freehigh = 0;
#endif
	val->mem_unit = PAGE_SIZE_BS;
}
EXPORT_SYMBOL_GPL(si_meminfo_bs);

/* FIXME: Conver between Common page and page_bs */

void PAGE_TO_PAGE_BS(struct page *page, struct page_bs *page_bs)
{
	if (!page)
		page_bs = NULL;
	if (!page_bs)
		return;
	page_bs->flags = page->flags;
	page_bs->_count = page->_refcount;
	page_bs->_mapcount = page->_mapcount;
	page_bs->private = page->private;
	page_bs->mapping = page->mapping;
#if defined(WANT_PAGE_VIRTUAL)
	page_bs->virtual = page->virtual;
#endif
	list_replace(&page->lru, &page_bs->lru);
}
EXPORT_SYMBOL_GPL(PAGE_TO_PAGE_BS);

void PAGE_BS_TO_PAGE(struct page *page, struct page_bs *page_bs)
{
	if (!page_bs) {
		page = NULL;
		return;
	}
	if (!page)
		return;

	page->flags = page_bs->flags;
	page->_refcount = page_bs->_count;
	page->_mapcount = page_bs->_mapcount;
	page->private = page_bs->private;
	page->mapping = page_bs->mapping;
#if defined(WANT_PAGE_VIRTUAL)
	page->virtual = page_bs->virtual;
#endif
	list_replace(&page_bs->lru, &page->lru);
}
EXPORT_SYMBOL_GPL(PAGE_BS_TO_PAGE);
