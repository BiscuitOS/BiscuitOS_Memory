#ifndef _BISCUITOS_ARM_TLBFLUSH_H
#define _BISCUITOS_ARM_TLBFLUSH_H

#define TLB_V4_U_PAGE	(1 << 1)
#define TLB_V4_D_PAGE	(1 << 2)
#define TLB_V4_I_PAGE	(1 << 3)
#define TLB_V6_U_PAGE	(1 << 4)
#define TLB_V6_D_PAGE	(1 << 5)
#define TLB_V6_I_PAGE	(1 << 6)

#define TLB_V4_U_FULL	(1 << 9)
#define TLB_V4_D_FULL	(1 << 10)
#define TLB_V4_I_FULL	(1 << 11)
#define TLB_V6_U_FULL	(1 << 12)
#define TLB_V6_D_FULL	(1 << 13)
#define TLB_V6_I_FULL	(1 << 14)

#define TLB_V6_U_ASID	(1 << 16)
#define TLB_V6_D_ASID	(1 << 17)
#define TLB_V6_I_ASID	(1 << 18)

#define TLB_V6_BP	(1 << 19)

/* Unified Inner Shareable TLB operations (ARMv7 MP extensions) */
#define TLB_V7_UIS_PAGE	(1 << 20)
#define TLB_V7_UIS_FULL	(1 << 21)
#define TLB_V7_UIS_ASID	(1 << 22)
#define TLB_V7_UIS_BP	(1 << 23)

#define TLB_BARRIER	(1 << 28)
#define TLB_L2CLEAN_FR	(1 << 29)               /* Feroceon */
#define TLB_DCLEAN	(1 << 30)
#define TLB_WB		(1 << 31)

extern unsigned long BiscuitOS_tlb_flags;
#define __cpu_tlb_flags_bs		BiscuitOS_tlb_flags

#define v4_tlb_flags			(TLB_V4_U_FULL | TLB_V4_U_PAGE)

/*
 * MMU TLB Model
 * =============
 *
 * We have the following to choose from:
 *   v4    - ARMv4 without write buffer
 *   v4wb  - ARMv4 with write buffer without I TLB flush entry instruction
 *   v4wbi - ARMv4 with write buffer with I TLB flush entry instruction
 *   fr    - Feroceon (v4wbi with non-outer-cacheable page table walks)
 *   fa    - Faraday (v4 with write buffer with UTLB)
 *   v6wbi - ARMv6 with write buffer with I TLB flush entry instruction
 *   v7wbi - identical to v6wbi
 */

#undef _TLB
#undef MULTI_TLB

#ifdef CONFIG_SMP_ON_UP
#define MULTI_TLB 1
#endif

#ifdef CONFIG_CPU_TLB_V4WT
#	define v4_possible_flags	v4_tlb_flags
#	define v4_always_flags		v4_tlb_flags
#	ifdef _TLB
#		define MULTI_TLB	1
#	else
#		define _TLB		v4
#	endif
#else
#	define v4_possible_flags	0
#	define v4_always_flags		(-1UL)
#endif

#define v4wbi_tlb_flags	(TLB_WB | TLB_DCLEAN | \
			TLB_V4_I_FULL | TLB_V4_D_FULL | \
			TLB_V4_I_PAGE | TLB_V4_D_PAGE)

#ifdef CONFIG_CPU_TLB_V4WBI
#	define v4wbi_possible_flags	v4wbi_tlb_flags
#	define v4wbi_always_flags	v4wbi_tlb_flags
#	ifdef _TLB
#		define MULTI_TLB	1
#	else
#		define _TLB		v4wbi
#	endif
#else
#	define v4wbi_possible_flags	0
#	define v4wbi_always_flags	(-1UL)
#endif

#define fr_tlb_flags	(TLB_WB | TLB_DCLEAN | TLB_L2CLEAN_FR | \
			TLB_V4_I_FULL | TLB_V4_D_FULL | \
			TLB_V4_I_PAGE | TLB_V4_D_PAGE)

