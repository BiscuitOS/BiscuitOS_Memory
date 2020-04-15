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

extern void paging_init_bs(struct meminfo *, struct machine_desc *desc);
static void __init early_end_bs(char **p);
static void __init early_begin_bs(char **p);

static struct meminfo meminfo_bs = { 0, };
static char command_line_bs[COMMAND_LINE_SIZE];
static char default_command_line_bs[COMMAND_LINE_SIZE] __initdata = CONFIG_CMDLINE_BS;


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

	start = PHYS_OFFSET;
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

void __init setup_arch_bs(char **cmdline_p)
{
	struct machine_desc *mdesc = NULL;
	char *from = default_command_line_bs;

	memcpy(saved_command_line_bs, from, COMMAND_LINE_SIZE);
	saved_command_line_bs[COMMAND_LINE_SIZE - 1] = '\0';
	parse_cmdline_bs(cmdline_p, from);

	paging_init_bs(&meminfo_bs, mdesc);
}

