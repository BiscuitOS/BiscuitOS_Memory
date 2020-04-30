/*
 * Resizable virtual memory filesystem for Linux.
 *
 * Copyright (C) 2000 Linus Torvalds.
 *               2000 Transmeta Corp.
 *               2000-2001 Christoph Rohland
 *               2000-2001 SAP AG
 *               2002 Red Hat Inc.
 * Copyright (C) 2002-2004 Hugh Dickins.
 * Copyright (C) 2002-2004 VERITAS Software Corporation.
 * Copyright (C) 2004 Andi Kleen, SuSE Labs
 *
 * Extended attribute support for tmpfs:
 * Copyright (c) 2004, Luke Kenneth Casson Leighton <lkcl@lkcl.net>
 * Copyright (c) 2004 Red Hat, Inc., James Morris <jmorris@redhat.com>
 *
 * This file is released under the GPL.
 */
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/cred.h>
#include <linux/xattr.h>
#include <uapi/linux/mount.h>
#include <linux/security.h>
#include <linux/statfs.h>
#include <linux/mm.h>
#include <linux/blkdev.h>
#include <linux/backing-dev.h>
#include <linux/dcache.h>
#include "biscuitos/kernel.h"
#include "biscuitos/init.h"
#include "biscuitos/shmem_fs.h"
#include "biscuitos/pagemap.h"
#include "asm-generated/page.h"
#include "biscuitos/slab.h"
#include "biscuitos/mm.h"

/* This magic number is used in glibc for posix shared memory */
#define TMPFS_MAGIC_BS		0x90919416

#define ENTRIES_PER_PAGE_BS	(PAGE_CACHE_SIZE_BS/sizeof(unsigned long))
#define ENTRIES_PER_PAGEPAGE_BS	(ENTRIES_PER_PAGE_BS*ENTRIES_PER_PAGE_BS)
#define BLOCKS_PER_PAGE_BS	(PAGE_CACHE_SIZE_BS/512)

#define SHMEM_MAX_INDEX_BS	(SHMEM_NR_DIRECT_BS + \
				(ENTRIES_PER_PAGEPAGE_BS/2) * \
				(ENTRIES_PER_PAGE_BS+1))
#define SHMEM_MAX_BYTES_BS	((unsigned long long)SHMEM_MAX_INDEX_BS << \
						PAGE_CACHE_SHIFT_BS)

/* Pretend that each entry is of this size in directory's i_size */
#define BOGO_DIRENT_SIZE_BS	20

static kmem_cache_t_bs *shmem_inode_cachep_bs;
static struct vfsmount *shm_mnt_bs;

static struct address_space_operations shmem_aops_bs;
static struct backing_dev_info shmem_backing_dev_info_bs;
static struct inode_operations shmem_special_inode_operations_bs;
static struct inode_operations shmem_inode_operations_bs;
static struct file_operations shmem_file_operations_bs;
static struct inode_operations shmem_dir_inode_operations_bs;

static inline struct shmem_sb_info_bs *SHMEM_SB_BS(struct super_block *sb)
{
	return sb->s_fs_info;
}

static int shmem_parse_options_bs(char *options, int *mode, uid_t *uid,
		gid_t *gid, unsigned long *blocks, unsigned long *inodes)
{
	char *this_char, *value, *rest;

	while ((this_char = strsep(&options, ",")) != NULL) {
		if (!*this_char)
			continue;
		if ((value = strchr(this_char, '=')) != NULL) {
			*value++ = 0;
		} else {
			printk(KERN_ERR "tmpfs_bs: No value for mount "
					"option '%s'\n", this_char);
			return 1;
		}

		if (!strcmp(this_char,"size")) {
			unsigned long long size;

			size = memparse(value,&rest);
			if (*rest == '%') {
				size <<= PAGE_SHIFT;
				size *= totalram_pages_bs;
				do_div(size, 100);
				rest++;
			}
			if (*rest)
				goto bad_val;
			*blocks = size >> PAGE_CACHE_SHIFT_BS;
		} else if (!strcmp(this_char,"nr_blocks")) {
			*blocks = memparse(value,&rest);
			if (*rest)
				goto bad_val;
		} else if (!strcmp(this_char,"nr_inodes")) {
			*inodes = memparse(value,&rest);
			if (*rest)
				goto bad_val;
		} else if (!strcmp(this_char,"mode")) {
			if (!mode)
				continue;
			*mode = simple_strtoul(value,&rest,0);
			if (*rest)
				goto bad_val;
		} else if (!strcmp(this_char,"uid")) {
			if (!uid)
				continue;
			*uid = simple_strtoul(value,&rest,0);
			if (*rest)
				goto bad_val;
		} else if (!strcmp(this_char,"gid")) {
			if (!gid)
				continue;
			*gid = simple_strtoul(value,&rest,0);
			if (*rest)
				goto bad_val;
		} else {
			printk(KERN_ERR "tmpfs: Bad mount option %s\n",
						this_char);
			return 1;
		}
	}
	return 0;

bad_val:
	printk(KERN_ERR "tmpfs: Bad value '%s' for mount option '%s'\n",
			value, this_char);
	return 1;
}

