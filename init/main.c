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
#include "biscuitos/cache.h"
#include "biscuitos/percpu.h"
#include "biscuitos/mm.h"
#include "biscuitos/bootmem.h"
#include "asm-generated/memory.h"

/* setup */
extern void setup_arch_bs(char **);

unsigned long __per_cpu_offset_bs[NR_CPUS_BS];
EXPORT_SYMBOL_GPL(__per_cpu_offset_bs);

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
}

asmlinkage void __init start_kernel_bs(void)
{
	const char *cmdline;

	page_address_init_bs();
	printk(KERN_NOTICE);
	setup_arch_bs((char **)&cmdline);
	setup_per_cpu_areas_bs();
}
