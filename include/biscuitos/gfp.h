#ifndef _BISCUITOS_GFP_H
#define _BISCUITOS_GFP_H

/*
 * GFP bitmasks..
 */
/* Zone modifiers in GFP_ZONEMASK (see linux/mmzone.h - low two bits) */
#define __GFP_DMA_BS		0x01
#define __GFP_HIGHMEM_BS	0x02

/*
 * Action modifiers - doesn't change the zoning
 *
 * __GFP_REPEAT: Try hard to allocate the memory, but the allocation attempt
 * _might_ fail.  This depends upon the particular VM implementation.
 *
 * __GFP_NOFAIL: The VM implementation _must_ retry infinitely: the caller
 * cannot handle allocation failures.
 *
 * __GFP_NORETRY: The VM implementation must not retry indefinitely.
 */
#define __GFP_WAIT_BS		0x10u	/* Can wait and reschedule? */
#define __GFP_HIGH_BS		0x20u	/* Should access emergency pools? */
#define __GFP_IO_BS		0x40u	/* Can start physical IO? */ 
#define __GFP_FS_BS		0x80u	/* Can call down to low-level FS? */
#define __GFP_COLD_BS		0x100u	/* Cache-cold page required */
#define __GFP_NOWARN_BS		0x200u	/* Suppress page allocation failure warning */
#define __GFP_REPEAT_BS		0x400u	/* Retry the allocation.  Might fail */
#define __GFP_NOFAIL_BS		0x800u	/* Retry for ever.  Cannot fail */
#define __GFP_NORETRY_BS	0x1000u	/* Do not retry.  Might fail */
#define __GFP_NO_GROW_BS	0x2000u	/* Slab internal usage */
#define __GFP_COMP_BS		0x4000u	/* Add compound page metadata */
#define __GFP_ZERO_BS		0x8000u	/* Return zeroed page on success */
#define __GFP_NOMEMALLOC_BS	0x10000u /* Don't use emergency reserves */

extern void __free_pages_bs(struct page_bs *page, unsigned int order);

#define __free_page_bs(page)	__free_pages_bs((page), 0)

#ifndef HAVE_ARCH_FREE_PAGE_BS
static inline void arch_free_page_bs(struct page_bs *page, int order) { }
#endif

#endif
