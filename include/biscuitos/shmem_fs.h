#ifndef _BISCUITOS_SHMEM_FS_H
#define _BISCUITOS_SHMEM_FS_H

#include "biscuitos/swap.h"
#include "biscuitos/mempolicy.h"

/* inode in-kernel data */
#define SHMEM_NR_DIRECT_BS	16

struct shmem_inode_info_bs {
	spinlock_t		lock;
	unsigned long		flags;
	unsigned long		alloced;	/* data pages alloced to file */
	unsigned long		swapped;	/* subtotal assigned to swap */
	unsigned long		next_index;	/* highest alloced index + 1 */
	struct shared_policy_bs policy;		/* NUMA memory alloc policy */
	struct page_bs		*i_indirect;	/* top indirect blocks page */
	swp_entry_t_bs		i_direct[SHMEM_NR_DIRECT_BS]; /* first blocks */
	struct list_head	swaplist;	/* chain of maybes on swap */
	struct inode		vfs_inode;
};

struct shmem_sb_info_bs {
	unsigned long max_blocks;   /* How many blocks are allowed */
	unsigned long free_blocks;  /* How many are left for allocation */
	unsigned long max_inodes;   /* How many inodes are allowed */
	unsigned long free_inodes;  /* How many are left for allocation */
	spinlock_t    stat_lock;
};

static inline struct shmem_inode_info_bs *SHMEM_I_BS(struct inode *inode)
{
	return container_of(inode, struct shmem_inode_info_bs, vfs_inode);
}

#endif
