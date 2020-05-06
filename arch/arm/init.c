/*
 * arm/mm/init
 *
 * (C) 2020.04.14 BuddyZhang1 <buddy.zhang@aliyun.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/kernel.h>
#include <asm/string.h>

#include "biscuitos/kernel.h"
#include "biscuitos/mm.h"
#include "biscuitos/bootmem.h"
#include "biscuitos/nodemask.h"
#include "biscuitos/mmzone.h"
#include "biscuitos/swap.h"
#include "biscuitos/mman.h"
#include "biscuitos/init.h"
#include "biscuitos/page-flags.h"
#include "asm-generated/setup.h"
#include "asm-generated/arch.h"
#include "asm-generated/memory.h"
#include "asm-generated/map.h"
#include "asm-generated/tlbflush.h"
#include "asm-generated/string.h"
#include "asm-generated/cacheflush.h"
#include "asm-generated/mmzone.h"
#include "asm-generated/vmalloc.h"
#include "asm-generated/highmem.h"

/* BiscuitOS Emulate */
extern unsigned long _stext_bs, _etext_bs, __init_begin_bs, _text_bs;
extern unsigned long _end_bs, __init_end_bs, __data_start_bs;

unsigned long phys_initrd_start_bs __initdata = 0;
unsigned long phys_initrd_size_bs __initdata = 0;
unsigned long initrd_start_bs, initrd_end_bs;
extern void __init kmap_init_bs(void);
extern void __init build_mem_type_table_bs(void);
extern void __init create_mapping_bs(struct map_desc *md);

/*
 * empty_zero_page is a special page that is used for
 * zero-initialized data and COW.
 */
struct page_bs *empty_zero_page_bs;

/*
 * The sole use of this is to pass memory configuration
 * data from paging_init to mem_init
 */
struct meminfo meminfo_bs __initdata = { 0, };

struct node_info {
	unsigned int start;
	unsigned int end;
	int bootmap_pages;
};

#define O_PFN_DOWN(x)	((x) >> PAGE_SHIFT_BS)
#define O_PFN_UP(x)	(PAGE_ALIGN_BS(x) >> PAGE_SHIFT_BS)

#define for_each_nodebank_bs(iter,mi,no)			\
	for(iter = 0; iter < mi->nr_banks; iter++)		\
		if (mi->bank[iter].node == no)

/*
 * FIXME: We really want to avoid allocating the bootmap bitmap
 * over the top of the initrd. Hopefully, this is located towards
 * the start of a bank, so if we allocate the bootmap bitmap at
 * the end, we won't clash.
 */
static unsigned int __init
find_bootmap_pfn_bs(int node, struct meminfo *mi, unsigned int bootmap_pages)
{
	unsigned int start_pfn, bank, bootmap_pfn;

	start_pfn   = PAGE_ALIGN_BS(__pa_bs(_end_bs)) >> PAGE_SHIFT_BS;
	bootmap_pfn = 0;

	for_each_nodebank_bs(bank, mi, node) {
		unsigned long start, end;

		start = mi->bank[bank].start >> PAGE_SHIFT_BS;
		end   = (mi->bank[bank].size +
			 mi->bank[bank].start) >> PAGE_SHIFT_BS;

		if (end < start_pfn)
			continue;

		if (start < start_pfn)
			start = start_pfn;

		if (end <= start)
			continue;

		if (end - start >= bootmap_pages) {
			bootmap_pfn = start;
			break;
		}
	}

	if (bootmap_pfn == 0)
		BUG_BS();

	return bootmap_pfn;
}

static int __init check_initrd_bs(struct meminfo *mi)
{
	int initrd_node = -2;
	unsigned long end = phys_initrd_start_bs + phys_initrd_size_bs;

	/*
	 * Make sure that the initrd is within a valid area of
	 * memory.
	 */
	if (phys_initrd_size_bs) {
		unsigned int i;

		initrd_node = -1;

		for (i = 0; i < mi->nr_banks; i++) {
			unsigned long bank_end;

			bank_end = mi->bank[i].start + mi->bank[i].size;

			if (mi->bank[i].start <= phys_initrd_start_bs &&
					end <= bank_end)
				initrd_node = mi->bank[i].node;
		}
	}

	if (initrd_node == -1) {
		printk(KERN_ERR "initrd (0x%08lx - 0x%08lx) extends beyond "
			"physical memory - disabling initrd\n",
				phys_initrd_start_bs, end);
		phys_initrd_start_bs = phys_initrd_size_bs = 0;
	}

	return initrd_node;
}

/*
 * Reserve the various regions of node 0
 */
