/*
 * BiscuitOS Memory Manager: Kmap
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
#include "biscuitos/highmem.h"

/*
 * TestCase: kmap and kunmap
 */
static int TestCase_kmap(void)
{
	struct page_bs *kmap_page;
	void *addr;

	/* alloc an unmap page */
	kmap_page = alloc_page_bs(GFP_KERNEL_BS);
	if (!kmap_page) {
		printk("%s alloc_page() failed.\n", __func__);
		return -ENOMEM;
	}

	/* kmap */
	addr = kmap_atomic_bs(kmap_page, KM_USER0_BS);
	if (!addr) {
		printk("%s kmap_atomic failed\n", __func__);
		free_page_bs((unsigned long)kmap_page);
		return -ENOMEM;
	}

	sprintf((char *)addr, "BiscuitOS-%s", __func__);
	bs_debug("[%#lx] %s\n", (unsigned long)addr, (char *)addr);

	/* kunmap */
	kunmap_atomic_bs(addr, KM_USER0_BS);

	/* free page */
	free_page_bs((unsigned long)kmap_page);

	return 0;
}
kmap_initcall_bs(TestCase_kmap);
