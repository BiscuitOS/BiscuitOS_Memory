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
	sprintf((char *)data, "BiscuitOS");
	bs_debug("%s\n", (char *)data);

	/* free */
	vfree_bs(data);
	return 0;
}
vmalloc_initcall_bs(TestCase_vmalloc);
