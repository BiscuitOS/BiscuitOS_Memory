/*
 * bootmem
 *
 * (C) 2020.04.14 BuddyZhang1 <buddy.zhang@aliyun.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/kernel.h>
#include "biscuitos/mm.h"
#include "biscuitos/mmzone.h"

/*
 * Access to this subsystem has to be serialized externally. (this is
 * true for the boot process anyway)
 */
unsigned long max_low_pfn_bs;
unsigned long min_low_pfn_bs;
unsigned long max_pfn_bs;

EXPORT_SYMBOL_GPL(max_pfn_bs);		/* This is exported so
					 * dma_get_required_mask(), which uses
					 * it, can be an inline function */

/* return the number of _pages_ that will be allocated for the boot bitmap */
unsigned long __init bootmem_bootmap_pages_bs(unsigned long pages)
{
	unsigned long mapsize;

	mapsize = (pages+7)/8;
	mapsize = (mapsize + ~PAGE_MASK) & PAGE_MASK;
	mapsize >>= PAGE_SHIFT;

	return mapsize;
}

/*
 * Called once to set up the allocator itself
 */

unsigned long __init
init_bootmem_node_bs(pg_data_t *pgdat, unsigned long freepfn,
			unsigned long startpfn, unsigned long endpfn)
{
	return 0;
}
