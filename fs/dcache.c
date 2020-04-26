/*
 * fs/dcache.c
 *
 * Complete reimplementation
 * (C) 1997 Thomas Schoebel-Theuer,
 * with heavy changes by Linus Torvalds
 */
#include <linux/kernel.h>
#include "biscuitos/bootmem.h"
#include "biscuitos/mm.h"
#include "biscuitos/fs.h"

static __initdata unsigned long dhash_entries_bs;
static unsigned int d_hash_shift_bs;
static unsigned int d_hash_mask_bs;
static struct hlist_head *dentry_hashtable_bs;

static void __init dcache_init_early_bs(void)
{
	int loop;

	/* If hashes area distributed across NUMA nodes, defer
	 * hash allocation until vmalloc space is available
	 */
	if (hashdist_bs)
		return;

	dentry_hashtable_bs =
		alloc_large_system_hash_bs(
				"Dentry cache bs",
				sizeof(struct hlist_head),
				dhash_entries_bs,
				13,
				HASH_EARLY_BS,
				&d_hash_shift_bs,
				&d_hash_mask_bs,
				0);

	for (loop = 0; loop < (1 << d_hash_shift_bs); loop++)
		INIT_HLIST_HEAD(&dentry_hashtable_bs[loop]);
}

void __init vfs_caches_init_early_bs(void)
{
	dcache_init_early_bs();
	inode_init_early_bs();
}