#ifdef CONFIG_TMPFS_BS
static int shmem_set_size_bs(struct shmem_sb_info_bs *sbinfo,
			unsigned long max_blocks, unsigned long max_inodes)
{
	int error;
	unsigned long blocks, inodes;

	spin_lock(&sbinfo->stat_lock);
	blocks = sbinfo->max_blocks - sbinfo->free_blocks;
	inodes = sbinfo->max_inodes - sbinfo->free_inodes;
	error = -EINVAL;
	if (max_blocks < blocks)
		goto out;
	if (max_inodes < inodes)
		goto out;
	error = 0;
	sbinfo->max_blocks  = max_blocks;
	sbinfo->max_inodes  = max_inodes;	
#ifndef CONFIG_BISCUITOS_5
	sbinfo->free_blocks = max_blocks - blocks;
	sbinfo->free_indoes = max_inodes - inodes;
#endif
out:
	spin_unlock(&sbinfo->stat_lock);
	return error;
}
#endif

#ifdef CONFIG_TMPFS_SECURITY_BS

static size_t __unused shmem_xattr_security_list_bs(struct inode *inode,
	char *list, size_t list_len, const char *name, size_t name_len)
{
	return security_inode_listsecurity(inode, list, list_len);
}

static int __unused shmem_xattr_security_get_bs(struct inode *inode, 
		const char *name, void *buffer, size_t size)
{
	if (strcmp(name, "") == 0)
		return -EINVAL;
	return security_inode_getsecurity(inode, name, buffer, size);
}

static int __unused shmem_xattr_security_set_bs(struct inode *inode,
		const char *name, const void *value, size_t size, int flags)
{
	if (strcmp(name, "") == 0)
		return -EINVAL;
	return security_inode_setsecurity(inode, name, value, size, flags);
}

static struct xattr_handler shmem_xattr_security_handler_bs = {
	.prefix	= XATTR_SECURITY_PREFIX,
	.list	= shmem_xattr_security_list_bs,
	.get	= shmem_xattr_security_get_bs,
	.set	= shmem_xattr_security_set_bs,
};

#endif

static struct inode *shmem_alloc_inode_bs(struct super_block *sb)
{
	struct shmem_inode_info_bs *p;

	p = (struct shmem_inode_info_bs *)kmem_cache_alloc_bs(
				shmem_inode_cachep_bs, SLAB_KERNEL_BS);
	if (!p)
		return NULL;
	return &p->vfs_inode;
}

static void shmem_destroy_inode_bs(struct inode *inode)
{
	if ((inode->i_mode & S_IFMT) == S_IFREG) {
		/* only struct inode is valid if it's inline symlink */
		mpol_free_shared_policy_bs(&SHMEM_I_BS(inode)->policy);
	}
	kmem_cache_free_bs(shmem_inode_cachep_bs, SHMEM_I_BS(inode));
}

