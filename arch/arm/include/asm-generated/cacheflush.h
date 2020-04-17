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

#endif
