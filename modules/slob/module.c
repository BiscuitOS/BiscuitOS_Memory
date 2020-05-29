/*
 * BiscuitOS Memory Manager: Slob
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
#include "biscuitos/slab.h"

struct node_default {
	unsigned long index;
	unsigned long offset;
	char *name;
};

/*
 * TestCase: kmalloc/kfree
 */
static int TestCase_kmalloc(void)
{
	struct node_default *np;

	/* alloc */
	np = (struct node_default *)kmalloc_bs(sizeof(struct node_default *),
								GFP_KERNEL_BS);	
	if (!np) {
		printk("%s-module: kmalloc failed\n", __func__);
		return -ENOMEM;
	}

	np->index = 0x90;
	printk("INDEX: %#lx\n", np->index);

	/* free */
	kfree_bs(np);
	return 0;
}

/* Module initialize entry */
static int __init Slob_Allocator_init(void)
{
	TestCase_kmalloc();

	return 0;
}

/* Module exit entry */
static void __exit Slob_Allocator_exit(void)
{
}

module_init(Slob_Allocator_init);
module_exit(Slob_Allocator_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("BiscuitOS <buddy.zhang@aliyun.com>");
MODULE_DESCRIPTION("Slob Allocator Test Case");
