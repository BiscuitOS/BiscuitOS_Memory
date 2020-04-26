#ifndef _BISCUITOS_FS_H
#define _BISCUITOS_FS_H

struct address_space_bs {
	unsigned long	nrpages;
};

extern void __init vfs_caches_init_early_bs(void);
extern void __init inode_init_early_bs(void);

#endif
