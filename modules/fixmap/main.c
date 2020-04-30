/*
 * BiscuitOS Memory Manager: FIXMAP
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
#include "biscuitos/mmzone.h"
#include "biscuitos/gfp.h"
#include "biscuitos/highmem.h"
#include "asm-generated/tlbflush.h"
#include "asm-generated/highmem.h"

/*
 * TestCase - FIXMAP
 */
static int TestCase_fixmap(void)
{
	enum fixed_addresses_bs idx = KM_IRQ0_BS;
	struct page_bs *page, *conform_page;
	void *addr;

	/* alloc */
	page = alloc_page_bs(__GFP_HIGHMEM_BS);
	if (!page || !PageHighMem_bs(page)) {
		printk("%s alloc_page() failed.\n", __func__);
		return -ENOMEM;
	}

	/* Fixmap */
	addr = (void *)__fix_to_virt_bs(FIX_KMAP_BEGIN_BS + idx);
	set_pte_bs(kmap_pte_bs - idx, mk_pte_bs(page, kmap_prot_bs));
	/* flush tlb */
	__flush_tlb_one_bs((unsigned long)addr);

	/* Conform */
	conform_page = kmap_atomic_to_page_bs(addr);
	bs_debug("Default Page: %#lx Conform Page: %#lx\n", 
			(unsigned long)page, (unsigned long)conform_page);

	sprintf((char *)addr, "BiscuitOS-%s", __func__);
	bs_debug("[%#lx] %s\n", (unsigned long)addr, (char *)addr);

	/* no need unmap */
	free_page_bs((unsigned long)page);
	return 0;
}
fixmap_initcall_bs(TestCase_fixmap);