static __init void reserve_node_zero_bs(pg_data_t_bs *pgdat)
{
	/*
	 * Register the kernel text and data with bootmeme.
	 * Note that this can only be in node 0.
	 */
	reserve_bootmem_node_bs(pgdat, __pa_bs(_stext_bs), 
							_end_bs - _stext_bs);

	/*
	 * Reserve the page table. These are already in use,
	 * and can only be in node 0.
	 */
	reserve_bootmem_node_bs(pgdat, __pa_bs(swapper_pg_dir_bs),
			PTRS_PER_PGD_BS * sizeof(pgd_t_bs));
}

/* FIXME: BiscuitOS bootmem entry */
DEBUG_FUNC_T(bootmem);

static unsigned long __init
bootmem_init_node_bs(int node, int initrd_node, struct meminfo *mi)
{
	unsigned long zone_size[MAX_NR_ZONES_BS], zhole_size[MAX_NR_ZONES_BS];
	unsigned long start_pfn, end_pfn, boot_pfn;
	unsigned int boot_pages;
	pg_data_t_bs *pgdat;
	int i;

	start_pfn = -1UL;
	end_pfn = 0;

	/*
	 * Calculate the pfn range, and map the memory banks for this node.
	 */
	for_each_nodebank_bs(i, mi, node) {
		unsigned long start, end;
		struct map_desc map;

		start = mi->bank[i].start >> PAGE_SHIFT_BS;
		end = (mi->bank[i].start + mi->bank[i].size) >> PAGE_SHIFT_BS;

		if (start_pfn > start)
			start_pfn = start;
		if (end_pfn < end)
			end_pfn = end;

		map.pfn = __phys_to_pfn_bs(mi->bank[i].start);
		map.virtual = __phys_to_virt_bs(mi->bank[i].start);
		map.length = mi->bank[i].size;
		map.type = MT_MEMORY_RWX_BS;

		create_mapping_bs(&map);
	}

	/*
	 * If there is no memory in this node, ignore it.
	 */
	if (end_pfn == 0)
		return end_pfn;

	/*
	 * Allocate the bootmem bitmap page.
	 */
	boot_pages = bootmem_bootmap_pages_bs(end_pfn - start_pfn);
	boot_pfn = find_bootmap_pfn_bs(node, mi, boot_pages);

	/*
	 * Initialise the bootmem allocator for this node, handing the
	 * memory banks over to bootmem.
	 */
	node_set_online_bs(node);
	pgdat = NODE_DATA_BS(node);
	init_bootmem_node_bs(pgdat, boot_pfn, start_pfn, end_pfn);

	for_each_nodebank_bs(i, mi, node)
		free_bootmem_node_bs(pgdat, 
				mi->bank[i].start, mi->bank[i].size);

	/*
	 * Reserve the bootmem bitmap for this node.
	 */
	reserve_bootmem_node_bs(pgdat, boot_pfn << PAGE_SHIFT,
			     boot_pages << PAGE_SHIFT);

#ifdef CONFIG_BLK_DEV_INITRD
	/*
	 * If the initrd is in this node, reserve its memory.
	 */
	if (node == initrd_node) {
		reserve_bootmem_node_bs(pgdat, phys_initrd_start_bs,
				     phys_initrd_size_bs);
		initrd_start_bs = __phys_to_virt_bs(phys_initrd_start_bs);
		initrd_end_bs = initrd_start_bs + phys_initrd_size_bs;
	}
#endif

	/*
	 * Finally, reserve any node zero regions.
	 */
	if (node == 0)
		reserve_node_zero_bs(pgdat);

	/*
	 * initialise the zones within this node.
	 */
	memset(zone_size, 0, sizeof(zone_size));
	memset(zhole_size, 0, sizeof(zhole_size));

	/*
	 * The size of this node has already been determined.  If we need
	 * to do anything fancy with the allocation of this memory to the
	 * zones, now is the time to do it.
	 */
	zone_size[0] = end_pfn - start_pfn;

	/*
	 * For each bank in this node, calculate the size of the holes.
	 *  holes = node_size - sum(bank_sizes_in_node)
	 */
	zhole_size[0] = zone_size[0];
	for_each_nodebank_bs(i, mi, node)
		zhole_size[0] -= mi->bank[i].size >> PAGE_SHIFT_BS;

	/*
	 * Adjust the sizes according to any special requirements for
	 * this machine type.
	 */
	arch_adjust_zones_bs(node, zone_size, zhole_size);

	free_area_init_node_bs(node, pgdat, zone_size, start_pfn, zhole_size);

#ifdef CONFIG_HIGHMEM_BS
	/* FIXME: BiscuitOS Supprot HighMem Zone */
	end_pfn -= PAGE_ALIGN_BS(meminfo_bs.bank[1].size) >> PAGE_SHIFT_BS;
#endif

	return end_pfn;
}

/*
 * Initialise the bootmem allocator for all nodes. This is called
 * early during the architecture specific initialisation.
 */
