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

#define SI_LOAD_SHIFT   16
struct sysinfo_bs {
	long uptime;			/* Seconds since boot */
	unsigned long loads[3];		/* 1, 5, and 15 minute load averages */
	unsigned long totalram;		/* Total usable main memory size */
	unsigned long freeram;		/* Available memory size */
	unsigned long sharedram;	/* Amount of shared memory */
	unsigned long bufferram;	/* Memory used by buffers */
	unsigned long totalswap;	/* Total swap space size */
	unsigned long freeswap;		/* swap space still available */
	unsigned short procs;		/* Number of current processes */
	unsigned short pad;		/* explicit padding for m68k */
	unsigned long totalhigh;	/* Total high memory size */
	unsigned long freehigh;		/* Available high memory size */
	unsigned int mem_unit;		/* Memory unit size in bytes */
	char _f[20-2*sizeof(long)-sizeof(int)];	/* Padding: libc5 uses this.. */
};

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
