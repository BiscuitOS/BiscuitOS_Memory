/*
 * BiscuitOS Memory Manager: PCP
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
 * TestCase: alloc page from DMA Zone PCP
 */
static int TestCase_alloc_page_from_DMA_PCP(void)
{
	struct page_bs *page;
	void *addr;

	/* allocate page from PCP (Normal) */
	page = alloc_page_bs(GFP_DMA_BS);
	if (!page) {
		printk("%s alloc page failed\n", __func__);
		return -ENOMEM;
	}

	/* Obtain page virtual address */
	addr = page_address_bs(page);
	if (!addr) {
		printk("%s bad page address!\n", __func__);
		return -EINVAL;
	}
	sprintf((char *)addr, "BiscuitOS-%s", __func__);
	bs_debug("[%#lx] %s\n", (unsigned long)addr, (char *)addr);

	/* free all pages to PCP-hot */
	free_page_bs((unsigned long)addr);

	return 0;
}
pcp_initcall_bs(TestCase_alloc_page_from_DMA_PCP);

/*
 * TestCase: alloc page from Normal Zone PCP
 */
static int TestCase_alloc_page_from_normal_PCP(void)
{
	struct page_bs *page;
	void *addr;

	/* allocate page from PCP (Normal) */
	page = alloc_page_bs(GFP_KERNEL_BS);
	if (!page) {
		printk("%s alloc page failed\n", __func__);
		return -ENOMEM;
	}

	/* Obtain page virtual address */
	addr = page_address_bs(page);
	if (!addr) {
		printk("%s bad page address!\n", __func__);
		return -EINVAL;
	}
	sprintf((char *)addr, "BiscuitOS-%s", __func__);
	bs_debug("[%#lx] %s\n", (unsigned long)addr, (char *)addr);

	/* free all pages to PCP-hot */
	free_page_bs((unsigned long)addr);

	return 0;
}
pcp_initcall_bs(TestCase_alloc_page_from_normal_PCP);
