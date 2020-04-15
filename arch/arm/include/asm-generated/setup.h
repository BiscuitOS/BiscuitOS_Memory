#ifndef _BISCUITOS_ARM_SETUP_H
#define _BISCUITOS_ARM_SETUP_H

#define COMMAND_LINE_SIZE	1024

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
struct early_params {
	const char *arg;
	void (*fn)(char **p);
};

#define __early_param_bs(name,fn)					\
static struct early_params __early_##fn __attribute__((__unused__))	\
__attribute__((__section__("__early_param_bs"))) = { name, fn }

#endif
