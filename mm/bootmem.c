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
#include "biscuitos/page-flags.h"
#include "asm-generated/types.h"
#include "internal.h"

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

#ifdef CONFIG_CRASH_DUMP
/*
 * If we have booted due to a crash, max_pfn will be a very low value. We need
 * to know the amount of memory that the previous kernel used.
 */
unsigned long saved_max_pfn_bs;
#endif

/* return the number of _pages_ that will be allocated for the boot bitmap */
unsigned long __init bootmem_bootmap_pages_bs(unsigned long pages)
{
	unsigned long mapsize;

	mapsize = (pages+7)/8;
	mapsize = (mapsize + ~PAGE_MASK_BS) & PAGE_MASK_BS;
	mapsize >>= PAGE_SHIFT_BS;

	return mapsize;
}

/*
 * Called once to set up the allocator itself
 */
static unsigned long __init init_bootmem_core_bs(pg_data_t_bs *pgdat,
	unsigned long mapstart, unsigned long start, unsigned long end)
{
	bootmem_data_t_bs *bdata = pgdat->bdata;
	unsigned long mapsize = ((end - start)+7)/8;

	pgdat->pgdat_next = pgdat_list_bs;
	pgdat_list_bs = pgdat;

	mapsize = ALIGN(mapsize, sizeof(long));
	bdata->node_bootmem_map = phys_to_virt_bs(mapstart << PAGE_SHIFT_BS);
	bdata->node_boot_start = (start << PAGE_SHIFT_BS);
	bdata->node_low_pfn = end;

	/*
	 * Initially all pages are reserved - setup_arch() has to
	 * register free RAM area explicitly.
	 */
	memset(bdata->node_bootmem_map, 0xff, mapsize);

	return mapsize;
}

static void __init free_bootmem_core_bs(bootmem_data_t_bs *bdata,
				unsigned long addr, unsigned long size)
{
	unsigned long i;
	unsigned long start;
	/*
	 * round down end of usable mem, partially free pages are
	 * considered reserved.
	 */
	unsigned long sidx;
	unsigned long eidx = (addr + size - 
				bdata->node_boot_start)/PAGE_SIZE_BS;
	unsigned long end = (addr + size)/PAGE_SIZE_BS;

	BUG_ON_BS(!size);
	BUG_ON_BS(end > bdata->node_low_pfn);

	if (addr < bdata->last_success)
		bdata->last_success = addr;

	/*
	 * Round up the beginning of the address.
	 */
	start = (addr + PAGE_SIZE_BS-1) / PAGE_SIZE_BS;
	sidx = start - (bdata->node_boot_start/PAGE_SIZE_BS);

	for (i = sidx; i < eidx; i++) {
		if (unlikely(!test_and_clear_bit(i, bdata->node_bootmem_map)))
			BUG_BS();
	}
}

/*
 * Marks a particular physical memory range as unallocatable. Usable RAM
 * might be used for boot-time allocations - or it might get added
 * to the free page pool later on.
 */
static void __init reserve_bootmem_core_bs(bootmem_data_t_bs *bdata, 
				unsigned long addr, unsigned long size)
{
	unsigned long i;
	/*
	 * round up, partially reserved pages are considered
	 * fully reserved.
	 */
	unsigned long sidx = (addr - bdata->node_boot_start)/PAGE_SIZE_BS;
	unsigned long eidx = (addr + size - bdata->node_boot_start +
					PAGE_SIZE_BS-1)/PAGE_SIZE_BS;
	unsigned long end = (addr + size + PAGE_SIZE_BS-1)/PAGE_SIZE_BS;

	BUG_ON_BS(!size);
	BUG_ON_BS(sidx >= eidx);
	BUG_ON_BS((addr >> PAGE_SHIFT_BS) >= bdata->node_low_pfn);
	BUG_ON_BS(end > bdata->node_low_pfn);

	for (i = sidx; i < eidx; i++)
		if (test_and_set_bit(i, bdata->node_bootmem_map)) {
			printk("hm, page %#lx reserved twice.\n", 
					i*PAGE_SIZE_BS);
		}
}

/*
 * We 'merge' subsequest allocation to save space. We might 'lose'
 * some fraction of a page if allocations cannot be satisfied due to
 * size constraints on boxes where is physical RAM space fragmentation -
 * in these cases (mostly large memory boxes) this is not a problem.
 *
 * On low memory boxes we get it right in 100% of the cases.
 *
 * alignment has to be a power of 2 value.
 *
 * NOTE: This function is _not_ reentrant.
 */
