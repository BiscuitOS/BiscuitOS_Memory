#ifndef _BISCUITOS_ARM_CACHEFLUSH_H
#define _BISCUITOS_ARM_CACHEFLUSH_H

#include <asm/glue.h>

/*
 *      Cache Model
 *      ===========
 */
#undef _CACHE
#undef MULTI_CACHE

#if defined(CONFIG_CPU_V7)
#	ifdef _CACHE
#		define MULTI_CACHE 1
#	else
#		define _CACHE v7
#	endif
#endif

extern void v7_flush_kern_cache_all_bs(void);

#ifndef MULTI_CACHE
#define __cpuc_flush_kern_all_bs	__glue(_CACHE,_flush_kern_cache_all_bs)
#endif

#define flush_cache_all_bs()		__cpuc_flush_kern_all_bs()

/*
 * flush_cache_vmap() is used when creating mappings (eg, via vmap,
 * vmalloc, ioremap etc) in kernel space for pages.  Since the
 * direct-mappings of these pages may contain cached data, we need 
 * to do a full cache flush to ensure that writebacks don't corrupt
 * data placed into these pages via the new mappings.
 */
#define flush_cache_vmap_bs(start, end)		flush_cache_all_bs()
#define flush_cache_vunmap_bs(start, end)	flush_cache_all_bs()

/*
 * Perform necessary cache operations to ensure that the TLB will
 * see data written in the specified area.
 */     
#define clean_dcache_area_bs(start,size)	do { } while (0)

#define flush_dcache_page_bs(page)	do { } while (0)

#endif
