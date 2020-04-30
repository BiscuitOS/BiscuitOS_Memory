/*
 * mm/page-writeback.c.
 *
 * Copyright (C) 2002, Linus Torvalds.
 *
 * Contains functions related to writing back dirty pages at the
 * address_space level.
 *
 * 10Apr2002    akpm@zip.com.au
 *              Initial version
 */
#include <linux/kernel.h>
#include <linux/mm.h>
#include "biscuitos/kernel.h"


/*
 * For address_spaces which do not use buffers.  Just tag the page as dirty in
 * its radix tree.
 *
 * This is also used when a single buffer is being dirtied: we want to set the
 * page dirty in that case, but not all the buffers.  This is a "bottom-up"
 * dirtying, whereas __set_page_dirty_buffers() is a "top-down" dirtying.
 *
 * Most callers have locked the page, which pins the address_space in memory.
 * But zap_pte_range() does not lock the page, however in that case the
 * mapping is pinned by the vma's ->vm_file reference.
 *
 * We take care to handle the case where the page was truncated from the
 * mapping by re-checking page_mapping() insode tree_lock.
 */
int __set_page_dirty_nobuffers_bs(struct page *page)
{
	BS_DUP();
	return 0;
}
EXPORT_SYMBOL_GPL(__set_page_dirty_nobuffers_bs);
