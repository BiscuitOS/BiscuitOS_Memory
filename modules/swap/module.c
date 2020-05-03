/*
 * BiscuitOS Memory Manager: SWAP
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

static int test_kswap(void)
{
	for ( ; ;) {
			struct page_bs *page_bs = alloc_page_bs(GFP_KERNEL_BS);
			if (!page_bs) {
				alloc_page_bs(GFP_KERNEL_BS);
				break;
			}
	}

	return 0;
}

/* Module initialize entry */
static int __init SWAP_init(void)
{
	test_kswap();
	return 0;
}

/* Module exit entry */
static void __exit SWAP_exit(void)
{
}

module_init(SWAP_init);
module_exit(SWAP_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("BiscuitOS <buddy.zhang@aliyun.com>");
MODULE_DESCRIPTION("swap Test Case");
