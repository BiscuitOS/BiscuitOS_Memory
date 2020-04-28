/*
 * BiscuitOS Memory Manager: Kmap
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
#include "biscuitos/highmem.h"

/*
 * TestCase: kmap and kunmap
 */
static int TestCase_kmap(void)
{
	struct page_bs *kmap_page;
	void *addr;

	/* alloc an unmap page */
	kmap_page = alloc_page_bs(GFP_KERNEL_BS);
	if (!kmap_page) {
		printk("%s alloc_page() failed.\n", __func__);
		return -ENOMEM;
	}

	/* kmap */
	addr = kmap_atomic_bs(kmap_page, KM_USER0_BS);
	if (!addr) {
		printk("%s kmap_atomic failed\n", __func__);
		free_page_bs((unsigned long)kmap_page);
		return -ENOMEM;
	}

	sprintf((char *)addr, "BiscuitOS-%s", __func__);
	printk("[%#lx] %s\n", (unsigned long)addr, (char *)addr);

	/* kunmap */
	kunmap_atomic_bs(addr, KM_USER0_BS);

	/* free page */
	free_page_bs((unsigned long)kmap_page);

	return 0;
}

/* Module initialize entry */
static int __init KMAP_Allocator_init(void)
{
	TestCase_kmap();

	return 0;
}

/* Module exit entry */
static void __exit KMAP_Allocator_exit(void)
{
}

module_init(KMAP_Allocator_init);
module_exit(KMAP_Allocator_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("BiscuitOS <buddy.zhang@aliyun.com>");
MODULE_DESCRIPTION("Kmap Allocator Test Case");
