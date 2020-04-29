/*
 *  linux/init/main.c
 *
 *  Copyright (C) 1991, 1992  Linus Torvalds
 *
 *  GK 2/5/95  -  Changed to support mounting root fs via NFS
 *  Added initrd & change_root: Werner Almesberger & Hans Lermen, Feb '96
 *  Moan early if gcc is old, avoiding bogus kernels - Paul Gortmaker, May '96
 *  Simplified starting of init:  Michael A. Griffith <grif@acm.org> 
 */
#include <linux/kernel.h>
#include <linux/string.h>
#include "biscuitos/kernel.h"
#include "biscuitos/cache.h"
#include "biscuitos/percpu.h"
#include "biscuitos/mm.h"
#include "biscuitos/bootmem.h"
#include "biscuitos/slab.h"
#include "biscuitos/init.h"
#include "biscuitos/fs.h"
#include "biscuitos/moduleparam.h"
#include "asm-generated/memory.h"
#include "asm-generated/setup.h"

/*
 * Boot command-line arguments
 */
#define MAX_INIT_ARGS_BS	CONFIG_INIT_ENV_ARG_LIMIT
#define MAX_INIT_ENVS_BS	CONFIG_INIT_ENV_ARG_LIMIT

/* setup */
extern void setup_arch_bs(char **);

static char * argv_init_bs[MAX_INIT_ARGS_BS+2] = { "init", NULL, };
char * envp_init_bs[MAX_INIT_ENVS_BS+2] = { "HOME=/", "TERM=linux", NULL, };
static const char *panic_later_bs, *panic_param_bs;

unsigned long __per_cpu_offset_bs[NR_CPUS_BS];
EXPORT_SYMBOL_GPL(__per_cpu_offset_bs);
extern struct obs_kernel_param_bs __setup_start_bs[], __setup_end_bs[];

/* Untouched command line (eg. for /proc) saved by arch-specific code. */
char saved_command_line_bs[COMMAND_LINE_SIZE_BS];

DEBUG_FUNC_T(percpu);

static void __init setup_per_cpu_areas_bs(void)
{
	unsigned long size, i;
	char *ptr;
	/* Created by linker magic */
	extern char __per_cpu_start_bs[], __per_cpu_end_bs[];

	/* Copy section for each CPU (we discard the original) */
	size = ALIGN(__per_cpu_end_bs - __per_cpu_start_bs, 
							SMP_CACHE_BYTES_BS);
#ifdef CONFIG_MODULES
	if (size < PERCPU_ENOUGH_ROOM_BS)
		size = PERCPU_ENOUGH_ROOM_BS;
#endif
	ptr = alloc_bootmem_bs(size * NR_CPUS_BS);

	for (i = 0; i < NR_CPUS_BS; i++, ptr += size) {
		__per_cpu_offset_bs[i] = ptr - __per_cpu_start_bs;
		memcpy(ptr, __per_cpu_start_bs, 
				__per_cpu_end_bs - __per_cpu_start_bs);
	}

	/* FIXME: Debug percpu stuf */
	DEBUG_CALL(percpu);
}

/* Check for early params. */
static int __init do_early_param_bs(char *param, char *val)
{
	struct obs_kernel_param_bs *p;

	for (p = __setup_start_bs; p < __setup_end_bs; p++) {
		if (p->early && strcmp(param, p->str) == 0) {
			if (p->setup_func(val) != 0)
				printk(KERN_WARNING "Malformed early option "
						"'%s'\n", param);
		}
	}
	/* We accept earything at this stage. */
	return 0;
}

/* Arch code calls this early on, or if not, just before other parsing. */
void __init parse_early_param_bs(void)
{
	static __initdata int done_bs = 0;
	static __initdata char tmp_cmdline_bs[COMMAND_LINE_SIZE_BS];

	if (done_bs)
		return;

	/* All fall through to do_early_param */
	strlcpy(tmp_cmdline_bs, saved_command_line_bs, COMMAND_LINE_SIZE_BS);
	parse_args_bs("early options", tmp_cmdline_bs, NULL, 0, 
							do_early_param_bs);
	done_bs = 1;
}

