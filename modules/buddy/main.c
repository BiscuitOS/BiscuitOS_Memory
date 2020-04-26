/*
 * BiscuitOS Memory Manager: Buddy
 *
 * (C) 2019.10.01 BuddyZhang1 <buddy.zhang@aliyun.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/kernel.h>
#include <linux/errno.h>
#include "biscuitos/kernel.h"
#include "biscuitos/init.h"
#include "biscuitos/mm.h"
#include "biscuitos/gfp.h"

/*
 * TestCase: alloc page from Normal Zone
 */
static int TestCase_alloc_page_from_normal(void)
{
	struct page_bs *pages;
	int order = 0;
	void *addr;

	/* allocate page from Buddy (Normal) */
	pages = alloc_pages_bs(GFP_KERNEL_BS, order);
	if (!pages) {
		printk("%s error\n", __func__);
		return -ENOMEM;
	}

	/* Obtain page virtual address */
	addr = page_address_bs(pages);
	if (!addr) {
		printk("%s bad page address!\n", __func__);
		return -EINVAL;
	}
	sprintf((char *)addr, "BiscuitOS-%s", __func__);
	bs_debug("[%#lx] %s\n", (unsigned long)addr, (char *)addr);

	/* free all pages to buddy */
	free_pages_bs((unsigned long)pages, order);

	return 0;
}
buddy_initcall_bs(TestCase_alloc_page_from_normal);

/*
 * TestCase: alloc page from HighMem Zone
 */
static int TestCase_alloc_page_from_highmem(void)
{
	struct page_bs *pages;
	int order = 0;
	void *addr;

	/* allocate page from Buddy (HighMem) */
	pages = alloc_pages_bs(__GFP_HIGHMEM_BS, order);
	if (!pages) {
		printk("%s error\n", __func__);
		return -ENOMEM;
	}

	if (!PageHighMem_bs(pages))
		printk("%s Page doesn't from HighMem Zone\n", __func__);

	/* Obtain page virtual address */
	addr = page_address_bs(pages);
	if (!addr) {
		bs_debug("%s Page doesn't mapping to virtual\n", __func__);
	} else {
		sprintf((char *)addr, "BiscuitOS-%s", __func__);
		bs_debug("[%#lx] %s\n", (unsigned long)addr, (char *)addr);
	}

	/* free pages */
	free_pages_bs((unsigned long)pages, order);

	return 0;
}
buddy_initcall_bs(TestCase_alloc_page_from_highmem);
