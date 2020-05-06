#ifndef _BISCUITOS_MMZONE_H
#define _BISCUITOS_MMZONE_H

#include <linux/wait.h>
#include <linux/list.h>
#include "biscuitos/cache.h"
#include "asm-generated/arch.h"
#include "asm-generated/types.h"

/* Free memory management - zoned buddy allocator.  */
#ifndef CONFIG_FORCE_MAX_ZONEORDER
#define MAX_ORDER_BS	11
#else   
#define MAX_ORDER_BS	CONFIG_FORCE_MAX_ZONEORDER
#endif

/*
 * When a memory allocation must conform to specific limitations (such
 * as being suitable for DMA) the caller will pass in hints to the
 * allocator in the gfp_mask, in the zone modifier bits.  These bits
 * are used to select a priority ordered list of memory zones which
 * match the requested limits.  GFP_ZONEMASK defines which bits within
 * the gfp_mask should be considered as zone modifiers.  Each valid
 * combination of the zone modifier bits has a corresponding list
 * of zones (in node_zonelists).  Thus for two zone modifiers there
 * will be a maximum of 4 (2 ** 2) zonelists, for 3 modifiers there will
 * be 8 (2 ** 3) zonelists.  GFP_ZONETYPES defines the number of possible
 * combinations of zone modifiers in "zone modifier space".
 *
 * NOTE! Make sure this matches the zones in <linux/gfp.h>
 */
#define GFP_ZONEMASK_BS		0x07
#define GFP_ZONETYPES_BS	5

#ifdef CONFIG_NUMA
#define zone_pcp_bs(__z, __cpu) ((__z)->pageset[(__cpu)])
#else
#define zone_pcp_bs(__z, __cpu) (&(__z)->pageset[(__cpu)])
#endif

#define ZONE_DMA_BS		0
#define ZONE_DMA32_BS		1
#define ZONE_NORMAL_BS		2
#define ZONE_HIGHMEM_BS		3
                
#define MAX_NR_ZONES_BS		4	/* Sync this with ZONES_SHIFT */
#define ZONES_SHIFT_BS		2	/* ceil(log2(MAX_NR_ZONES)) */

/*              
 * The "priority" of VM scanning is how much of the queues we will scan in one
 * go. A value of 12 for DEF_PRIORITY implies that we will scan 1/4096th of the
 * queues ("queue_length >> 12") during an aging round.
 */             
#define DEF_PRIORITY_BS		12


struct free_area_bs {
	struct list_head	free_list;
	unsigned long		nr_free;
};

/*
 * zone->lock and zone->lru_lock are two of the hottest locks in the kernel.
 * So add a wild amount of padding here to ensure that they fall into separate
 * cachelines.  There are very few zone structures in the machine, so space
 * consumption is not a concern here.
 */
#if defined(CONFIG_SMP)
struct zone_padding_bs {
	char x[0];
} ____cacheline_maxaligned_in_smp_bs; 
#define ZONE_PADDING_BS(name)	struct zone_padding_bs name;
#else
#define ZONE_PADDING_BS(name)
#endif

struct per_cpu_pages_bs {
	int count;		/* number of pages in the list */
	int low;		/* low watermark, refill needed */
	int high;		/* high watermark, emptying needed */
	int batch;		/* chunk size for buddy add/remove */
	struct list_head list;	/* the list of pages */
};

struct per_cpu_pageset_bs {
	struct per_cpu_pages_bs pcp[2];	/* 0: hot.  1: cold */
#ifdef CONFIG_NUMA
	unsigned long numa_hit;		/* allocated in intended node */
	unsigned long numa_miss;	/* allocated in non intended node */
	unsigned long numa_foreign;	/* was intended here, hit elsewhere */
	unsigned long interleave_hit;	/* interleaver prefered this zone */
	unsigned long local_node;	/* allocation from local node */
	unsigned long other_node;	/* allocation from other node */
#endif
} ____cacheline_aligned_in_smp_bs;