static int __init obsolete_checksetup_bs(char *line)
{
	struct obs_kernel_param_bs *p;

	p = __setup_start_bs;
	do {
		int n = strlen(p->str);

		if (!strncmp(line, p->str, n)) {
			if (p->early) {
				/* Already done in parse_early_param?  (Needs
				 * exact match on param part) */
				if (line[n] == '\0' || line[n] == '=')
					return 1;
			} else if (!p->setup_func) {
				printk(KERN_WARNING "Parameter %s is obsolete,"
							" ignored\n", p->str);
				return 1;
			} else if (p->setup_func(line + n))
				return 1;
		}
		p++;
	} while (p < __setup_end_bs);
	return 0;
}

/*
 * Unknown boot options get handed to init, unless they look like
 * failed parameters
 */
static int __init unknown_bootoption_bs(char *param, char *val)
{
	/* Change NUL term back to "=", to make "param" the whole string. */
	if (val) {
		/* param=val or param="val"? */
		if (val == param+strlen(param)+1)
			val[-1] = '=';
		else if (val == param+strlen(param)+2) {
			val[-2] = '=';
			memmove(val-1, val, strlen(val)+1);
			val--;
		} else
			BUG_BS();
	}

	/* Handle obsolete-style parameters */
	if (obsolete_checksetup_bs(param))
		return 0;

	/*
	 * Preemptive maintenance for "why didn't my mispelled command
	 * line work?"
	 */
	if (strchr(param, '.') && (!val || strchr(param, '.') < val)) {
		printk(KERN_ERR "Unknown boot option `%s': ignoring\n", param);
		return 0;
	}

	if (panic_later_bs)
		return 0;

	if (val) {
		/* Environment option */
		unsigned int i;

		for (i = 0; envp_init_bs[i]; i++) {
			if (i == MAX_INIT_ENVS_BS) {
				panic_later_bs = "Too many boot env vars "
						 "at `%s'";
				panic_param_bs = param;
			}
			if (!strncmp(param, envp_init_bs[i], val - param))
				break;
		}
		envp_init_bs[i] = param;
	} else {
		/* Command line option */
		unsigned int i;

		for (i = 0; argv_init_bs[i]; i++) {
			if (i == MAX_INIT_ARGS_BS) {
				panic_later_bs = "Too many boot init vars "
						 "at `%s'";
				panic_param_bs = param;
			}
		}
		argv_init_bs[i] = param;
	}
	return 0;
}

DEBUG_FUNC_T(vmalloc);
DEBUG_FUNC_T(module);
DEBUG_FUNC_T(kmap);
DEBUG_FUNC_T(fixmap);
DEBUG_FUNC_T(login);

asmlinkage void __init start_kernel_bs(void)
{
	char *command_line;
	extern struct kernel_param_bs __start___param_bs[];
	extern struct kernel_param_bs __stop___param_bs[];

	page_address_init_bs();
	printk(KERN_NOTICE);
	setup_arch_bs(&command_line);
	setup_per_cpu_areas_bs();

	build_all_zonelists_bs();
	page_alloc_init_bs();
	printk(KERN_NOTICE "Kernel command line: %s\n", saved_command_line_bs);
	parse_early_param_bs();
	parse_args_bs("Booting kernel", command_line, __start___param_bs,
		((unsigned long)__stop___param_bs - 
		(unsigned long)__start___param_bs) / 
		sizeof(struct kernel_param_bs),
		&unknown_bootoption_bs);

	vfs_caches_init_early_bs();
	mem_init_bs();
	kmem_cache_init_bs();
	DEBUG_CALL(module);
	DEBUG_CALL(vmalloc);
	DEBUG_CALL(kmap);
	DEBUG_CALL(fixmap);
	DEBUG_CALL(login);
}
