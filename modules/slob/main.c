/*
 * BiscuitOS Memory Manager: Slob
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
#include "biscuitos/string.h"

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

	/* allocate */
	np = (struct node_default *)kmalloc_bs(sizeof(struct node_default),
								GFP_KERNEL_BS);
	if (!np) {
		printk("%s kmalloc failed.\n", __func__);
		return -ENOMEM;
	}

	np->index = 0x50;
	np->offset = 0x60;
	bs_debug("%s INDEX: %#lx\n", __func__, np->index);

	/* Free */
	kfree_bs(np);

	return 0;
}
slob_initcall_bs(TestCase_kmalloc);

/*
 * TestCase: kmem_cache_create/kmem_cache_alloc/kmem_cache_free/
 *           kmem_cache_destroy
 */
static int __unused TestCase_kmem_cache_func(void)
{
	kmem_cache_t_bs *BiscuitOS_cache;
	struct node_default *np;

	/* kmem cache create */
	BiscuitOS_cache = kmem_cache_create_bs("BiscuitOS-cache",
				sizeof(struct node_default),
				0,
				SLAB_HWCACHE_ALIGN_BS,
				NULL,
				NULL);
	if (!BiscuitOS_cache) {
		printk("%s kmem_cache_create failed.\n", __func__);
		return -ENOMEM;
	}

	/* alloc from cache */
	np = kmem_cache_alloc_bs(BiscuitOS_cache, SLAB_ATOMIC_BS);
	if (!np) {
		printk("%s kmem_cache_alloc failed.\n", __func__);
		goto out_alloc;
	}

	np->index = 0x98;
	bs_debug("%s INDEX: %#lx\n", __func__, np->index);

	/* free to cache */
	kmem_cache_free_bs(BiscuitOS_cache, np);

out_alloc:
	/* kmem cache destroy */
	kmem_cache_destroy_bs(BiscuitOS_cache);
	return 0;
}
slob_initcall_bs(TestCase_kmem_cache_func);

/*
 * TestCase: kstrdup_bs
 */
static int __unused TestCase_kstrdup(void)
{
	const char *string;

	string = kstrdup_bs("BiscuitOS", GFP_KERNEL_BS);
	if (!string) {
		printk("%s kstrdup() failed.\n", __func__);
		return -ENOMEM;
	}
	bs_debug("[%#lx] %s\n", (unsigned long)string, string);

	kfree_bs(string);
	return 0;
}
slob_initcall_bs(TestCase_kstrdup);

/*
 * TestCase: Zero 
 */
static int __unused TestCase_kzalloc(void)
{
	struct node_default *np;

	np = kzalloc_bs(sizeof(*np), GFP_KERNEL_BS);
	if (!np) {
		printk("%s kzalloc() failed\n", __func__);
		return -ENOMEM;
	}

	bs_debug("[%#lx-%s] %#lx\n", (unsigned long)np, __func__, np->index);

	kfree_bs(np);
	return 0;
}
slob_initcall_bs(TestCase_kzalloc);
