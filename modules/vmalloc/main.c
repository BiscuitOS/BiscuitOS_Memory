/*
 * BiscuitOS Memory Manager: Vmalloc
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
#include "biscuitos/gfp.h"
#include "biscuitos/vmalloc.h"

/*
 * TestCase: vmalloc
 */
static int TestCase_vmalloc(void)
{
	void *data;

	/* Alloc */
	data = vmalloc_bs(PAGE_SIZE_BS);
	sprintf((char *)data, "BiscuitOS-%s", __func__);
	bs_debug("[%#lx] %s\n", (unsigned long)data, (char *)data);

	/* free */
	vfree_bs(data);
	return 0;
}
vmalloc_initcall_bs(TestCase_vmalloc);

/*
 * TestCase: vmalloc_32
 */
static int TestCase_vmalloc_32(void)
{
	void *data;

	/* Alloc */
	data = vmalloc_32_bs(PAGE_SIZE_BS);
	sprintf((char *)data, "BiscuitOS-%s", __func__);
	bs_debug("[%#lx] %s\n", (unsigned long)data, (char *)data);

	/* free */
	vfree_bs(data);
	return 0;
}
vmalloc_initcall_bs(TestCase_vmalloc_32);

/*
 * TestCase: vmalloc_exec
 */
static int TestCase_vmalloc_exec(void)
{
	void *data;

	/* Alloc */
	data = vmalloc_exec_bs(PAGE_SIZE);
	sprintf((char *)data, "BiscuitOS-%s", __func__);
	bs_debug("[%#lx] %s\n", (unsigned long)data, (char *)data);

	/* free */
	vfree_bs(data);
	return 0;
}
vmalloc_initcall_bs(TestCase_vmalloc_exec);

/*
 * TestCase: vmap/vunmap
 */
static int TestCase_vmap(void)
{
	struct page_bs *io_page;
	void *addr;
	int ret;

	/* Page indcate IO */
	io_page = alloc_page_bs(__GFP_HIGHMEM_BS);
	if (!io_page) {
		printk("%s alloc_page failed.\n", __func__);
		return -ENOMEM;
	}

	/* vmap */
	addr = vmap_bs(&io_page, 1, VM_IOREMAP_BS, PAGE_KERNEL_BS);
	if (!addr) {
		printk("%s vmap failed\n", __func__);
		ret = -EINVAL;
		goto out_vmap;
	}

	sprintf((char *)addr, "BiscuitOS-0-%s", __func__);
	bs_debug("[%#lx] %s\n", (unsigned long)addr, (char *)addr);

	/* unmap */
	vunmap_bs(addr);
	/* free page */
	free_page_bs((unsigned long)io_page);
	return 0;

out_vmap:
	free_page_bs((unsigned long)io_page);
	return ret;
}
vmalloc_initcall_bs(TestCase_vmap);
