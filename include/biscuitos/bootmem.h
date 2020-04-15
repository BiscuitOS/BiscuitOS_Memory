#ifndef _BISCUITOS_BOOTMEM_H
#define _BISCUITOS_BOOTMEM_H

#include "biscuitos/mmzone.h"
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
typedef struct bootmem_data {
	unsigned long node_boot_start;
	unsigned long node_low_pfn;
	void *node_bootmem_map;
	unsigned long last_offset;
	unsigned long last_pos;
	unsigned long last_success;	/* Previous allocation point.
					 * To speed up searching */
} bootmem_data_t;

extern unsigned long __init bootmem_bootmap_pages_bs(unsigned long);
extern unsigned long __init init_bootmem_node_bs(pg_data_t *,
				unsigned long, unsigned long, unsigned long);
extern void __init free_bootmem_node_bs(pg_data_t *, 
					unsigned long, unsigned long);
extern void __init reserve_bootmem_node_bs(pg_data_t *, unsigned long,
					unsigned long);

#endif
