#ifndef _BISCUITOS_PAGEMAP_H
#define _BISCUITOS_PAGEMAP_H
#include <linux/fs.h>
#include "biscuitos/gfp.h"

/*
 * Copyright 1995 Linus Torvalds
 */

static inline unsigned int
__nocast mapping_gfp_mask_bs(struct address_space *mapping)
{
	return mapping->flags & __GFP_BITS_MASK_BS;
}

/*
 * The page cache can done in larger chunks than
 * one page, because it allows for more efficient
 * throughput (it can then be mapped into user
 * space in smaller chunks for same flexibility).
 *
 * Or rather, it _will_ be done in larger chunks.
 */
#define PAGE_CACHE_SHIFT_BS	PAGE_SHIFT_BS
#define PAGE_CACHE_SIZE_BS	PAGE_SIZE_BS

extern struct shmem_page *find_lock_page_bs(struct address_space *mapping,
							unsigned long offset);
extern int add_to_page_cache_lru_bs(struct page_bs *page,
		struct address_space *mapping, pgoff_t offset, int gfp_mask);
#endif
