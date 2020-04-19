/*
 * mm/memory.c
 *
 * (C) 2020.04.14 BuddyZhang1 <buddy.zhang@aliyun.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/kernel.h>
#include "biscuitos/mm.h"

#ifndef CONFIG_DISCONTIGMEM
/* use the per-pgdat data instead for discontigmem - mbligh */
unsigned long max_mapnr_bs;
struct page_bs *mem_map_bs;

EXPORT_SYMBOL_GPL(max_mapnr_bs);
EXPORT_SYMBOL_GPL(mem_map_bs);
#endif

/*
 * A number of key systems in x86 including ioremap() relay on the assumption
 * that high_memory defines the upper bound on direct map memory, then end
 * of ZONE_NORMAL. Under CONFIG_DISCONTIG this means that max_low_pfn and
 * highstart_pfn mut be the same; there must be no gap between ZONE_NORMAL
 * and ZONE_HIGHMEM.
 */
void *high_memory_bs;
EXPORT_SYMBOL_GPL(high_memory_bs);
