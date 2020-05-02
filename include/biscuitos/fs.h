#ifndef _BISCUITOS_FS_H
#define _BISCUITOS_FS_H

#include <linux/radix-tree.h>
#include "biscuitos/prio_tree.h"

struct address_space_bs {
	struct inode		*host;	/* owner: inode, block_device */
	struct radix_tree_root	page_tree; /* radix tree of all pages */
	rwlock_t		tree_lock; /* and rwlock protecting it */
	unsigned int		i_mmap_writeable; /* count VM_STARED mappings */
	struct prio_tree_root_bs i_mmap; /* tree of private and shared mappings */
	struct list_head	i_mmap_nonlinear;/* list VM_NONLINER mappings */
	spinlock_t		i_mmap_lock; /* protect tree, count, list */
	unsigned long		nrpages; /* number of total pages */
	pgoff_t			writeback_index; /* writeback starts here */
	struct address_space_operations *a_ops; /* methods */
	unsigned long		flags;	/* error bits/gfp mask */
	struct backing_dev_info *backing_dev_info; /* device readahead, etc */
	spinlock_t		private_lock; /* for use by the address_space */
	struct list_head	private_list; /* ditto */
	struct address_space_bs *acsoc_mapping; /* ditto */
} __attribute__((aligned(sizeof(long))));

extern void __init vfs_caches_init_early_bs(void);
extern void __init inode_init_early_bs(void);

#endif