static void __init bootmem_init_bs(struct meminfo *mi)
{
	unsigned long addr, memend_pfn = 0;
	int node, initrd_node, i;

	/*
	 * Invalidate the node number for empty or invalid memory banks
	 */
	for (i = 0; i < mi->nr_banks; i++)
		if (mi->bank[i].size == 0 || mi->bank[i].node >= 
						MAX_NUMNODES_BS)
			mi->bank[i].node = -1;

	memcpy(&meminfo_bs, mi, sizeof(meminfo_bs));

	/*
	 * Clear out all the kernel space mappings, except for the first
	 * memory bank, up to the end of the vmalloc region.
	 */
	for (addr = __phys_to_virt_bs(mi->bank[0].start + mi->bank[0].size);
	     addr < VMALLOC_END_BS; addr += PGDIR_SIZE_BS)
		pmd_clear_bs(pmd_off_k_bs(addr));

	/*
	 * Locate which node contains the ramdisk image, if any.
	 */
	initrd_node = check_initrd_bs(mi);

	/*
	 * Run through each node initialising the bootmem allocator.
	 */
	for_each_node_bs(node) {
		unsigned long end_pfn;

		end_pfn = bootmem_init_node_bs(node, initrd_node, mi);

		/*
		 * Remember the highest memory PFN.
		 */
		if (end_pfn > memend_pfn)
			memend_pfn = end_pfn;
	}

	high_memory_bs = __va_bs(memend_pfn << PAGE_SHIFT_BS);

	/*
	 * This doesn't seem to be used by the Linux memory manager any
	 * more, but is used by ll_rw_block.  If we can get rid of it, we
	 * also get rid of some of the stuff above as well.
	 *
	 * Note: max_low_pfn and max_pfn reflect the number of _pages_ in
	 * the system, not the maximum PFN.
	 */
	max_pfn_bs = max_low_pfn_bs = memend_pfn - PHYS_PFN_OFFSET_BS;

	/* FIXME: bootmem_initcall entry, used to debug bootmem,
	 * This code isn't default code */
	DEBUG_CALL(bootmem);
}

/*
 * Set up device the mappings.  Since we clear out the page tables for all
 * mappings above VMALLOC_END, we will remove any debug device mappings.
 * This means you have to be careful how you debug this function, or any
 * called function.  This means you can't use any function or debugging
 * method which may touch any device, otherwise the kernel _will_ crash.
 */
static void __init devicemaps_init_bs(struct machine_desc_bs *mdesc)
{
#ifdef CONFIG_HIGHMEM_BS
	unsigned long addr;
	struct meminfo *mi = &meminfo_bs;

        /*
         * Clear out all the kernel space mappings, except for the first
         * memory bank, up to the end of the vmalloc region.
         */
        for (addr = __phys_to_virt_bs(mi->bank[0].start + mi->bank[0].size);
             addr < __phys_to_virt_bs(mi->bank[1].start + mi->bank[1].size); 
					addr += PGDIR_SIZE_BS) {
                pmd_clear_bs(pmd_off_k_bs(addr));
	}
#endif

}

/*
 * paging_init() sets up the page tables, initializes the zone memory
 * maps, and sets up the zero page, bad page and page table.
 */
void __init paging_init_bs(struct meminfo *mi, struct machine_desc_bs *mdesc)
{
	void *zero_page;

	build_mem_type_table_bs();
	bootmem_init_bs(mi);
	devicemaps_init_bs(mdesc);

	top_pmd_bs = pmd_off_k_bs(0x96400000);

	/*
	 * allocate the zero page.  Note that we count on this going ok.
	 */
	zero_page = alloc_bootmem_low_pages_bs(PAGE_SIZE_BS);
	/*
	 * finish off the bad pages once the mem_map is initialized
	 */
	memzero_bs(zero_page, PAGE_SIZE);
	empty_zero_page_bs = virt_to_page_bs(zero_page);
	/* FIXME: ignore? */
	flush_dcache_page_bs(empty_zero_page_bs);

	/* FIXME: Kmap from Intel-i386 and high-version ARM */
	kmap_init_bs();
}

static inline void
free_memmap_bs(int node, unsigned long start_pfn, unsigned long end_pfn)
{
	struct page_bs *start_pg, *end_pg;
	unsigned long pg, pgend;

	/*
	 * Convert start_pfn/end_pfn to a struct page pointer.
	 */
	start_pg = pfn_to_page_bs(start_pfn);
	end_pg = pfn_to_page_bs(end_pfn);

	/*
	 * Convert to physical addresses, and
	 * round start upwards and end downwards.
	 */
	pg = PAGE_ALIGN_BS(__pa_bs(start_pg));
	pgend = __pa_bs(end_pg) & PAGE_MASK_BS;

	/*
	 * If there are free pages between these,
	 * free the section of the memmap array.
	 */
	if (pg < pgend)
		free_bootmem_node_bs(NODE_DATA_BS(node), pg, pgend - pg);
}

