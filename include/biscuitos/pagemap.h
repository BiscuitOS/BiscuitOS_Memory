#ifndef _BISCUITOS_PAGEMAP_H
#define _BISCUITOS_PAGEMAP_H
/*
 * Copyright 1995 Linus Torvalds
 */

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
#endif
