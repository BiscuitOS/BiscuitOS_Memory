/*
 * BiscuitOS Memory Manager: Buddy
 *
 * (C) 2019.10.01 BuddyZhang1 <buddy.zhang@aliyun.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>

#include "biscuitos/kernel.h"
#include "biscuitos/mm.h"
#include "biscuitos/gfp.h"

/*
 * TestCase: alloc_pages() - page from Normal Zone
 */
static int TestCase_alloc_pages(void)
{
	struct page_bs *pages;
	int order = 1;

	/* Alloc pages */
	pages = alloc_pages_bs(GFP_KERNEL_BS, order);
	if (!pages) {
		printk("alloc_pages_bs() from Normal Zone failed!\n");
		return -EINVAL;
	}
	printk("TestCase0: Page from Normal Zone, PFN %#lx\n", 
						page_to_pfn_bs(pages));

	/* Free pages */
	free_pages_bs(pages, order);
}

/*
 * TestCase: alloc_pages() - page from DMA Zone
 */
static int TestCase_alloc_pages_DMA(void)
{
	struct page_bs *pages;
	int order = 1;

	/* Alloc pages */
	pages = alloc_pages_bs(GFP_DMA_BS, order);
	if (!pages) {
		printk("alloc_pages_bs() from DMA Zone failed\n");
		return -EINVAL;
	}
	printk("TestCase1: Page from DMA Zone, PFN %#lx\n",
						page_to_pfn_bs(pages));

	/* Free pages */
	free_pages_bs(pages, order);

	return 0;
}

/* Module initialize entry */
static int __init Buddy_Allocator_init(void)
{

	TestCase_alloc_pages();
	TestCase_alloc_pages_DMA();

	return 0;
}

/* Module exit entry */
static void __exit Buddy_Allocator_exit(void)
{
}

module_init(Buddy_Allocator_init);
module_exit(Buddy_Allocator_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("BiscuitOS <buddy.zhang@aliyun.com>");
MODULE_DESCRIPTION("Buddy Allocator Test Case");
