#ifndef _BISCUITOS_ARM_BUG_H
#define _BISCUITOS_ARM_BUG_H

#ifndef likely 
#define likely(x)	__builtin_expect(!!(x), 1)
#endif
#ifndef unlikely
#define unlikely(x)	__builtin_expect(!!(x), 0)
#endif

#define BUG_BS() do {						\
	printk("Kernel BUG at %s:%d!\n", __FILE__, __LINE__);	\
	panic("BUG!");						\
} while (0)


#define BUG_ON_BS(condition) do { 					\
	if (unlikely((condition)!=0)) 				\
		BUG_BS(); 						\
} while (0)

#endif