#ifdef CONFIG_CPU_TLB_FEROCEON
#	define fr_possible_flags	fr_tlb_flags
#	define fr_always_flags		fr_tlb_flags
#	ifdef _TLB
#		define MULTI_TLB	1
#	else
#		define _TLB		v4wbi
#	endif
#else
#	define fr_possible_flags	0
#	define fr_always_flags		(-1UL)
#endif

#define v4wb_tlb_flags	(TLB_WB | TLB_DCLEAN | \
			TLB_V4_I_FULL | TLB_V4_D_FULL | \
			TLB_V4_D_PAGE)

#ifdef CONFIG_CPU_TLB_V4WB
#	define v4wb_possible_flags	v4wb_tlb_flags
#	define v4wb_always_flags	v4wb_tlb_flags
#	ifdef _TLB
#		define MULTI_TLB	1
#	else
#		define _TLB		v4wb
#	endif
#else
#	define v4wb_possible_flags	0
#	define v4wb_always_flags	(-1UL)
#endif

#define fa_tlb_flags	(TLB_WB | TLB_DCLEAN | TLB_BARRIER | \
			TLB_V4_U_FULL | TLB_V4_U_PAGE)

#ifdef CONFIG_CPU_TLB_FA
#	define fa_possible_flags	fa_tlb_flags
#	define fa_always_flags		fa_tlb_flags
#	ifdef _TLB
#		define MULTI_TLB	1
#	else
#		define _TLB		fa
#	endif
#else
#	define fa_possible_flags	0
#	define fa_always_flags		(-1UL)
#endif

#define v6wbi_tlb_flags	(TLB_WB | TLB_DCLEAN | TLB_BARRIER | \
			TLB_V6_I_FULL | TLB_V6_D_FULL | \
			TLB_V6_I_PAGE | TLB_V6_D_PAGE | \
			TLB_V6_I_ASID | TLB_V6_D_ASID | \
			TLB_V6_BP)

#ifdef CONFIG_CPU_TLB_V6
#	define v6wbi_possible_flags	v6wbi_tlb_flags
#	define v6wbi_always_flags	v6wbi_tlb_flags
#	ifdef _TLB
#		define MULTI_TLB	1
#	else
#		define _TLB		v6wbi
#	endif
#else
#	define v6wbi_possible_flags	0
#	define v6wbi_always_flags	(-1UL)
#endif

#define v7wbi_tlb_flags_smp	(TLB_WB | TLB_BARRIER | \
				TLB_V7_UIS_FULL | TLB_V7_UIS_PAGE | \
				TLB_V7_UIS_ASID | TLB_V7_UIS_BP)
#define v7wbi_tlb_flags_up	(TLB_WB | TLB_DCLEAN | TLB_BARRIER | \
				TLB_V6_U_FULL | TLB_V6_U_PAGE | \
				TLB_V6_U_ASID | TLB_V6_BP)

#ifdef CONFIG_CPU_TLB_V7
#	ifdef CONFIG_SMP_ON_UP
#		define v7wbi_possible_flags	(v7wbi_tlb_flags_smp | \
						 v7wbi_tlb_flags_up)
#		define v7wbi_always_flags	(v7wbi_tlb_flags_smp & \
						 v7wbi_tlb_flags_up)
#	elif defined(CONFIG_SMP)
#		define v7wbi_possible_flags	v7wbi_tlb_flags_smp
#		define v7wbi_always_flags	v7wbi_tlb_flags_smp
#	else
#		define v7wbi_possible_flags	v7wbi_tlb_flags_up
#		define v7wbi_always_flags	v7wbi_tlb_flags_up
#	endif
#	ifdef _TLB
#		define MULTI_TLB	1
#	else
#		define _TLB		v7wbi
#	endif
#else
#	define v7wbi_possible_flags	0
#	define v7wbi_always_flags	(-1UL)
#endif

/*
 * We optimise the code below by:
 *  - building a set of TLB flags that might be set in __cpu_tlb_flags
 *  - building a set of TLB flags that will always be set in __cpu_tlb_flags
 *  - if we're going to need __cpu_tlb_flags, access it once and only once
 *
 * This allows us to build optimal assembly for the single-CPU type case,
 * and as close to optimal given the compiler constrants for multi-CPU
 * case.  We could do better for the multi-CPU case if the compiler
 * implemented the "%?" method, but this has been discontinued due to too
 * many people getting it wrong.
 */
