/*
 * arm/kernel/init_task.c
 *
 * (C) 2020.04.14 BuddyZhang1 <buddy.zhang@aliyun.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/kernel.h>
#include "asm-generated/pgtable.h"

pgd_t_bs swapper_pg_dir_array_bs[2048];

struct mm_struct_bs init_mm_bs = {
	.pgd = swapper_pg_dir_array_bs,
};
EXPORT_SYMBOL_GPL(init_mm_bs);
