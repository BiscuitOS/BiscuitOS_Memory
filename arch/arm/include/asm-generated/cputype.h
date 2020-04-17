#ifndef _BISCUITOS_ARM_CPUTYPE_H
#define _BISCUITOS_ARM_CPUTYPE_H

#define CPUID_ID		0
#define CPUID_CACHETYPE		1
#define CPUID_TCM		2
#define CPUID_TLBTYPE		3
#define CPUID_MPUIR		4
#define CPUID_MPIDR		5
#define CPUID_REVIDR		6

#define CPUID_EXT_MMFR0		"c1, 4"

#define __stringify_1_bs(x...)	#x
#define __stringify_bs(x...)	__stringify_1_bs(x)

#ifdef CONFIG_CPU_CP15

#define read_cpuid_bs(reg)						\
({									\
	unsigned int __val;						\
	asm("mrc	p15, 0, %0, c0, c0, " __stringify_bs(reg)	\
	    : "=r" (__val)						\
	    : 								\
	    : "cc");							\
	__val;								\
})

/*              
 * The memory clobber prevents gcc 4.5 from reordering the mrc before
 * any is_smp() tests, which can cause undefined instruction aborts on
 * ARM1136 r0 due to the missing extended CP15 registers.
 */
#define read_cpuid_ext_bs(ext_reg)					\
({									\
	unsigned int __val;						\
	asm("mrc	p15, 0, %0, c0, " ext_reg			\
	    : "=r" (__val)						\
	    :								\
	    : "memory");						\
	__val;								\
})

/*
 * The CPU ID never changes at run time, so we might as well tell the
 * compiler that it's constant. Use this function to read the CPU ID
 * rather than directly reading processor_id or read_cpuid() directly.
 */
static inline unsigned int __attribute_const__ read_cpuid_id_bs(void)
{
	return read_cpuid_bs(CPUID_ID);
}

#endif

#endif