static void * __init
__alloc_bootmem_core_bs(struct bootmem_data_bs *bdata, unsigned long size,
		unsigned long align, unsigned long goal, unsigned long limit)
{
	unsigned long offset, remaining_size, areasize, preferred;
	unsigned long i, start = 0, incr, eidx, end_pfn = bdata->node_low_pfn;
	void *ret;

	if (!size) {
		printk("__alloc_bootmem_core(): zero-sized request\n");
		BUG_BS();
	}
	BUG_ON_BS(align & (align-1));

	if (limit && bdata->node_boot_start >= limit)
		return NULL;

        limit >>= PAGE_SHIFT_BS;
	if (limit && end_pfn > limit)
		end_pfn = limit;

	eidx = end_pfn - (bdata->node_boot_start >> PAGE_SHIFT);
	offset = 0;
	if (align &&
		(bdata->node_boot_start & (align - 1UL)) != 0)
		offset = (align - (bdata->node_boot_start & (align - 1UL)));
	offset >>= PAGE_SHIFT_BS;

	/*
	 * We try to allocate bootmem pages above 'goal'
	 * first, then we try to allocate lower pages.
	 */
	if (goal && (goal >= bdata->node_boot_start) &&
		((goal >> PAGE_SHIFT_BS) < end_pfn)) {
		preferred = goal - bdata->node_boot_start;

		if (bdata->last_success >= preferred)
			if (!limit || (limit && limit > bdata->last_success))
				preferred = bdata->last_success;
	} else
		preferred = 0;

	preferred = ALIGN(preferred, align) >> PAGE_SHIFT;
	preferred += offset;
	areasize = (size+PAGE_SIZE_BS-1)/PAGE_SIZE_BS;
	incr = align >> PAGE_SHIFT_BS ? : 1;

restart_scan:
	for (i = preferred; i < eidx; i += incr) {
		unsigned long j;

		i = find_next_zero_bit(bdata->node_bootmem_map, eidx, i);
		i = ALIGN(i, incr);
		if (test_bit(i, bdata->node_bootmem_map))
			continue;
		for (j = i + 1; j < i + areasize; ++j) {
			if (j >= eidx)
				goto fail_block;
			if (test_bit(j, bdata->node_bootmem_map))
				goto fail_block;
		}
		start = i;
		goto found;
	fail_block:
		i = ALIGN(j, incr);
	}

	if (preferred > offset) {
		preferred = offset;
		goto restart_scan;
	}
	return NULL;

found:
	bdata->last_success = start << PAGE_SHIFT_BS;
	BUG_ON_BS(start >= eidx);

	/*
	 * Is the next page of the previous allocation-end the start
	 * of this allocation's buffer? If yes then we can 'merge'
	 * the previous partial page with this allocation.
	 */
	if (align < PAGE_SIZE_BS &&
		bdata->last_offset && bdata->last_pos+1 == start) {
		offset = ALIGN(bdata->last_offset, align);
		BUG_ON_BS(offset > PAGE_SIZE_BS);
		remaining_size = PAGE_SIZE_BS - offset;
		if (size < remaining_size) {
			areasize = 0;
			/* last_pos unchanged */
			bdata->last_offset = offset+size;
			ret = phys_to_virt_bs(bdata->last_pos*PAGE_SIZE_BS +
					offset + bdata->node_boot_start);
		} else {
			remaining_size = size - remaining_size;
			areasize = (remaining_size+PAGE_SIZE_BS-1)/PAGE_SIZE_BS;
			ret = phys_to_virt_bs(bdata->last_pos*PAGE_SIZE_BS +
					offset + bdata->node_boot_start);
			bdata->last_pos = start+areasize-1;
			bdata->last_offset = remaining_size;
		}
		bdata->last_offset &= ~PAGE_MASK_BS;
	} else {
		bdata->last_pos = start + areasize - 1;
		bdata->last_offset = size & ~PAGE_MASK_BS;
		ret = phys_to_virt_bs(start * PAGE_SIZE_BS + 
					bdata->node_boot_start);
	}

	/*
	 * Reserve the area now:
	 */
	for (i = start; i < start+areasize; i++)
		if (unlikely(test_and_set_bit(i, bdata->node_bootmem_map)))
			BUG_BS();
	memset(ret, 0, size);
	return ret;
}

void __init free_bootmem_bs(unsigned long addr, unsigned long size)
{
	free_bootmem_core_bs(NODE_DATA_BS(0)->bdata, addr, size);
}

