#ifndef _BISCUITOS_PERCPU_H
#define _BISCUITOS_PERCPU_H

#include "asm-generated/arch.h"

/* Enough to cover all DEFINE_PER_CPUs in kernel, including modules. */
#ifndef PERCPU_ENOUGH_ROOM_BS
#define PERCPU_ENOUGH_ROOM_BS	32768
#endif

/* Must be an lvalue. */
#define get_cpu_var_bs(var) (*({ preempt_disable(); &__get_cpu_var_bs(var); }))
#define put_cpu_var_bs(var) preempt_enable()

#ifdef CONFIG_SMP

struct percpu_data_bs {
	void *ptrs[NR_CPUS_BS];
	void *blkp;
};

/* 
 * Use this to get to a cpu's version of the per-cpu object allocated using
 * alloc_percpu.  Non-atomic access to the current CPU's version should
 * probably be combined with get_cpu()/put_cpu().
 */
#define per_cpu_ptr_bs(ptr, cpu)					\
({									\
	struct percpu_data_bs *__p = 					\
			(struct percpu_data_bs *)~(unsigned long)(ptr);	\
	(__typeof__(ptr))__p->ptrs[(cpu)];				\
})

extern void *__alloc_percpu_bs(size_t size, size_t align);
extern void free_percpu_bs(const void *);

#else /* CONFIG_SMP */

#define per_cpu_ptr_bs(ptr, cpu) (ptr)

static inline void *__alloc_percpu_bs(size_t size, size_t align)
{
	void *ret = kmalloc_bs(size, GFP_KERNEL_BS);
	if (ret)
		memset(ret, 0, size);
	return ret;
}
static inline void free_percpu_bs(const void *ptr)
{
	kfree_bs(ptr);
}

#endif /* CONFIG_SMP */

/* Simple wrapper for the common case: zeros memory. */
#define alloc_percpu_bs(type) \
	((type *)(__alloc_percpu_bs(sizeof(type), __alignof__(type))))

#endif
