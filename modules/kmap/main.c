/*
 * BiscuitOS Memory Manager: Kmap
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
#include "biscuitos/highmem.h"

/*
 * TestCase: kmap and kunmap from Highmem
 */
static int TestCase_kmap(void)
{
	struct page_bs *page;
	void *addr;

	/* alloc page */
	page = alloc_page_bs(__GFP_HIGHMEM_BS);
	if (!page || !PageHighMem_bs(page)) {
		printk("%s alloc_page() failed.\n", __func__);
		return -ENOMEM;
	}

	/* kmap */
	addr = kmap_bs(page);
	if (!addr) {
		printk("%s kmap() failed.\n", __func__);
		free_page_bs((unsigned long)page);
		return -EINVAL;
	}

	sprintf((char *)addr, "BiscuitOS-%s", __func__);
	bs_debug("[%#lx] %s\n", (unsigned long)addr, (char *)addr);

	/* kunmap */
	kunmap_bs(page);
	return 0;
}
kmap_initcall_bs(TestCase_kmap);

/*
 * TestCase: kmap and kunmap from not highmem (Normal or DMA)
 */
static int TestCase_kmap_low(void)
{
	struct page_bs *page;
	void *addr;

	/* alloc page */
	page = alloc_page_bs(GFP_KERNEL_BS);
	if (!page || PageHighMem_bs(page)) {
		printk("%s alloc_page() failed.\n", __func__);
		return -ENOMEM;
	}

	/* kmap */
	addr = kmap_bs(page);
	if (!addr) {
		printk("%s kmap() failed.\n", __func__);
		free_page_bs((unsigned long)page);
		return -EINVAL;
	}

	sprintf((char *)addr, "BiscuitOS-%s", __func__);
	bs_debug("[%#lx] %s\n", (unsigned long)addr, (char *)addr);

	/* kunmap */
	kunmap_bs(page);
	return 0;
}
kmap_initcall_bs(TestCase_kmap_low);

/*
 * TestCase: kmap_high/kunmap_high
 */
static int TestCase_kmap_high(void)
{
	struct page_bs *page;
	void *addr;

	page = alloc_page_bs(__GFP_HIGHMEM_BS);
	if (!page || !PageHighMem_bs(page)) {
		printk("%s alloc_page() failed.\n", __func__);
		return -ENOMEM;
	}

	/* kmap to high */
	addr = kmap_high_bs(page);
	if (!addr) {
		printk("%s kmap_high() failed.\n", __func__);
		free_page_bs((unsigned long)page);
		return -EINVAL;
	}

	sprintf((char *)addr, "BiscuitOS-%s", __func__);
	bs_debug("[%#lx] %s\b", (unsigned long)addr, (char *)addr);

	/* kumap */
	kunmap_high_bs(page);
	return 0;
}
kmap_initcall_bs(TestCase_kmap_high);

/*
 * TestCase: kmap_atomic/kunmap_atomic
 */
static int TestCase_kmap_atomic(void)
{
	struct page_bs *page;
	void *addr;

	/* all page */
	page = alloc_page_bs(__GFP_HIGHMEM_BS);
	if (!page || !PageHighMem_bs(page)) {
		printk("%s alloc_page() failed.\n", __func__);
		return -ENOMEM;
	}

	/* map */
	addr = kmap_atomic_bs(page, KM_USER0_BS);
	if (!addr) {
		printk("%s kmap_atomic() failed.\n", __func__);
		free_page_bs((unsigned long)page);
		return -EINVAL;
	}

	sprintf((char *)addr, "BiscuitOS-%s", __func__);
	bs_debug("[%#lx] %s\n", (unsigned long)addr, (char *)addr);

	/* unmap */
	kunmap_atomic_bs(addr, KM_USER0_BS);
	return 0;
}
kmap_initcall_bs(TestCase_kmap_atomic);
