/*
 * linux/fs/inode.c
 *
 * (C) 1997 Linus Torvalds
 */

#include <linux/kernel.h>
#include "biscuitos/bootmem.h"

static struct hlist_head *inode_hashtable_bs;
static __initdata unsigned long ihash_entries_bs;
static unsigned int i_hash_mask_bs;
static unsigned int i_hash_shift_bs;

/*
 * Initialize the waitqueues and inode hash table.
 */
void __init inode_init_early_bs(void)
{
	int loop;

	/* If hashes are distributed across NUMA nodes, defer
	 * hash allocation until vmalloc space is available.
	 */
	if (hashdist_bs)
		return;

	inode_hashtable_bs =
		alloc_large_system_hash_bs(
				"Inode cache bs",
				sizeof(struct hlist_head),
				ihash_entries_bs,
				14,
				HASH_EARLY_BS,
				&i_hash_shift_bs,
				&i_hash_mask_bs,
				0);

	for (loop = 0; loop < (1 << i_hash_shift_bs); loop++)
		INIT_HLIST_HEAD(&inode_hashtable_bs[loop]);
}
