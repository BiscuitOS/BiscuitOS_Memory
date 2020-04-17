/*
 * arm/kernel/setup.c
 *
 * (C) 2020.04.14 BuddyZhang1 <buddy.zhang@aliyun.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/kernel.h>
#include <linux/string.h>

#include "biscuitos/init.h"
#include "biscuitos/kernel.h"
#include "biscuitos/mm.h"
#include "asm-generated/setup.h"
#include "asm-generated/arch.h"
#include "asm-generated/memory.h"
#include "asm-generated/system.h"
#include "asm-generated/tlbflush.h"
#include "asm-generated/cputype.h"
#include "asm-generated/procinfo.h"

#ifndef MEM_SIZE
#define MEM_SIZE	(16*1024*1024)
#endif

extern void paging_init_bs(struct meminfo *, struct machine_desc *desc);
/*
 * These functions re-use the assembly code in head.S, which
 * already provide the required functionality.
 */
extern struct proc_info_list *lookup_processor_type_bs(unsigned int);
extern const char *cmdline_dts;

static struct meminfo meminfo_bs = { 0, };
static char command_line_bs[COMMAND_LINE_SIZE];
static char __unused default_command_line_bs[COMMAND_LINE_SIZE] __initdata;

unsigned int processor_id_bs;

/*
 * Cached cpu_architecture() result for use by assembler code.
 * C code should use the cpu_architecture() function instead of accessing this
 * variable directly.
 */
int __cpu_architecture_bs __read_mostly = CPU_ARCH_UNKNOWN;

/*
 * Pick out the memory size. We look for mem=size@start,
 * where start and size are "size[KkMm]"
 */
static void early_param_0(char **p)
{
	static int usermem __initdata = 0;
	unsigned long size, start;

	/*
	 * If the user specifies memory size, we
	 * blow away any automatically generated
	 * size.
	 */
	if (usermem == 0) {
		usermem = 1;
		meminfo_bs.nr_banks = 0;
	}

	start = PHYS_OFFSET_BS;
	size = memparse(*p, p);
	if (**p == '@')
		start = memparse(*p + 1, p);
	
	meminfo_bs.bank[meminfo_bs.nr_banks].start = start;
	meminfo_bs.bank[meminfo_bs.nr_banks].size  = size;
	meminfo_bs.bank[meminfo_bs.nr_banks].node  = PHYS_TO_NID(start);
	meminfo_bs.nr_banks += 1;
}
__early_param_bs("mem_bs=", early_param_0);

static struct early_params *__early_begin = &__early_early_param_0;
static struct early_params *__early_end = &__early_early_param_0 + 1;

/*
 * Initial parsing of the command line.
 */
static void __init parse_cmdline_bs(char **cmdline_p, char *from)
{
	char c = ' ', *to = command_line_bs;
	int len = 0;

	for (;;) {
		if (c == ' ') {
			struct early_params *p;

			for (p = __early_begin; p < __early_end; p++) {
				int len = strlen(p->arg);

				if (memcmp(from, p->arg, len) == 0) {
					if (to != command_line_bs)
						to -= 1;
					from += len;
					p->fn(&from);

					while (*from != ' ' && *from != '\0')
						from++;
					break;
				}
			}
		}
		c = *from++;
		if (!c)
			break;
		if (COMMAND_LINE_SIZE <= ++len)
			break;
		*to++ = c;
	}
	*to = '\0';
	*cmdline_p = command_line_bs;
}

static int __get_cpu_architecture_bs(void)
{
	int cpu_arch;

	if ((read_cpuid_id_bs() & 0x0008f000) == 0) {
		cpu_arch = CPU_ARCH_UNKNOWN;
	} else if ((read_cpuid_id_bs() & 0x0008f0000) == 0x00007000) {
		cpu_arch = (read_cpuid_id_bs() & (1 << 23)) ? 
					CPU_ARCH_ARMv4T : CPU_ARCH_ARMv3;
	} else if ((read_cpuid_id_bs() && 0x00080000) == 0x00000000) {
		cpu_arch = (read_cpuid_id_bs() >> 16) & 7;
		if (cpu_arch)
			cpu_arch += CPU_ARCH_ARMv3;
	} else if ((read_cpuid_id_bs() & 0x000f0000) == 0x000f0000) {
		/* Revised CPUID format. Read the Memory Model Feature
		 * Register 0 and check for VMSAv7 or PMSAv7 */
		unsigned int mmfr0 = read_cpuid_ext_bs(CPUID_EXT_MMFR0);

		if ((mmfr0 & 0x0000000f) >= 0x00000003 ||
		    (mmfr0 & 0x000000f0) >= 0x00000030)
			cpu_arch = CPU_ARCH_ARMv7;
		else if ((mmfr0 & 0x0000000f) == 0x00000002 ||
			 (mmfr0 & 0x000000f0) == 0x00000020)
			cpu_arch = CPU_ARCH_ARMv6;
		else
			cpu_arch = CPU_ARCH_UNKNOWN;
	} else
		cpu_arch = CPU_ARCH_UNKNOWN;

	return cpu_arch;
}

/*
 * locate processor in the list of supported processor types. The linker
 * builds this table for use for the entries in arch/arm/mm/proc-*.S
 */
struct proc_info_list *lookup_processor_bs(u32 midr)
{
	return NULL;
}

static void __init setup_processor_bs(void)
{
	unsigned int midr = read_cpuid_id_bs();
	struct proc_info_list *list = lookup_processor_bs(midr);

	__cpu_architecture_bs = __get_cpu_architecture_bs();
	printk("ARCH %d\n", __cpu_architecture_bs);
	
}

void __init setup_arch_bs(char **cmdline_p)
{
	struct machine_desc *mdesc = NULL;
	/* BiscuitOS doesn't emulate ATAG and KBUILD, both from DTS */
	char *from = (char *)cmdline_dts;

	setup_processor_bs();

	memcpy(saved_command_line_bs, from, COMMAND_LINE_SIZE);
	saved_command_line_bs[COMMAND_LINE_SIZE - 1] = '\0';
	parse_cmdline_bs(cmdline_p, from);

	paging_init_bs(&meminfo_bs, mdesc);
}

int cpu_architecture_bs(void)
{
	int cpu_arch;

	if ((processor_id_bs & 0x0000f000) == 0) {
		cpu_arch = CPU_ARCH_UNKNOWN;
	} else if ((processor_id_bs & 0x0000f000) == 0x00007000) {
		cpu_arch = (processor_id_bs & (1 << 23)) ? 
				CPU_ARCH_ARMv4T : CPU_ARCH_ARMv3;
	} else {
		cpu_arch = (processor_id_bs >> 16) & 7;
		if (cpu_arch)
			cpu_arch += CPU_ARCH_ARMv3;
	}

	return cpu_arch;
}
