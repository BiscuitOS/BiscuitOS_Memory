/*
 *      linux/mm/filemap.c
 *
 * Copyright (C) 1994-1999  Linus Torvalds
 */
#include <linux/kernel.h>
#include <linux/spinlock.h>
#include <linux/fs.h>
#include <linux/pagemap.h>
#include "biscuitos/kernel.h"
#include "biscuitos/fs.h"
#include "biscuitos/mm.h"
#include "biscuitos/gfp.h"

/**
 * find_get_entry - find and get a page cache entry
 * @mapping: the address_space to search
 * @offset: the page cache index
 *
 * Looks up the page cache slot at @mapping & @offset.  If there is a
 * page cache page, it is returned with an increased refcount.
 *
 * If the slot holds a shadow entry of a previously evicted page, or a
 * swap entry from shmem/tmpfs, it is returned.
 *
 * Otherwise, %NULL is returned.
 */
struct page_bs *find_get_entry_bs(struct address_space *mapping, 
							pgoff_t offset)
{
#ifdef CONFIG_BISCUITOS_5
	struct page_bs *page_bs;
	struct page *page = find_lock_page(mapping, offset);

	if (page) {
		/* allocate a page */
		page_bs = alloc_page_bs(GFP_KERNEL_BS);
		PAGE_TO_PAGE_BS(page, page_bs);
	} else {
		return NULL;
	}
	return page_bs;
#else
	;
#endif
}

/**             
 * find_lock_page - locate, pin and lock a pagecache page
 *
 * @mapping: the address_space to search
 * @offset: the page index
 *
 * Locates the desired pagecache page, locks it, increments its reference
 * count and returns its address.
 *
 * Returns zero if the page was not present. find_lock_page() may sleep.
 */
struct shmem_page *find_lock_page_bs(struct address_space *mapping,
						unsigned long offset)
{
#ifdef CONFIG_BISCUITOS_5
	return (struct shmem_page *)find_lock_page(mapping, offset);
#endif
}
EXPORT_SYMBOL_GPL(find_lock_page_bs);

int add_to_page_cache_lru_bs(struct page_bs *page, 
		struct address_space *mapping, pgoff_t offset, int gfp_mask)
{
	return 0;
}
