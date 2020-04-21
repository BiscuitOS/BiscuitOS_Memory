/*
 *  linux/include/asm/setup.h
 *
 *  Copyright (C) 1997-1999 Russell King
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 *  Structure passed to kernel to tell it about the
 *  hardware it's running on.  See Documentation/arm/Setup
 *  for more info.
 */
#ifndef _BISCUITOS_ARM_SETUP_H
#define _BISCUITOS_ARM_SETUP_H

#define COMMAND_LINE_SIZE_BS	1024

#define NR_BANKS		8

struct meminfo {
	int nr_banks;
	struct {
		unsigned long start;
		unsigned long size;
		int node;
	} bank[NR_BANKS];
};

/*
 * Early command line parameters
 */
struct early_params_bs {
	const char *arg;
	void (*fn)(char **p);
};

#ifndef __attribute_used__
#define __attribute_used__	__attribute__((__used__))
#endif

#define __early_param_bs(name,fn)					\
static struct early_params_bs __early_##fn __attribute_used__		\
__attribute__((__section__("__early_param_bs"))) = { name, fn }

#endif
