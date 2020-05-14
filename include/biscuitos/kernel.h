#ifndef _BISCUITOS_KERNEL_H
#define _BISCUITOS_KERNEL_H

#define __unused	__attribute__((__unused__))

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
