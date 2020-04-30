/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _BISCUITOS_ARM_CACHETYPE_H
#define _BISCUITOS_ARM_CACHETYPE_H

#define CACHEID_VIVT_BS			(1 << 0)
#define CACHEID_VIPT_NONALIASING_BS	(1 << 1)
#define CACHEID_VIPT_ALIASING_BS		(1 << 2)
#define CACHEID_VIPT_BS			(CACHEID_VIPT_ALIASING_BS | \
					 CACHEID_VIPT_NONALIASING_BS)
#define CACHEID_ASID_TAGGED_BS		(1 << 3)
#define CACHEID_VIPT_I_ALIASING_BS	(1 << 4)
#define CACHEID_PIPT_BS			(1 << 5)

extern unsigned int cacheid_bs; 
                        
#define cache_is_vivt_bs()		cacheid_is_bs(CACHEID_VIVT_BS)
#define cache_is_vipt_bs()		cacheid_is_bs(CACHEID_VIPT_BS)
#define cache_is_vipt_nonaliasing_bs()					\
				cacheid_is_bs(CACHEID_VIPT_NONALIASING_BS)
#define cache_is_vipt_aliasing_bs()	cacheid_is_bs(CACHEID_VIPT_ALIASING_BS)
#define icache_is_vivt_asid_tagged_bs()	cacheid_is_bs(CACHEID_ASID_TAGGED_BS)
#define icache_is_vipt_aliasing_bs()					\
				cacheid_is_bs(CACHEID_VIPT_I_ALIASING_BS)
#define icache_is_pipt_bs()		cacheid_is_bs(CACHEID_PIPT_BS)

/*                      
 * __LINUX_ARM_ARCH__ is the minimum supported CPU architecture
 * Mask out support which will never be present on newer CPUs.
 * - v6+ is never VIVT  
 * - v7+ VIPT never aliases on D-side
 */                     
#if __LINUX_ARM_ARCH__ >= 7
#define __CACHEID_ARCH_MIN_BS	(CACHEID_VIPT_NONALIASING_BS |\
				CACHEID_ASID_TAGGED_BS |\
				CACHEID_VIPT_I_ALIASING_BS |\
				CACHEID_PIPT_BS)
#elif __LINUX_ARM_ARCH__ >= 6
#define __CACHEID_ARCH_MIN_BS	(~CACHEID_VIVT_BS)
#else
#define __CACHEID_ARCH_MIN_BS	(~0)
#endif

/*
 * Mask out support which isn't configured
 */
#if defined(CONFIG_CPU_CACHE_VIVT) && !defined(CONFIG_CPU_CACHE_VIPT)
#define __CACHEID_ALWAYS_BS		(CACHEID_VIVT_BS)
#define __CACHEID_NEVER_BS		(~CACHEID_VIVT_BS)
#elif !defined(CONFIG_CPU_CACHE_VIVT) && defined(CONFIG_CPU_CACHE_VIPT)
#define __CACHEID_ALWAYS_BS		(0)
#define __CACHEID_NEVER_BS		(CACHEID_VIVT_BS)
#else
#define __CACHEID_ALWAYS_BS		(0)
#define __CACHEID_NEVER_BS		(0)
#endif

static inline unsigned int cacheid_is_bs(unsigned int mask)
{
	return (__CACHEID_ALWAYS_BS & mask) |
	       (~__CACHEID_NEVER_BS & __CACHEID_ARCH_MIN_BS & 
						mask & cacheid_bs);
}

#endif
