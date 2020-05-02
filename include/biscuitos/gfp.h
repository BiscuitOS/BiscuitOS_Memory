#ifndef _BISCUITOS_GFP_H
#define _BISCUITOS_GFP_H

#include "biscuitos/mmzone.h"
#include "biscuitos/linkage.h"

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

#define __GFP_BITS_SHIFT_BS	20     /* Room for 20 __GFP_FOO bits */
#define __GFP_BITS_MASK_BS	((1 << __GFP_BITS_SHIFT_BS) - 1)

/* if you forget to add the bitmask here kernel will crash, period */
#define GFP_LEVEL_MASK_BS	(__GFP_WAIT_BS | __GFP_HIGH_BS | \
				 __GFP_IO_BS | __GFP_FS_BS | \
				 __GFP_FS_BS | __GFP_COLD_BS | \
				 __GFP_NOWARN_BS | __GFP_REPEAT_BS | \
				 __GFP_NOFAIL_BS | __GFP_NORETRY_BS | \
				 __GFP_NO_GROW_BS | __GFP_COMP_BS | \
				 __GFP_NOMEMALLOC_BS)

#define GFP_ATOMIC_BS		(__GFP_HIGH_BS)
#define GFP_NOIO_BS		(__GFP_WAIT_BS)
#define GFP_NOFS_BS		(__GFP_WAIT_BS | __GFP_IO_BS)
#define GFP_KERNEL_BS		(__GFP_WAIT_BS | __GFP_IO_BS | __GFP_FS_BS)
#define GFP_USER_BS		(__GFP_WAIT_BS | __GFP_IO_BS | __GFP_FS_BS)

/* Flag - indicates that the buffer will be suitable for DMA.  Ignored on some
   platforms, used as appropriate on others */

#define GFP_DMA_BS		__GFP_DMA_BS

extern void __free_pages_bs(struct page_bs *page, unsigned int order);

#define __free_page_bs(page)	__free_pages_bs((page), 0)

extern void FASTCALL_BS(free_pages_bs(unsigned long addr, unsigned int order));

extern void arch_free_page_bs(struct page_bs *page, int order);

extern struct page_bs *
FASTCALL_BS(__alloc_pages_bs(unsigned int, unsigned int, struct zonelist_bs *));

static inline struct page_bs *alloc_pages_node_bs(int nid,
			unsigned int __nocast gfp_mask, unsigned int order)
{
	if (unlikely(order >= MAX_ORDER_BS))
		return NULL;

	return __alloc_pages_bs(gfp_mask, order,
			NODE_DATA_BS(nid)->node_zonelists +
			(gfp_mask & GFP_ZONEMASK_BS));
}

#define alloc_pages_bs(gfp_mask, order)	\
		alloc_pages_node_bs(numa_node_id_bs(), gfp_mask, order)
#define alloc_page_bs(gfp_mask)	alloc_pages_bs(gfp_mask, 0)

extern unsigned long
FASTCALL_BS(__get_free_pages_bs(unsigned int __nocast gfp_mask, unsigned int order));
extern unsigned long
FASTCALL_BS(get_zeroed_page_bs(unsigned int __nocast gfp_mask));

#define __get_free_page_bs(gfp_mask)				\
		__get_free_pages_bs((gfp_mask), 0)

#define free_page_bs(addr)	free_pages_bs((addr), 0)
#endif