/*
 * On machines where it is needed (eg PCs) we divide physical memory
 * into multiple physical zones. On a PC we have 4 zones:
 *
 * ZONE_DMA	  < 16 MB	ISA DMA capable memory
 * ZONE_DMA32	     0 MB 	Empty
 * ZONE_NORMAL	16-896 MB	direct mapped by the kernel
 * ZONE_HIGHMEM	 > 896 MB	only page cache and user processes
 */

struct zone_bs {
	/* Fields commonly accessed by the page allocator */
	unsigned long		free_pages;
	unsigned long		pages_min, pages_low, pages_high;
	/*
	 * We don't know if the memory that we're going to allocate will 
	 * be freeable or/and it will be released eventually, so to avoid
	 * totally wasting several GB of ram we must reserve some of the
	 * lower zone memory (otherwise we risk to run OOM on the lower 
	 * zones despite there's tons of freeable ram on the higher zones).
	 * This array is recalculated at runtime if the 
	 * sysctl_lowmem_reserve_ratio sysctl changes.
	 */     
	unsigned long		lowmem_reserve[MAX_NR_ZONES_BS];

#ifdef CONFIG_NUMA
	struct per_cpu_pageset_bs	*pageset[NR_CPUS_BS];
#else
	struct per_cpu_pageset_bs	pageset[NR_CPUS_BS];
#endif

	/*
	 * free areas of different sizes
	 */
	spinlock_t		lock;
	struct free_area_bs	free_area[MAX_ORDER_BS];

	ZONE_PADDING_BS(_pad1_)

	/* Fields commonly accessed by the page reclaim scanner */
	spinlock_t		lru_lock;
	struct list_head	active_list;
	struct list_head	inactive_list;
	unsigned long		nr_scan_active;
	unsigned long		nr_scan_inactive;
	unsigned long		nr_active;
	unsigned long		nr_inactive;
	unsigned long		pages_scanned;	    /* since last reclaim */
	int			all_unreclaimable;  /* All pages pinned */

	/*
	 * Does the allocator try to reclaim pages from the zone as soon
	 * as it fails a watermark_ok() in __alloc_pages?
	 */
	int			reclaim_pages;
	/* A count of how many reclaimers are scanning this zone */
	atomic_t		reclaim_in_progress;

	/*
	 * prev_priority holds the scanning priority for this zone.  It is
	 * defined as the scanning priority at which we achieved our reclaim
	 * target at the previous try_to_free_pages() or balance_pgdat()
	 * invokation.
	 *
	 * We use prev_priority as a measure of how much stress page reclaim is
	 * under - it drives the swappiness decision: whether to unmap mapped
	 * pages.
	 *
	 * temp_priority is used to remember the scanning priority at which
	 * this zone was successfully refilled to free_pages == pages_high.
	 *
	 * Access to both these fields is quite racy even on uniprocessor.  But
	 * it is expected to average out OK.
	 */
	int temp_priority;
	int prev_priority;

	ZONE_PADDING_BS(_pad2_)
	/* Rarely used or read-mostly fields */

	/*
	 * wait_table           -- the array holding the hash table
	 * wait_table_size      -- the size of the hash table array
	 * wait_table_bits      -- wait_table_size == (1 << wait_table_bits)
	 *
	 * The purpose of all these is to keep track of the people
	 * waiting for a page to become available and make them
	 * runnable again when possible. The trouble is that this
	 * consumes a lot of space, especially when so few things
	 * wait on pages at a given time. So instead of using
	 * per-page waitqueues, we use a waitqueue hash table.
	 *
	 * The bucket discipline is to sleep on the same queue when
	 * colliding and wake all in that wait queue when removing.
	 * When something wakes, it must check to be sure its page is
	 * truly available, a la thundering herd. The cost of a
	 * collision is great, but given the expected load of the
	 * table, they should be so rare as to be outweighed by the
	 * benefits from the saved space.
	 *
	 * __wait_on_page_locked() and unlock_page() in mm/filemap.c, are the
	 * primary users of these fields, and in mm/page_alloc.c
	 * free_area_init_core() performs the initialization of them.
	 */
	wait_queue_head_t	*wait_table;
	unsigned long		wait_table_size;
	unsigned long		wait_table_bits;

