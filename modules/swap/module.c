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
	pg_data_t_bs *pgdat = NODE_DATA_BS(0);
	struct zone_bs *zone = &pgdat->node_zones[0];
	int order = 6;

	for ( ; ;) {
			alloc_page_bs(GFP_KERNEL_BS);
	}
	printk("ZONE: %s\n", zone->name);
	wakeup_kswapd_bs(zone, 0);

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
