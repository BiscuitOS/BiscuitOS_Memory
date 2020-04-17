#ifndef _BISCUITOS_ARM_PROCINFO_H
#define _BISCUITOS_ARM_PROCINFO_H

/*
 * Note!  struct processor is always defined if we're
 * using MULTI_CPU, otherwise this entry is unused,
 * but still exists.
 *
 * NOTE! The following structure is defined by assembly
 * language, NOT C code.  For more information, check:
 *  arch/arm/mm/proc-*.S and arch/arm/kernel/head.S
 */     
struct proc_info_list {
	unsigned int		cpu_val;
	unsigned int		cpu_mask;
	unsigned long		__cpu_mm_mmu_flags;     /* used by head.S */
	unsigned long		__cpu_io_mmu_flags;     /* used by head.S */
	unsigned long		__cpu_flush;            /* used by head.S */
	const char		*arch_name;
	const char		*elf_name;
	unsigned int		elf_hwcap;
	const char		*cpu_name;
};

#endif
