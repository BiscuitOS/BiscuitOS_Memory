#ifndef _BISCUITOS_BOOTMEM_H
#define _BISCUITOS_BOOTMEM_H

#include "biscuitos/mmzone.h"
#include "biscuitos/cache.h"
#include "asm-generated/dma.h"

/*
 * simple boot-time physical memory area allocator.
 */

extern unsigned long max_low_pfn_bs;
extern unsigned long min_low_pfn_bs;

/*
 * higest page
 */
extern unsigned long max_pfn_bs;

/*
 * node_bootmem_map is a map pointer - the bits represent all physical
 * memory pages (including holes) on the node.
 */
typedef struct bootmem_data_bs {
	unsigned long node_boot_start;
	unsigned long node_low_pfn;
	void *node_bootmem_map;
	unsigned long last_offset;
	unsigned long last_pos;
	unsigned long last_success;	/* Previous allocation point.
					 * To speed up searching */
} bootmem_data_t_bs;

extern unsigned long __init bootmem_bootmap_pages_bs(unsigned long);
extern unsigned long __init init_bootmem_node_bs(pg_data_t_bs *,
				unsigned long, unsigned long, unsigned long);
extern void __init free_bootmem_node_bs(pg_data_t_bs *, 
					unsigned long, unsigned long);
extern void __init reserve_bootmem_node_bs(pg_data_t_bs *, unsigned long,
					unsigned long);
extern void * __init __alloc_bootmem_bs(unsigned long, 
					unsigned long, unsigned long);
extern void * __init __alloc_bootmem_node_bs(pg_data_t_bs *, 
	unsigned long, unsigned long, unsigned long);

#define alloc_bootmem_node_bs(pgdat, x) \
	__alloc_bootmem_node_bs((pgdat), (x), SMP_CACHE_BYTES_BS, \
						__pa_bs(MAX_DMA_ADDRESS_BS))
#define alloc_bootmem_low_pages_bs(x) \
	__alloc_bootmem_bs((x), PAGE_SIZE_BS, 0)

#endif
