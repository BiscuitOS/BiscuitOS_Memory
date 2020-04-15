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
#include <linux/string.h>
#include "biscuitos/kernel.h"
#include "biscuitos/mm.h"
#include "biscuitos/bootmem.h"
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
static unsigned long __init init_bootmem_core_bs(pg_data_t *pgdat,
	unsigned long mapstart, unsigned long start, unsigned long end)
{
	bootmem_data_t *bdata = pgdat->bdata;
	unsigned long mapsize = ((end - start)+7)/8;

	pgdat->pgdat_next = pgdat_list_bs;
	pgdat_list_bs = pgdat;

	mapsize = (mapsize + (sizeof(long) - 1UL)) & ~(sizeof(long) - 1UL);
	bdata->node_bootmem_map = phys_to_virt_bs(mapstart << PAGE_SHIFT);
	bdata->node_boot_start = (start << PAGE_SHIFT);
	bdata->node_low_pfn = end;

	/*
	 * Initially all pages are reserved - setup_arch() has to
	 * register free RAM area explicitly.
	 */
	memset(bdata->node_bootmem_map, 0xff, mapsize);

	return mapsize;
}

static void __init free_bootmem_core_bs(bootmem_data_t *bdata,
				unsigned long addr, unsigned long size)
{
	unsigned long i;
	unsigned long start;
	/*
	 * round down end of usable mem, partially free pages are
	 * considered reserved.
	 */
	unsigned long sidx;
	unsigned long eidx = (addr + size - bdata->node_boot_start)/PAGE_SIZE;
	unsigned long end = (addr + size)/PAGE_SIZE;

	BUG_ON(!size);
	BUG_ON(end > bdata->node_low_pfn);

	if (addr < bdata->last_success)
		bdata->last_success = addr;

	/*
	 * Round up the beginning of the address.
	 */
	start = (addr + PAGE_SIZE-1) / PAGE_SIZE;
	sidx = start - (bdata->node_boot_start/PAGE_SIZE);

	for (i = sidx; i < eidx; i++) {
		if (unlikely(!test_and_clear_bit(i, bdata->node_bootmem_map)))
			BUG();
	}
}

/*
 * Marks a particular physical memory range as unallocatable. Usable RAM
 * might be used for boot-time allocations - or it might get added
 * to the free page pool later on.
 */
static void __init reserve_bootmem_core_bs(bootmem_data_t *bdata, 
				unsigned long addr, unsigned long size)
{
	unsigned long i;
	/*
	 * round up, partially reserved pages are considered
	 * fully reserved.
	 */
	unsigned long sidx = (addr - bdata->node_boot_start)/PAGE_SIZE;
	unsigned long eidx = (addr + size - bdata->node_boot_start +
					PAGE_SIZE-1)/PAGE_SIZE;
	unsigned long end = (addr + size + PAGE_SIZE-1)/PAGE_SIZE;

	BUG_ON(!size);
	BUG_ON(sidx >= eidx);
	BUG_ON((addr >> PAGE_SHIFT) >= bdata->node_low_pfn);
	BUG_ON(end > bdata->node_low_pfn);

	for (i = sidx; i < eidx; i++)
		if (test_and_set_bit(i, bdata->node_bootmem_map)) {
			printk("hm, page %#lx reserved twice.\n", i*PAGE_SIZE);
		}
}

unsigned long __init
init_bootmem_node_bs(pg_data_t *pgdat, unsigned long freepfn,
			unsigned long startpfn, unsigned long endpfn)
{
	return (init_bootmem_core_bs(pgdat, freepfn, startpfn, endpfn));
}

void __init free_bootmem_node_bs(pg_data_t *pgdat, unsigned long physaddr,
						unsigned long size)
{
	free_bootmem_core_bs(pgdat->bdata, physaddr, size);
}

void __init reserve_bootmem_node_bs(pg_data_t *pgdat, unsigned long physaddr,
					unsigned long size)
{
	reserve_bootmem_core_bs(pgdat->bdata, physaddr, size);
}