#define possible_tlb_flags_bs	(v4_possible_flags | \
				v4wbi_possible_flags | \
				fr_possible_flags | \
				v4wb_possible_flags | \
				fa_possible_flags | \
				v6wbi_possible_flags | \
				v7wbi_possible_flags)

#define always_tlb_flags_bs	(v4_always_flags & \
				v4wbi_always_flags & \
				fr_always_flags & \
				v4wb_always_flags & \
				fa_always_flags & \
				v6wbi_always_flags & \
				v7wbi_always_flags)

#define __tlb_op(f, insnarg, arg)					\
	do {								\
		if (always_tlb_flags_bs & (f))				\
			asm("mcr " insnarg				\
			    : : "r" (arg) : "cc");			\
		else if (possible_tlb_flags_bs & (f))			\
			asm("tst %1, %2\n\t"				\
			    "mcrne " insnarg				\
			    : : "r" (arg), "r" (__tlb_flag), "Ir" (f)	\
			    : "cc");					\
	} while (0)

#define tlb_flag_bs(f)			((always_tlb_flags_bs & (f)) || \
					(__tlb_flag & \
					possible_tlb_flags_bs & (f)))
#define tlb_op_bs(f, regs, arg)		__tlb_op(f, "p15, 0, %0, " regs, arg)
#define tlb_l2_op_bs(f, regs, arg)	__tlb_op(f, "p15, 1, %0, " regs, arg)

#define dsb_bs(option)	__asm__ __volatile__ ("dsb " #option : : : "memory")
#define isb_bs(option) __asm__ __volatile__ ("isb " #option : : : "memory")

static inline void __local_flush_tlb_all_bs(void)
{
	const int zero = 0;
	const unsigned int __tlb_flag = __cpu_tlb_flags_bs;

	tlb_op_bs(TLB_V4_U_FULL | TLB_V6_U_FULL, "c8, c7, 0", zero);
	tlb_op_bs(TLB_V4_D_FULL | TLB_V6_D_FULL, "c8, c6, 0", zero);
	tlb_op_bs(TLB_V4_I_FULL | TLB_V6_I_FULL, "c8, c5, 0", zero);
}

static inline void __flush_tlb_all_bs(void)
{
	const int zero = 0;
	const unsigned int __tlb_flag = __cpu_tlb_flags_bs;

	if (tlb_flag_bs(TLB_WB))
		dsb_bs(ishst);

	__local_flush_tlb_all_bs();
	tlb_op_bs(TLB_V7_UIS_FULL, "c8, c3, 0", zero);

	if (tlb_flag_bs(TLB_BARRIER)) {
		dsb_bs(ish);
		isb_bs();
	}
}

/*
 * flush_pmd_entry
 *
 * Flush a PMD entry (word aligned, or double-word aligned) to
 * RAM if the TLB for the CPU we are running on requires this.
 * This is typically used when we are creating PMD entries.
 *
 * clean_pmd_entry
 *
 * Clean (but don't drain the write buffer) if the CPU requires
 * these operations.  This is typically used when we are removing
 * PMD entries.
 */
static inline void flush_pmd_entry_bs(pmd_t_bs *pmd)
{
	const unsigned int __tlb_flag = __cpu_tlb_flags_bs;

	tlb_op_bs(TLB_DCLEAN, "c7, c10, 1  @ flush_pmd", pmd);
	tlb_l2_op_bs(TLB_L2CLEAN_FR, "c15, c9, 1  @ L2 flush_pmd", pmd);

	if (tlb_flag_bs(TLB_WB))
		dsb_bs(ishst);
}

#undef tlb_op_bs
#undef tlb_flag_bs
#undef always_tlb_flags_bs
#undef possible_tlb_flags_bs

#ifdef CONFIG_SMP
extern void flush_tlb_all_bs(void);
#endif

#define flush_tlb_kernel_range_bs(x, y)	do { } while (0)

#endif