static unsigned long __init free_all_bootmem_core_bs(pg_data_t_bs *pgdat)
{
	struct page_bs *page;
	unsigned long pfn;
	bootmem_data_t_bs *bdata = pgdat->bdata;
	unsigned long i, count, total = 0;
	unsigned long idx;
	unsigned long *map;
	int gofast = 0;

	BUG_ON_BS(!bdata->node_bootmem_map);

	count = 0;
	/* first extant page of the node */
	pfn = bdata->node_boot_start >> PAGE_SHIFT_BS;
	idx = bdata->node_low_pfn - (bdata->node_boot_start >> PAGE_SHIFT_BS);
	map = bdata->node_bootmem_map;
	/* Check physaddr is O(LOG2(BITS_PER_LONG)) page aligned */
	if (bdata->node_boot_start == 0 ||
	    ffs(bdata->node_boot_start) - PAGE_SHIFT > ffs(BITS_PER_LONG_BS))
		gofast = 1;
	for (i = 0; i < idx; ) {
		unsigned long v = ~map[i / BITS_PER_LONG_BS];

		if (gofast && v == ~0UL) {
			int j, order;

			page = pfn_to_page_bs(pfn);
			count += BITS_PER_LONG_BS;
			__ClearPageReserved_bs(page);
			order = ffs(BITS_PER_LONG_BS) - 1;
			set_page_refs_bs(page, order);
			for (j = 1; j < BITS_PER_LONG_BS; j++) {
				if (j + 16 < BITS_PER_LONG_BS)
					prefetchw(page + j + 16);
				__ClearPageReserved_bs(page + j);
			}
			__free_pages_bs(page, order);
			i += BITS_PER_LONG_BS;
			page += BITS_PER_LONG_BS;
		} else if (v) {
			unsigned long m;

			page = pfn_to_page_bs(pfn);
			for (m = 1; m && i < idx; m <<= 1, page++, i++) {
				if (v & m) {
					count++;
					__ClearPageReserved_bs(page);
					set_page_refs_bs(page, 0);
					__free_page_bs(page);
				}
			}
		} else {
			i+= BITS_PER_LONG_BS;
		}
		pfn += BITS_PER_LONG_BS;
	}
	total += count;

	/*
	 * Now free the allocator bitmap itself, it's not
	 * needed anymore.
	 */
	page = virt_to_page_bs(bdata->node_bootmem_map);
	count = 0;
	for (i = 0; i < ((bdata->node_low_pfn - (bdata->node_boot_start >>
			PAGE_SHIFT_BS))/8 + PAGE_SIZE_BS-1)/PAGE_SIZE_BS;
						i++,page++) {
		count++;
		__ClearPageReserved_bs(page);
		set_page_count_bs(page, 1);
		__free_page_bs(page);
	}
	total += count;
	bdata->node_bootmem_map = NULL;

	return total;
}

unsigned long __init
init_bootmem_node_bs(pg_data_t_bs *pgdat, unsigned long freepfn,
			unsigned long startpfn, unsigned long endpfn)
{
	return (init_bootmem_core_bs(pgdat, freepfn, startpfn, endpfn));
}

void __init free_bootmem_node_bs(pg_data_t_bs *pgdat, unsigned long physaddr,
						unsigned long size)
{
	free_bootmem_core_bs(pgdat->bdata, physaddr, size);
}

void __init reserve_bootmem_node_bs(pg_data_t_bs *pgdat, unsigned long physaddr,
					unsigned long size)
{
	reserve_bootmem_core_bs(pgdat->bdata, physaddr, size);
}

void * __init __alloc_bootmem_limit_bs(unsigned long size, unsigned long align,
		unsigned long goal, unsigned long limit)
{
	pg_data_t_bs *pgdat = pgdat_list_bs;
	void *ptr;

	for_each_pgdat_bs(pgdat)
		if ((ptr = __alloc_bootmem_core_bs(pgdat->bdata, 
						size, align, goal, limit)))
			return (ptr);
	/*
	 * Whoops, we cannot satisfy the allocation request.
	 */
	printk(KERN_ALERT "bootmem alloc of %lu bytes failed!\n", size);
	panic("Out of memory");
	return NULL;
}

void *__init __alloc_bootmem_node_limit_bs(pg_data_t_bs *pgdat, 
	unsigned long size, unsigned long align, unsigned long goal,
	unsigned long limit)
{
	void *ptr;

	ptr = __alloc_bootmem_core_bs(pgdat->bdata, size, align, goal, limit);
	if (ptr)
		return (ptr);

	return __alloc_bootmem_limit_bs(size, align, goal, limit);
}

unsigned long __init free_all_bootmem_node_bs(pg_data_t_bs *pgdat)
{
	return (free_all_bootmem_core_bs(pgdat));
}

#ifndef CONFIG_HAVE_ARCH_BOOTMEM_NODE
void __init reserve_bootmem_bs(unsigned long addr, unsigned long size)
{
	reserve_bootmem_core_bs(NODE_DATA_BS(0)->bdata, addr, size);
}
#endif
