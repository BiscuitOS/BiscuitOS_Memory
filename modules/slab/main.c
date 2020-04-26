/*
 * BiscuitOS Memory Manager: Slab
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
#include "biscuitos/slab.h"
#include "biscuitos/gfp.h"

struct node_default {
	unsigned long index;
	unsigned long offset;
	char *name;
};

/*
 * TestCase: Kmalloc
 */
static int TestCase_kmalloc(void)
{
	struct node_default *np;

	bs_debug_enable();
	/* allocate */
	np = (struct node_default *)kmalloc_bs(sizeof(struct node_default),
								GFP_KERNEL_BS);
	if (!np) {
		printk("%s kmalloc failed.\n", __func__);
		return -ENOMEM;
	}
	bs_debug_disable();

	np->index = 0x50;
	np->offset = 0x60;

	/* Free */
	kfree_bs(np);

	return 0;
}
slab_initcall_bs(TestCase_kmalloc);
