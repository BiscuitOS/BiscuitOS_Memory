#ifndef _BISCUITOS_CPUSET_H
#define _BISCUITOS_CPUSET_H

#include "biscuitos/types.h"

static inline int cpuset_zone_allowed_bs(struct zone_bs *z, gfp_t_bs gfp_mask)
{
	return 1;
}

static inline void cpuset_memory_pressure_bump_bs(void) {}

#endif
