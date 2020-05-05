#ifndef _BISCUITOS_ARM_SMP_PLAT_H
#define _BISCUITOS_ARM_SMP_PLAT_H

#include <linux/cpumask.h>
#include <asm/cputype.h>
#include "biscuitos/cpumask.h"

extern cpumask_t cpu_online_map_bs;

#ifndef node_to_cpumask_bs
#define node_to_cpumask_bs(node)	(cpu_online_map_bs)
#endif

#ifndef nr_cpus_node_bs
#define nr_cpus_node_bs(node)						\
({									\
		cpumask_t __tmp__;					\
		__tmp__ = node_to_cpumask_bs(node);			\
		cpus_weight_bs(__tmp__);				\
})
#endif

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
