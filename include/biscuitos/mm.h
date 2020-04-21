#ifndef _BISCUITOS_MM_H
#define _BISCUITOS_MM_H

#include "biscuitos/mmzone.h"
#include "biscuitos/gfp.h"
#include "asm-generated/memory.h"
#include "asm-generated/pgtable.h"
#include "asm-generated/page.h"

#ifdef ARCH_HAS_ATOMIC_UNSIGNED
typedef unsigned page_flags_t_bs;
#else
typedef unsigned long page_flags_t_bs;
#endif

/*
 * The zone field is never updated after free_area_init_core()
 * sets it, so none of the operations on it need to be atomic.
 * We'll have up to (MAX_NUMNODES * MAX_NR_ZONES) zones total,
 * so we use (MAX_NODES_SHIFT + MAX_ZONES_SHIFT) here to get enough bits.
 */
#define NODEZONE_SHIFT_BS	(sizeof(page_flags_t_bs)*8 - \
				 MAX_NODES_SHIFT_BS - MAX_ZONES_SHIFT_BS)
#define NODEZONE_BS(node, zone)	((node << ZONES_SHIFT_BS) | zone)

/*
 * Each physical page in the system has a struct page associated with
 * it to keep track of whatever it is we are using the page for at the
 * moment. Note that we have no way to track which tasks are using
 * a page.
 */
struct page_bs {
	page_flags_t_bs flags;          /* Atomic flags, some possibly
					 * updated asynchronously */
	atomic_t _count;                /* Usage count, see below. */
	atomic_t _mapcount;             /* Count of ptes mapped in mms,
					 * to show when page is mapped
					 * & limit reverse map searches.
					 */
	unsigned long private;          /* Mapping-private opaque data:
					 * usually used for buffer_heads
					 * if PagePrivate set; used for
					 * swp_entry_t if PageSwapCache
					 * When page is free, this indicates
					 * order in the buddy system.
					 */
	pgoff_t index;                  /* Our offset within mapping. */
	struct list_head lru;           /* Pageout list, eg. active_list
					 * protected by zone->lru_lock !
					 */
	/*
	 * On machines where all RAM is mapped into kernel address space,
	 * we can simply calculate the virtual address. On machines with
	 * highmem some memory is mapped into kernel virtual memory
	 * dynamically, so we need a place to store that address.
	 * Note that this field could be 16 bits on x86 ... ;)
	 *
	 * Architectures with slow multiplication can define
	 * WANT_PAGE_VIRTUAL in asm/page.h
	 */
#if defined(WANT_PAGE_VIRTUAL)
	void *virtual;                  /* Kernel virtual address (NULL if
					   not kmapped, ie. highmem) */
#endif /* WANT_PAGE_VIRTUAL */
};

#ifndef CONFIG_DISCONTIGMEM
/* The array of struct pages - for discontigmen use pgdat->lmem_map */
extern struct page_bs *mem_map_bs;
#endif

static inline void set_page_zone_bs(struct page_bs *page, 
					unsigned long nodezone_num)
{
	page->flags &= ~(~0UL << NODEZONE_SHIFT_BS);
	page->flags |= nodezone_num << NODEZONE_SHIFT_BS;
}

#define set_page_count_bs(p, v)		atomic_set(&(p)->_count, v - 1)

/*      
 * The atomic page->_mapcount, like _count, starts from -1:
 * so that transitions both from it and to it can be tracked,
 * using atomic_inc_and_test and atomic_add_negative(-1). 
 */
static inline void reset_page_mapcount_bs(struct page_bs *page)
{
	atomic_set(&(page)->_mapcount, -1);
}

static inline void page_alloc_init_bs(void)
{
}

struct pglist_data_bs;
extern void *high_memory_bs;
extern pgprot_t_bs protection_map_bs[16];
extern void __init free_area_init_node_bs(int nid, struct pglist_data_bs *pgdat,
		unsigned long *zones_size, unsigned long node_start_pfn,
		unsigned long *zhole_size);
extern void __init page_address_init_bs(void);

#endif
