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

#endif
