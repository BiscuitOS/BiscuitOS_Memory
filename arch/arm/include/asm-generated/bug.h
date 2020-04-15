#ifndef _BISCUITOS_ARM_BUG_H
#define _BISCUITOS_ARM_BUG_H

#define likely(x)	__builtin_expect(!!(x), 1)
#define unlikely(x)	__builtin_expect(!!(x), 0)

#define BUG() do {						\
	printk("Kernel BUG at %s:%d!\n", __FILE__, __LINE__);	\
	panic("BUG!");						\
} while (0)


#define BUG_ON(condition) do { 					\
	if (unlikely((condition)!=0)) 				\
		BUG(); 						\
} while (0)

#endif
