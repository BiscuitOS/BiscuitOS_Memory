#ifndef _BISCUITOS_ARM_SMP_PLAT_H
#define _BISCUITOS_ARM_SMP_PLAT_H

#include <asm/cputype.h>

/*
 * Return true if we are running on a SMP platform
 */
static inline bool is_smp(void)
{
	return false;
}

static inline int tlb_ops_need_broadcast_bs(void)
{
	if (!is_smp())
		return 0;

	return ((read_cpuid_ext(CPUID_EXT_MMFR3) >> 12) & 0xf) < 2;
}

#endif