/*
 * The mem_map array can get very big.  Free the unused area of the memory map.
 */
static void __init free_unused_memmap_node_bs(int node, struct meminfo *mi)
{
	unsigned long bank_start, prev_bank_end = 0;
	unsigned int i;

	/*
	 * [FIXME] This relies on each bank being in address order.  This
	 * may not be the case, especially if the user has provided the
	 * information on the command line.
	 */
	for_each_nodebank_bs(i, mi, node) {
		bank_start = mi->bank[i].start >> PAGE_SHIFT_BS;
		if (bank_start < prev_bank_end) {
			printk(KERN_ERR "MEM: unordered memory banks.  "
				"Not freeing memmap.\n");
			break;
		}

		/*
		 * If we had a previous bank, and there is a space
		 * between the current bank and the previous, free it.
		 */
		if (prev_bank_end && prev_bank_end != bank_start)
			free_memmap_bs(node, prev_bank_end, bank_start);

		prev_bank_end = (mi->bank[i].start +
				 mi->bank[i].size) >> PAGE_SHIFT_BS;
	}
}

/* FIXME: BiscuitOS buddy debug stuf */
DEBUG_FUNC_T(buddy);
DEBUG_FUNC_T(pcp);

/*
 * mem_init() marks the free areas in the mem_map and tells us how much
 * memory is free. This is done after various parts of the system have
 * claimed their memory after the kernel image.
 */
void __init mem_init_bs(void)
{
	unsigned int codepages, datapages, initpages;
	int i, node;

	codepages = _etext_bs - _text_bs;
	datapages = _end_bs - __data_start_bs;
	initpages = __init_end_bs - __init_begin_bs;

	max_mapnr_bs = virt_to_page_bs(high_memory_bs) - mem_map_bs;

	/* this will put all unused low memory onto the freelists */
	for_each_online_node_bs(node) {
		pg_data_t_bs *pgdat = NODE_DATA_BS(node);

		free_unused_memmap_node_bs(node, &meminfo_bs);

		if (pgdat->node_spanned_pages != 0)
			totalram_pages_bs += free_all_bootmem_node_bs(pgdat);
	}

	/*
	 * Since our memory may not be contiguous, calculate the
	 * real number of pages we have in this system
	 */
	printk(KERN_INFO "Memory:");

	num_physpages_bs = 0;
	for (i = 0; i < meminfo_bs.nr_banks; i++) {
		num_physpages_bs +=
				meminfo_bs.bank[i].size >> PAGE_SHIFT_BS;
		printk(" %ldMB", meminfo_bs.bank[i].size >> 20);
	}

	printk(" = %luMB total\n", num_physpages_bs >> (20 - PAGE_SHIFT_BS));
	printk(KERN_NOTICE "Memory: %luKB available (%dK code, "
			"%dK data, %dK init)\n",
		(unsigned long)nr_free_pages_bs() << (PAGE_SHIFT_BS-10),
		codepages >> 10, datapages >> 10, initpages >> 10);

	if (PAGE_SIZE_BS > 16364 && num_physpages_bs <= 128) {
		extern int sysctl_overcommit_memory;

		/*
		 * On a machine this small we won't get
		 * anywhere without overcommit, so turn
		 * it on by default.
		*/
		sysctl_overcommit_memory = OVERCOMMIT_ALWAYS_BS;

	}

	/* FIXME: buddy_initcall entry, used to debug buddy,
	 * This code isn't default code */
	DEBUG_CALL(buddy);
	DEBUG_CALL(pcp);
}

void show_mem_bs(void)
{
	int free = 0, total = 0, reserved = 0;
	int shared = 0, cached = 0, slab = 0, node;

	printk("Mem-info:\n");
	show_free_areas_bs();
	printk("Free swap:       %6ldkB\n",
				nr_swap_pages_bs << (PAGE_SHIFT_BS-10));

	for_each_online_node_bs(node) {
		struct page_bs *page, *end;

		page = NODE_MEM_MAP_BS(node);
		end  = page + NODE_DATA_BS(node)->node_spanned_pages;

		do {
			total++;
			if (PageReserved_bs(page))
				reserved++;
			else if (PageSwapCache_bs(page))
				cached++;
			else if (PageSlab_bs(page))
				slab++;
			else if (!page_count_bs(page))
				free++;
			else
				shared += page_count_bs(page) - 1;
			page++;
		} while (page < end);
	}

	printk("%d pages of RAM\n", total);
	printk("%d free pages\n", free);
	printk("%d reserved pages\n", reserved);
	printk("%d slab pages\n", slab);
	printk("%d pages shared\n", shared);
	printk("%d pages swap cached\n", cached);
}
