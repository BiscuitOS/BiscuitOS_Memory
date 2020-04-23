/*
 * BiscuitOS Memory Manager: Buddy
 *
 * (C) 2019.10.01 BuddyZhang1 <buddy.zhang@aliyun.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/kernel.h>
#include "biscuitos/init.h"

static int bootmem_debug_entry(void)
{
	printk("BOOTMEM......\n");

	return 0;
}
bootmem_initcall_bs(bootmem_debug_entry);