	/*
	 * Discontig memory support fields.
	 */
	struct pglist_data_bs	*zone_pgdat;
	struct page_bs		*zone_mem_map;
	/* zone_start_pfn == zone_start_paddr >> PAGE_SHIFT */
	unsigned long		zone_start_pfn;

	/*
	 * zone_start_pfn, spanned_pages and present_pages are all
	 * protected by span_seqlock.  It is a seqlock because it has
	 * to be read outside of zone->lock, and it is done in the main
	 * allocator path.  But, it is written quite infrequently.
	 *
	 * The lock is declared along with zone->lock because it is
	 * frequently read in proximity to zone->lock.  It's good to
	 * give them a chance of being in the same cacheline.
	 */
	unsigned long		spanned_pages;
	unsigned long		present_pages;

	/*
	 * rarely used fields:
	 */
	char                    *name;
} ____cacheline_maxaligned_in_smp_bs;

/*
 * One allocation request operates on a zonelist. A zonelist
 * is a list of zones, the first one is the 'goal' of the
 * allocation, the other zones are fallback zones, in decreasing
 * priority.
 *
 * Right now a zonelist takes up less than a cacheline. We never
 * modify it apart from boot-up, and only a few indices are used,
 * so despite the zonelist table being relatively big, the cache
 * footprint of this construct is very small.
 */
struct zonelist_bs {
	struct zone_bs *zones[MAX_NUMNODES_BS * MAX_NR_ZONES_BS + 1]; 
};

struct task_struct;

struct bootmem_data_bs;
typedef struct pglist_data_bs {
	struct zone_bs node_zones[MAX_NR_ZONES_BS];
	struct zonelist_bs node_zonelists[GFP_ZONETYPES_BS];
	int nr_zones;
#ifdef CONFIG_FLAT_NODE_MEM_MAP
	struct page_bs *node_mem_map;
#endif
	struct bootmem_data_bs *bdata;
	unsigned long node_start_pfn;
	unsigned long node_present_pages; /* total number of physical pages */
	unsigned long node_spanned_pages; /* total size of physical page
					     range, including holes */
	int node_id;
	struct pglist_data_bs *pgdat_next;
	wait_queue_head_t kswapd_wait;
	int kswapd_max_order;
	struct task_struct *kswapd;
} pg_data_t_bs;

extern struct pglist_data_bs contig_page_data_bs;
extern struct pglist_data_bs *pgdat_list_bs;

#define NODE_DATA_BS(nid)	(&contig_page_data_bs)
#define MAX_NODES_SHIFT_BS	1

#define node_present_pages_bs(nid) (NODE_DATA_BS(nid)->node_present_pages)
#define node_spanned_pages_bs(nid) (NODE_DATA_BS(nid)->node_spanned_pages)
#ifdef CONFIG_FLAT_NODE_MEM_MAP
#define pgdat_page_nr_bs(pgdat, pagenr)	((pgdat)->node_mem_map + (pagenr))
#else
#define pgdat_page_nr_bs(pgdat, pagenr)	pfn_to_page_bs((pgdat)->node_start_pfn + (pagenr))
#endif
#define nid_page_nr_bs(nid, pagenr) 	pgdat_page_nr_bs(NODE_DATA_BS(nid),(pagenr))

#if BITS_PER_LONG_BS == 32
/*
 * with 32 bit page->flags field, we reserve 9 bits for node/zone info.
 * there are 4 zones (3 bits) and this leaves 9-3=6 bits for nodes.
 */
#define FLAGS_RESERVED_BS	9

#elif BITS_PER_LONG_BS == 64
/*
 * with 64 bit flags field, there's plenty of room.
 */
#define FLAGS_RESERVED_BS	32

#else

#error BITS_PER_LONG_BS not defined

#endif

