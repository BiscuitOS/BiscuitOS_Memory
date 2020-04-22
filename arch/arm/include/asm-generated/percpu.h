#ifndef _BISCUITOS_ARM_PERCPU_H
#define _BISCUITOS_ARM_PERCPU_H

extern unsigned long __per_cpu_offset_bs[NR_CPUS_BS];

/* Separete out the type, so (int[3], foo) works. */
#define DEFINE_PER_CPU_BS(type, name) \
__attribute__((__section__(".data.percpu_bs"))) __typeof__(type) per_cpu__##name

/* var is in discarded region: offset to particular copy we want */
#define per_cpu_bs(var, cpu)	\
		(*RELOC_HIDE(&per_cpu__##var, __per_cpu_offset_bs[cpu]))
#define __get_cpu_var_bs(var)	per_cpu_bs(var, smp_processor_id())

#endif
