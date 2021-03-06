#ifndef _BISCUITOS_PERCPU_H
#define _BISCUITOS_PERCPU_H

/* Enough to cover all DEFINE_PER_CPUs in kernel, including modules. */
#ifndef PERCPU_ENOUGH_ROOM_BS
#define PERCPU_ENOUGH_ROOM_BS	32768
#endif

/* Must be an lvalue. */
#define get_cpu_var_bs(var) (*({ preempt_disable(); &__get_cpu_var_bs(var); }))
#define put_cpu_var_bs(var) preempt_enable()

#endif
