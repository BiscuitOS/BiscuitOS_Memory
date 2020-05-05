#ifndef _BISCUITOS_CPUMASK_H
#define _BISCUITOS_CPUMASK_H

#include <linux/bitmap.h>
#include <linux/cpumask.h>
#include "asm-generated/arch.h"
#include "asm-generated/types.h"

#define CPU_MASK_LAST_WORD_BS BITMAP_LAST_WORD_MASK(NR_CPUS_BS)

#if NR_CPUS_BS <= BITS_PER_LONG_BS

#define CPU_MASK_ALL_BS							\
(cpumask_t) { {								\
        [BITS_TO_LONGS(NR_CPUS_BS)-1] = CPU_MASK_LAST_WORD_BS		\
} }

#else

#define CPU_MASK_ALL_BS							\
(cpumask_t) { {								\
        [0 ... BITS_TO_LONGS(NR_CPUS_BS)-2] = ~0UL,			\
        [BITS_TO_LONGS(NR_CPUS_BS)-1] = CPU_MASK_LAST_WORD_BS		\
} }

#endif

#define cpus_weight_bs(cpumask) __cpus_weight_bs(&(cpumask), NR_CPUS_BS)
static inline int __cpus_weight_bs(const cpumask_t *srcp, int nbits)
{
	return bitmap_weight(srcp->bits, nbits);
}

#endif
