/*
 * page_alloc
 *
 * (C) 2020.04.14 BuddyZhang1 <buddy.zhang@aliyun.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/kernel.h>
#include "biscuitos/nodemask.h"
#include "biscuitos/mmzone.h"
#include "biscuitos/bootmem.h"

nodemask_t_bs node_online_map_bs = { { [0] = 1UL } };

static bootmem_data_t contig_bootmem_data_bs;
struct pglist_data contig_page_data_bs = { .bdata = &contig_bootmem_data_bs };
EXPORT_SYMBOL_GPL(contig_page_data_bs);
