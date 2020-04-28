/*
 * BiscuitOS Memory Manager: mempool
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
#include "biscuitos/mempool.h"

struct node_default_bs {
	unsigned long index;
	unsigned long offset;
};

/*
 * TestCase: mempool
 */
static int TestCase_mempool(void)
{
	mempool_t_bs *BiscuitOS_mempool;
	kmem_cache_t_bs *BiscuitOS_cache;
	struct node_default_bs *np;
	int ret;
#define MIN_POOL_WRITE		(32)

	/* Create kmem cache */
	BiscuitOS_cache = kmem_cache_create_bs(
				"BiscuitOS mempool",
				sizeof(struct node_default_bs),
				0,
				SLAB_HWCACHE_ALIGN_BS,
				NULL,
				NULL);
	if (!BiscuitOS_cache) {
		printk("%s kmem_cache_create failed.\n", __func__);
		return -ENOMEM;
	}

	/* Create mempool */
	BiscuitOS_mempool = mempool_create_bs(
				MIN_POOL_WRITE,
				mempool_alloc_slab_bs,
				mempool_free_slab_bs,
				BiscuitOS_cache);
	if (!BiscuitOS_mempool) {
		printk("%s mempool_create failed.\n", __func__);
		ret = -ENOMEM;
		goto out_kmem;
	}

	/* alloc free mempool */
	np = mempool_alloc_bs(BiscuitOS_mempool, SLAB_NOFS_BS);
	if (!np) {
		printk("%s mempool_alloc failed.\n", __func__);
		ret = -ENOMEM;
		goto out_alloc;
	}

	np->index = 0x68;
	bs_debug("[%#lx] INDEX %#lx\n", (unsigned long)np, np->index);

	/* free to mempool */
	mempool_free_bs(np, BiscuitOS_mempool);

	/* Destroy mempool */
	mempool_destroy_bs(BiscuitOS_mempool);
	kmem_cache_destroy_bs(BiscuitOS_cache);

	return 0;

out_alloc:
	mempool_destroy_bs(BiscuitOS_mempool);
out_kmem:
	kmem_cache_destroy_bs(BiscuitOS_cache);
	return ret;
}
module_initcall_bs(TestCase_mempool);
