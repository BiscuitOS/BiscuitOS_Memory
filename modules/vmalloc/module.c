/*
 * BiscuitOS Memory Manager: VMALLOC
 *
 * (C) 2019.10.01 BuddyZhang1 <buddy.zhang@aliyun.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>

#include "biscuitos/kernel.h"
#include "biscuitos/mm.h"
#include "biscuitos/gfp.h"
#include "biscuitos/vmalloc.h"

/*
 * TestCase: vmalloc()/vfree()
 */
static int TestCase_vmalloc(void)
{
	void *addr;

	/* Alloc */
	addr = vmalloc_bs(PAGE_SIZE);
	if (!addr) {
		printk("%s VMALLOC alloc failed.\n", __func__);
		return -ENOMEM;
	}
	sprintf(addr, "BiscuitOS VMALLOC Module");
	printk("[%#lx] %s\n", (unsigned long)addr, (char *)addr);

	/* free */
	vfree_bs(addr);
	return 0;
}

/* Module initialize entry */
static int __init VMALLOC_Allocator_init(void)
{

	TestCase_vmalloc();

	return 0;
}

/* Module exit entry */
static void __exit VMALLOC_Allocator_exit(void)
{
}

module_init(VMALLOC_Allocator_init);
module_exit(VMALLOC_Allocator_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("BiscuitOS <buddy.zhang@aliyun.com>");
MODULE_DESCRIPTION("VMALLOC Allocator Test Case");
