#ifndef _BISCUITOS_CACHE_H
#define _BISCUITOS_CACHE_H

#include "asm-generated/cache.h"

#ifndef SMP_CACHE_BYTES_BS
#define SMP_CACHE_BYTES_BS	L1_CACHE_BYTES_BS
#endif

#ifndef ____cacheline_aligned_in_smp_bs
#ifdef CONFIG_SMP
#define ____cacheline_aligned_in_smp_bs	____cacheline_aligned
#else
#define ____cacheline_aligned_in_smp_bs
#endif /* CONFIG_SMP */
#endif

#if !defined(____cacheline_maxaligned_in_smp_bs)
#if defined(CONFIG_SMP)
#define ____cacheline_maxaligned_in_smp_bs 			\
		__attribute__((__aligned__(1 << (L1_CACHE_SHIFT_MAX_BS))))
#else
#define ____cacheline_maxaligned_in_smp_bs
#endif
#endif

#ifndef __cacheline_aligned_in_smp_bs
#ifdef CONFIG_SMP
#define __cacheline_aligned_in_smp_bs	__cacheline_aligned
#else   
#define __cacheline_aligned_in_smp_bs
#endif /* CONFIG_SMP */ 
#endif

/*
 * The maximum alignment needed for some critical structures
 * These could be inter-node cacheline sizes/L3 cacheline
 * size etc.  Define this in asm/cache.h for your arch
 */
#ifndef INTERNODE_CACHE_SHIFT_BS
#define INTERNODE_CACHE_SHIFT_BS	L1_CACHE_SHIFT_BS
#endif

#if !defined(____cacheline_internodealigned_in_smp_bs)
#if defined(CONFIG_SMP)
#define ____cacheline_internodealigned_in_smp_bs \
        __attribute__((__aligned__(1 << (INTERNODE_CACHE_SHIFT_BS))))
#else
#define ____cacheline_internodealigned_in_smp_bs
#endif
#endif

#endif
