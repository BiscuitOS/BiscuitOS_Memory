#ifndef _BISCUITOS_ARM_STRING_H
#define _BISCUITOS_ARM_STRING_H

extern void __memzero_bs(void *ptr, __kernel_size_t n);

#define memzero_bs(p,n)							\
({									\
	void *__p = (p); size_t __n = n;				\
	if ((__n) != 0)							\
		__memzero_bs((__p),(__n));				\
	(__p);								\
})


#endif