/* FIXME: Implement from linux 5.0 */
#ifdef CONFIG_BISCUITOS_5
static int shmem_statfs_bs(struct dentry *dentry, struct kstatfs *buf)
{
	struct shmem_sb_info_bs *sbinfo = SHMEM_SB_BS(dentry->d_sb);
#else
static int shmem_statfs_bs(struct super_block *sb, struct kstatfs *buf)
{
	struct shmem_sb_info_bs *sbinfo = SHMEM_SB_BS(sb);
#endif

	buf->f_type = TMPFS_MAGIC_BS;
	buf->f_bsize = PAGE_CACHE_SIZE_BS;
	buf->f_namelen = NAME_MAX;
	if (sbinfo) {
		spin_lock(&sbinfo->stat_lock);
		buf->f_blocks = sbinfo->max_blocks;
		buf->f_bavail = buf->f_bfree = sbinfo->free_blocks;
		buf->f_files = sbinfo->max_inodes;
		buf->f_ffree = sbinfo->free_inodes;
		spin_unlock(&sbinfo->stat_lock);
	}
	/* else leave those fields 0 like simple_statfs */
	return 0;
}

static int shmem_remount_fs_bs(struct super_block *sb, int *flags, char *data)
{
	struct shmem_sb_info_bs *sbinfo = SHMEM_SB_BS(sb);
	unsigned long max_blocks = 0;
	unsigned long max_inodes = 0;

	if (sbinfo) {
		max_blocks = sbinfo->max_blocks;
		max_inodes = sbinfo->max_inodes;
	}
	if (shmem_parse_options_bs(data, NULL, NULL, NULL, 
					&max_blocks, &max_inodes))
		return -EINVAL;
	/* Keep it simple: disallow limited <-> unlimited remount */
	if ((max_blocks || max_inodes) == !sbinfo)
		return -EINVAL;
	/* But allow the pointless unlimited -> unlimited remount */
	if (!sbinfo)
		return 0;
	return shmem_set_size_bs(sbinfo, max_blocks, max_inodes);
}

static void __unused shmem_delete_inode_bs(struct inode *inode)
{
	BS_DUP();
}

static void shmem_put_super_bs(struct super_block *sb)
{
	kfree_bs(sb->s_fs_info);
	sb->s_fs_info = NULL;
}

/*
 * Move the page from the page cache to the swap cache.
 */
static int shmem_writepage_bs(struct page *page, struct writeback_control *wbc)
{
	BS_DUP();
	return 0;
}

/*
 * Normally tmpfs makes no use of shmem_prepare_write, but it
 * lets a tmpfs file be used read-write below the loop driver.
 */
static int __unused
shmem_prepare_write_bs(struct file *file, struct page *page, 
					unsigned offset, unsigned to)
{
	BS_DUP();
	return 0;
}

static void __unused shmem_truncate_bs(struct inode *inode)
{
	BS_DUP();
}

static int shmem_notify_change_bs(struct dentry *dentry, struct iattr *attr)
{
	BS_DUP();
	return 0;
}

static int shmem_mmap_bs(struct file *file, struct vm_area_struct *vma)
{
	BS_DUP();
	return 0;
}

static ssize_t shmem_file_read_bs(struct file *filp, char __user *buf,
			size_t count, loff_t *ppos)
{
	BS_DUP();
	return 0;
}

static ssize_t
shmem_file_write_bs(struct file *filp, const char __user *buf, 
				size_t count, loff_t *ppos)
{
	BS_DUP();
	return 0;
}

static ssize_t __unused shmem_file_sendfile_bs(struct file *in_file, 
		loff_t *ppos, size_t count, read_actor_t actor, void *target)
{
	BS_DUP();
	return 0;
}

#ifdef CONFIG_BISCUITOS_5
static int shmem_create_bs(struct inode *dir, struct dentry *dentry,
						umode_t mode, bool excl)
#else
static int shmem_create_bs(struct inode *dir, struct dentry *dentry, int mode,
					struct nameidata *nd)
#endif
{
	BS_DUP();
	return 0;
}

/*
 * Link a file..
 */
static int shmem_link_bs(struct dentry *old_dentry, struct inode *dir,
					struct dentry *dentry)
{
	BS_DUP();
	return 0;
}

static int shmem_unlink_bs(struct inode *dir, struct dentry *dentry)
{
	BS_DUP();
	return 0;
}

static int shmem_symlink_bs(struct inode *dir, struct dentry *dentry,
					const char *symname)
{
	BS_DUP();
	return 0;
}

#ifdef CONFIG_BISCUITOS_5
static int shmem_mkdir_bs(struct inode *dir, struct dentry *dentry, 
								umode_t mode)
#else
static int shmem_mkdir_bs(struct inode *dir, struct dentry *dentry, int mode)
#endif
{
	BS_DUP();
	return 0;
}

static int shmem_rmdir_bs(struct inode *dir, struct dentry *dentry)
{
	BS_DUP();
	return 0;
}

/*
 * File creation. Allocate an inode, and we're done..
 */
static int
#ifndef CONFIG_BISCUITOS_5
shmem_mknod_bs(struct inode *dir, struct dentry *dentry, int mode, dev_t dev)
#else
shmem_mknod_bs(struct inode *dir, struct dentry *dentry, 
						umode_t mode, dev_t dev)
#endif
{
	BS_DUP();
	return 0;
}

/*
 * The VFS layer already does all the dentry stuff for rename,
 * we just have to decrement the usage count for the target if
 * it exists so that the VFS layer correctly free's it when it
 * gets overwritten.
 */
#ifndef CONFIG_BISCUITOS_5
static int shmem_rename_bs(struct inode *old_dir, struct dentry *old_dentry,
			struct inode *new_dir, struct dentry *new_dentry)
#else
static int shmem_rename_bs(struct inode *old_dir, struct dentry *old_dentry,
		struct inode *new_dir, struct dentry *new_dentry, 
		unsigned int flags)
#endif
{
	BS_DUP();
	return 0;
}

static struct inode *
shmem_get_inode_bs(struct super_block *sb, int mode, dev_t dev)
{
	struct inode *inode;
	struct shmem_inode_info_bs *info;
	struct shmem_sb_info_bs *sbinfo = SHMEM_SB_BS(sb);

	if (sbinfo) {
		spin_lock(&sbinfo->stat_lock);
		if (!sbinfo->free_inodes) {
			spin_unlock(&sbinfo->stat_lock);
			return NULL;
		}
		sbinfo->free_inodes--;
		spin_unlock(&sbinfo->stat_lock);
	}

	inode = new_inode(sb);
	if (inode) {
		inode->i_mode = mode;
#ifndef CONFIG_BISCUITOS_5
		inode->i_uid = current->fsuid;
		inode->i_gid = current->fsgid;
		inode->i_blksize = PAGE_CACHE_SIZE_BS;
#else
		inode->i_uid = current_fsuid();
		inode->i_gid = current_fsgid();
#endif
		inode->i_blocks = 0;
		inode->i_mapping->a_ops = &shmem_aops_bs;
#ifndef CONFIG_BISCUITOS_5
		inode->i_mapping->backing_dev_info = &shmem_backing_dev_info_bs;
#endif
		inode->i_atime = inode->i_mtime = inode->i_ctime = 
#ifndef CONFIG_BISCUITOS_5
						CURRENT_TIME;
#else
						current_time(inode);
#endif
		info = SHMEM_I_BS(inode);
		memset(info, 0, (char *)inode - (char *)info);
		spin_lock_init(&info->lock);
		INIT_LIST_HEAD(&info->swaplist);

		switch (mode & S_IFMT) {
		default:
			inode->i_op = &shmem_special_inode_operations_bs;
			init_special_inode(inode, mode, dev);
			break;
		case S_IFREG:
			inode->i_op = &shmem_inode_operations_bs;
			inode->i_fop = &shmem_file_operations_bs;
			mpol_shared_policy_init_bs(&info->policy);
			break;
		case S_IFDIR:
#ifndef CONFIG_BISCUITOS_5
			inode->i_nlink++;
#else
			inc_nlink(inode);
#endif
			/* Some things misbenhave if size == 0 on a directory */
			inode->i_size = 2 * BOGO_DIRENT_SIZE_BS;
			inode->i_op = &shmem_dir_inode_operations_bs;
			inode->i_fop = &simple_dir_operations;
			break;
		case S_IFLNK:
			BS_DUP();
			break;
		}
	} else if (sbinfo) {
#ifndef CONFIG_BISCUITOS_5
		spin_lock(&sbinfo->stat_lock);
		sbinfo->free_inodes++;
		spin_unlock(&sbinfo->start_lock);
#else
		sbinfo->free_inodes++;
#endif
	}
	return inode;
}

static struct inode_operations shmem_dir_inode_operations_bs = {
#ifdef CONFIG_TMPFS_BS
	.create		= shmem_create_bs,
	.lookup		= simple_lookup,
	.link		= shmem_link_bs,
	.unlink		= shmem_unlink_bs,
	.symlink	= shmem_symlink_bs,
	.mkdir		= shmem_mkdir_bs,
	.rmdir		= shmem_rmdir_bs,
	.mknod		= shmem_mknod_bs,
	.rename		= shmem_rename_bs,
#ifdef CONFIG_TMPFS_XATTR
#ifdef CONFIG_BISCUITOS_5
	.listxattr	= generic_listxattr,
#else
	.setxattr	= generic_setxattr,
	.getxattr	= generic_getxattr,
	.removexattr	= generic_removexattr,
#endif
#endif
#endif
};

static struct file_operations shmem_file_operations_bs = {
	.mmap		= shmem_mmap_bs,
#ifdef CONFIG_TMPFS_BS
	.llseek		= generic_file_llseek,
	.read		= shmem_file_read_bs,
	.write		= shmem_file_write_bs,
#ifdef CONFIG_BISCUITOS_5
	.fsync		= noop_fsync,
#else
	.fsync		= simple_sync_file,
	.sendfile	= shmem_file_sendfile_bs,
#endif
#endif
};

static struct inode_operations shmem_inode_operations_bs = {
#ifndef CONFIG_BISCUITOS_5
	.truncate	= shmem_truncate_bs,
	.setxattr	= generic_setxattr,
	.getxattr	= generic_getxattr,
	.removexattr	= generic_removexattr,
#endif
	.setattr	= shmem_notify_change_bs,
	.listxattr	= generic_listxattr,
};

static struct inode_operations shmem_special_inode_operations_bs = {
#ifdef CONFIG_TMPFS_XATTR_BS
#ifndef CONFIG_BISCUITOS_5
	.setxattr	= generic_setxattr,
	.getxattr	= generic_getxattr,
	.removexattr	= generic_removexattr,
#endif
	.listxattr	= generic_listxattr,
#endif
};

static struct backing_dev_info __unused shmem_backing_dev_info_bs = {
	.ra_pages	= 0,	/* No readahead */
	.capabilities	= BDI_CAP_NO_ACCT_DIRTY | BDI_CAP_NO_WRITEBACK,
};

static struct address_space_operations shmem_aops_bs = {
	.writepage	= shmem_writepage_bs,
	.set_page_dirty = __set_page_dirty_nobuffers_bs,
#ifndef CONFIG_BISCUITOS_5
#ifdef CONFIG_TMPFS_BS
	.prepare_write	= shmem_prepare_write_bs,
	.commit_write	= simple_commit_write_bs,
#endif
#endif
};

static struct super_operations shmem_ops_bs = {
	.alloc_inode	= shmem_alloc_inode_bs,
	.destroy_inode	= shmem_destroy_inode_bs,
#ifdef CONFIG_TMPFS_BS
	.statfs		= shmem_statfs_bs,
	.remount_fs	= shmem_remount_fs_bs,
#endif
#ifndef CONFIG_BISCUITOS_5
	.delete_inode	= shmem_delete_inode_bs,
#endif
	.drop_inode	= generic_delete_inode,
	.put_super	= shmem_put_super_bs,
};

#ifdef CONFIG_TMPFS_XATTR_BS
static const struct xattr_handler *shmem_xattr_handlers_bs[] = {
#ifdef CONFIG_TMPFS_SECURITY_BS
	&shmem_xattr_security_handler_bs,
#endif
	NULL
};
#endif

static void init_once_bs(void *foo, kmem_cache_t_bs *cachep, 
						unsigned long flags)
{
	struct shmem_inode_info_bs *p = (struct shmem_inode_info_bs *)foo;

	if ((flags & (SLAB_CTOR_VERIFY_BS|SLAB_CTOR_CONSTRUCTOR_BS)) ==
					SLAB_CTOR_CONSTRUCTOR_BS) {
		inode_init_once(&p->vfs_inode);
	}
}

static int init_inodecache_bs(void)
{
	shmem_inode_cachep_bs = kmem_cache_create_bs(
					"shmem_inode_cache_bs",
					sizeof(struct shmem_inode_info_bs),
					0,
					0,
					init_once_bs,
					NULL);
	if (shmem_inode_cachep_bs == NULL)
		return -ENOMEM;

	return 0;
}

static void destroy_inodecache_bs(void)
{
	if (kmem_cache_destroy_bs(shmem_inode_cachep_bs))
		printk(KERN_INFO "shmem_inode_cache: not all structures "
						"were freed\n");
}

#ifdef CONFIG_TMPFS_XATTR_BS
static const struct xattr_handler *shmem_xattr_handlers_bs[];
#else
#define shmem_xattr_handlers_bs NULL
#endif

static int shmem_fill_super_bs(struct super_block *sb,
					void *data, int silent)
{
	struct inode *inode;
	struct dentry *root;
	int mode = S_IRWXUGO | S_ISVTX;
#ifndef CONFIG_BISCUITOS_5
	uid_t uid = current->fsuid;
	gid_t gid = current->fsgid;
#else
	/* default */
	uid_t uid;
	gid_t gid;
#endif
	int err = -ENOMEM;

#ifdef CONFIG_TMPFS_BS
	unsigned long blocks = 0;
	unsigned long inodes = 0;

	/*
	 * Per default we only allow half of the physical ram per
	 * tmpfs instance, limiting inodes to one per page of lowmem;
	 * but the internal instance is left unlimited.
	 */
	if (!(sb->s_flags & MS_NOUSER)) {
		blocks = totalram_pages_bs / 2;
		inodes = totalram_pages_bs - totalhigh_pages_bs;

		if (inodes > blocks)
			inodes = blocks;

		if (shmem_parse_options_bs(data, &mode,
				&uid, &gid, &blocks, &inodes))
			return -EINVAL;
	}

	if (blocks || inodes) {
		struct shmem_sb_info_bs *sbinfo;

		sbinfo = kmalloc_bs(sizeof(struct shmem_sb_info_bs), 
							GFP_KERNEL_BS);
		if (!sbinfo)
			return -ENOMEM;
		sb->s_fs_info = sbinfo;
		spin_lock_init(&sbinfo->stat_lock);
		sbinfo->max_blocks = blocks;
		sbinfo->free_blocks = blocks;
		sbinfo->max_inodes = inodes;
		sbinfo->free_inodes = inodes;
	}
	sb->s_xattr = shmem_xattr_handlers_bs;
#else
	sb->s_flags |= MS_NOUSER;
#endif

	sb->s_maxbytes = SHMEM_MAX_BYTES_BS;
	sb->s_blocksize = PAGE_CACHE_SIZE_BS;
	sb->s_blocksize_bits = PAGE_CACHE_SHIFT_BS;
	sb->s_magic = TMPFS_MAGIC_BS;
	sb->s_op = &shmem_ops_bs;
	inode = shmem_get_inode_bs(sb, S_IFDIR | mode, 0);
	if (!inode)
		goto failed;
#ifdef CONFIG_BISCUITOS_5
	inode->i_uid = current_fsuid();
	inode->i_gid = current_fsgid();
	root = d_make_root(inode);
#else
	inode->i_uid = uid;
	inode->i_gid = gid;
	root = d_alloc_root(inode);
#endif
	if (!root)
		goto failed_iput;
	sb->s_root = root;
	return 0;

failed_iput:
	iput(inode);
failed:
	shmem_put_super_bs(sb);
	return err;
}

#ifdef CONFIG_BISCUITOS_5
/* FIXME: Implement from linux 5.0 */
static struct dentry *shmem_mount_bs(struct file_system_type *fs_type,
		int flags, const char *dev_name, void *data)
#else
static struct super_block *shmem_get_bs_bs(struct file_system_type *fs_type,
		int flags, const char *dev_name, void *data)
#endif
{
	return mount_nodev(fs_type, flags, data, shmem_fill_super_bs);
}


static struct file_system_type tmpfs_fs_type_bs = {
	.owner		= THIS_MODULE,
	.name		= "tmpfs_bs",
#ifndef CONFIG_BISCUITOS_5
	.get_sb		= shmem_get_sb_bs,
#else
	.mount		= shmem_mount_bs,
#endif
	.kill_sb	= kill_litter_super,
};

static int __init init_tmpfs_bs(void)
{
	int error;

	error = init_inodecache_bs();
	if (error)
		goto out3;

	error = register_filesystem(&tmpfs_fs_type_bs);
	if (error) {
		printk(KERN_ERR "Could not register tmpfs.\n");
		goto out2;
	}

#ifndef CONFIG_BISCUITOS_5
#ifndef CONFIG_TMPFS_BS
	devfs_mk_dir("shm_bs");
#endif
	shm_mnt_bs = do_kern_mount(tmpfs_fs_type_bs.name, MS_NOUSER,
				tmpfs_fs_type_bs.name, NULL);
	if (IS_ERR(shm_mnt_bs)) {
		error = PTR_ERR(shm_mnt_bs);
		printk(KERN_ERR "Could not kern_mount tmpfs\n");
		goto out1;
	}
	return 0;
out1:
	unregister_filesystem(&tmpfs_fs_type_bs);
#else
	return 0;
#endif

out2:
	destroy_inodecache_bs();
out3:
	shm_mnt_bs = ERR_PTR(error);
	return error;
}
login_initcall_bs(init_tmpfs_bs);