/* There are currently 3 zones: DMA, Normal & Highmem, thus we need 2 bits */
#define MAX_ZONES_SHIFT_BS	2

#ifdef CONFIG_NODES_SPAN_OTHER_NODES
#define early_pfn_in_nid_bs(pfn, nid)	(early_pfn_to_nid_bs(pfn) == (nid))
#else
#define early_pfn_in_nid_bs(pfn, nid)	(1)
#endif

#ifndef early_pfn_valid_bs
#define early_pfn_valid_bs(pfn)	(1)
#endif

/*
 * zone_idx() returns 0 for the ZONE_DMA zone, 1 for the ZONE_NORMAL zone, etc
 */
#define zone_idx_bs(zone)	((zone - (zone)->zone_pgdat->node_zones))

#define pfn_to_section_nr_bs(pfn) ((pfn) >> PFN_SECTION_SHIFT_BS)
#define section_nr_to_pfn_bs(sec) ((sec) << PFN_SECTION_SHIFT_BS)

/*
 * for_each_pgdat - helper macro to iterate over all nodes
 * @pgdat - pointer to a pg_data_t variable
 *
 * Meant to help with common loops of the from
 * pgdat = pgdat_list;
 * while (pgdat) {
 * 	...
 *	pgdat = pgdat->pgdat_next;
 * }
 */
#define for_each_pgdat_bs(pgdat) \
	for (pgdat = pgdat_list_bs; pgdat; pgdat = pgdat->pgdat_next)

static inline int is_highmem_idx_bs(int idx)
{
	return (idx == ZONE_HIGHMEM_BS);
}

static inline int is_normal_idx_bs(int idx)
{
	return (idx == ZONE_NORMAL_BS);
}

/**
 * is_highmem - helper function to quickly check if a struct zone is a 
 *              highmem zone or not.  This is an attempt to keep references
 *              to ZONE_{DMA/NORMAL/HIGHMEM/etc} in general code to a minimum.
 * @zone - pointer to struct zone variable
 */
static inline int is_highmem_bs(struct zone_bs *zone)
{
	return zone == zone->zone_pgdat->node_zones + ZONE_HIGHMEM_BS;
}

static inline int is_normal_bs(struct zone_bs *zone)
{
	return zone == zone->zone_pgdat->node_zones + ZONE_NORMAL_BS;
}

extern void __init build_all_zonelists_bs(void);

/*
 * next_zone - helper magic for for_each_zone()
 * Thanks to William Lee Irwin III for this piece of ingenuity.
 */
static inline struct zone_bs *next_zone_bs(struct zone_bs *zone)
{
	pg_data_t_bs *pgdat = zone->zone_pgdat;

	if (zone < pgdat->node_zones + MAX_NR_ZONES_BS - 1)
		zone++;
	else if (pgdat->pgdat_next) {
		pgdat = pgdat->pgdat_next;
		zone = pgdat->node_zones;
	} else
		zone = NULL;

	return zone;
}

/**
 * for_each_zone - helper macro to iterate over all memory zones
 * @zone - pointer to struct zone variable
 *
 * The user only needs to declare the zone variable, for_each_zone
 * fills it in. This basically means for_each_zone() is an
 * easier to read version of this piece of code:
 *      
 * for (pgdat = pgdat_list; pgdat; pgdat = pgdat->node_next)
 *      for (i = 0; i < MAX_NR_ZONES; ++i) {
 *              struct zone * z = pgdat->node_zones + i;
 *              ...
 *      }
 * }
 */
#define for_each_zone_bs(zone)	\
	for (zone = pgdat_list_bs->node_zones; zone; zone = next_zone_bs(zone))

#define numa_node_id_bs()	(0)

extern int zone_watermark_ok_bs(struct zone_bs *z, int order, 
  unsigned long mark, int classzone_idx, int alloc_flags);
extern void wakeup_kswapd_bs(struct zone_bs *zone, int order);
#define sparse_init_bs()	do {} while (0)
#define sparse_index_init_bs(_sec, _nid)  do {} while (0)

#endif
