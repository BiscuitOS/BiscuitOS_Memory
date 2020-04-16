#ifndef _BISCUITOS_ARM_TLBFLUSH_H
#define _BISCUITOS_ARM_TLBFLUSH_H

#define TLB_V3_PAGE	(1 << 0)
#define TLB_V4_U_PAGE	(1 << 1)
#define TLB_V4_D_PAGE	(1 << 2)
#define TLB_V4_I_PAGE	(1 << 3)
#define TLB_V6_U_PAGE	(1 << 4)
#define TLB_V6_D_PAGE	(1 << 5)
#define TLB_V6_I_PAGE	(1 << 6)

#define TLB_V3_FULL	(1 << 8)
#define TLB_V4_U_FULL	(1 << 9)
#define TLB_V4_D_FULL	(1 << 10)
#define TLB_V4_I_FULL	(1 << 11)
#define TLB_V6_U_FULL	(1 << 12)
#define TLB_V6_D_FULL	(1 << 13)
#define TLB_V6_I_FULL	(1 << 14)

#define TLB_V6_U_ASID	(1 << 16)
#define TLB_V6_D_ASID	(1 << 17)
#define TLB_V6_I_ASID	(1 << 18)

#define TLB_DCLEAN	(1 << 30)
#define TLB_WB		(1 << 31)

struct cpu_tlb_fns {
	unsigned long tlb_flags;
};

extern struct cpu_tlb_fns cpu_tlb;

#define __cpu_tlb_flags_bs	cpu_tlb.tlb_flags

#define v3_possible_flags	0
#define v3_always_flags		(-1UL)
#define v4_possible_flags	0
#define v4_always_flags		(-1UL)
#define v4wbi_possible_flags	0
#define v4wbi_always_flags	(-1UL)
#define v4wb_possible_flags	0
#define v4wb_always_flags	(-1UL)
#define v6wbi_possible_flags	0
#define v6wbi_always_flags	(-1UL)

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
#define possible_tlb_flags	(v3_possible_flags | \
				v4_possible_flags | \
				v4wbi_possible_flags | \
				v4wb_possible_flags | \
				v6wbi_possible_flags)

#define always_tlb_flags	(v3_always_flags & \
				v4_always_flags & \
				v4wbi_always_flags & \
				v4wb_always_flags & \
				v6wbi_always_flags)

#define tlb_flag_bs(f)		((always_tlb_flags & (f)) || \
				(__tlb_flag & possible_tlb_flags & (f)))

/*
 * flush_pmd_entry_bs
 *
 * Flush a PMD entry (word aligned, or double-word aligned) to
 * RAM if the TLB for the CPU we are running on requires this.
 * This is typically used when we are creating PMD entries.
 *
 * clean_pmd_entry
 *
 * Clean (but don[t drain the write buffer) if the CPU requires
 * these operations. This is typically used when are removing
 * PMD entries.
 */
static inline void flush_pmd_entry_bs(pmd_t_bs *pmd)
{
	const unsigned int zero = 0;
	const unsigned int __tlb_flag = __cpu_tlb_flags_bs;

	if (tlb_flag_bs(TLB_DCLEAN))
		asm("mcr%?	p15, 0, %0, c7, c10, 1 @ flush_pmd"
			: : "r" (pmd));
	if (tlb_flag_bs(TLB_WB))
		asm("mcr%?	p15, 0, %0, c7, c10, 4 @ flush_pmd"
			: : "r" (zero));
}

static inline void clean_pmd_entry_bs(pmd_t_bs *pmd)
{
	const unsigned int __tlb_flag = __cpu_tlb_flags_bs;

	if (tlb_flag_bs(TLB_DCLEAN))
		asm("mcr%?	p15, 0, %0, c7, c10, 1 @ flush_pmd"
			: : "r" (pmd));
}

#endif
