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

/* setup */
extern void setup_arch_bs(char **);

static void __init setup_per_cpu_areas_bs(void)
{
	unsigned long size, i;
	char *ptr;
	/* Created by linker magic */
	
}

asmlinkage void __init start_kernel_bs(void)
{
	const char *cmdline;

	printk(KERN_NOTICE);
	setup_arch_bs((char **)&cmdline);
	setup_per_cpu_areas_bs();
}
