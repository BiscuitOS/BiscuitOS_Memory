#ifndef _BISCUITOS_KERNEL_H
#define _BISCUITOS_KERNEL_H

#include "asm-generated/bug.h"
#include "biscuitos/linkage.h"

#define __unused	__attribute__((__unused__))

static inline int __attribute__((pure)) long_log2_bs(unsigned long x)
{
	int r = 0;
	for (x >>= 1; x > 0; x >>= 1)
		r++;
	return r;
}

#define BS_DUP()  printk("Expand..[%s-%s-%d]\n", __FILE__, __func__, __LINE__)
#define BS_DONE() printk("Done..[%s-%s-%d]\n", __FILE__, __func__, __LINE__)

#define bs_debug_enable()				\
({							\
	extern int BiscuitOS_debug;			\
	BiscuitOS_debug = 1;				\
})

#define bs_debug_disable()				\
({							\
	extern int BiscuitOS_debug;			\
	BiscuitOS_debug = 0;				\
})

#define bs_debug(...)					\
({							\
	extern int BiscuitOS_debug;			\
							\
	if (BiscuitOS_debug)				\
		printk(__VA_ARGS__);			\
})

#endif
