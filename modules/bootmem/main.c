/*
 * BiscuitOS Memory Manager: Bootmem
 *
 * (C) 2019.10.01 BuddyZhang1 <buddy.zhang@aliyun.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/string.h>
#include "biscuitos/kernel.h"
#include "biscuitos/init.h"
#include "biscuitos/bootmem.h"
#include "biscuitos/mm.h"

/*
 * TestCase: alloc/free on bootmem
 */
static int TestCase_bootmem_alloc(void)
{
	unsigned long *ptr;

	/* alloc from bootmem */
	ptr = (unsigned long *)alloc_bootmem_bs(sizeof(unsigned long));
	if (!ptr) {
		printk("%s alloc_bootmem() failed.\n", __func__);
		return -ENOMEM;
	}
	*ptr = 0x9091;
	bs_debug("INDEX %#lx\n", *ptr);

	/* free */
	free_bootmem_bs(__pa_bs(ptr), sizeof(unsigned long));

	return 0;
}
bootmem_initcall_bs(TestCase_bootmem_alloc);

/*
 * TestCase: alloc/free pages on bootmem
 */
static int TestCase_bootmem_alloc_pages(void)
{
	unsigned long pages;

	/* alloc page */
	pages = (unsigned long)alloc_bootmem_pages_bs(PAGE_SIZE);
	if (pages < 0) {
		printk("%s alloc_bootmem_pages() failed\n", __func__);
		return -ENOMEM;
	}
	memset((void *)pages, 0, PAGE_SIZE);
	sprintf((char *)pages, "BiscuitOS-%s", __func__);
	bs_debug("%s\n", (char *)pages);

	/* free */
	free_bootmem_bs(__pa_bs(pages), PAGE_SIZE);

	return 0;
}
bootmem_initcall_bs(TestCase_bootmem_alloc_pages);

/*
 * TestCase: alloc/free low memory
 */
static int TestCase_boot_alloc_low(void)
{
	unsigned long *ptr;

	/* alloc from bootmem */
	ptr = (unsigned long *)alloc_bootmem_low_bs(sizeof(unsigned long));
	if (!ptr) {
		printk("%s alloc_bootmem_low() failed\n", __func__);
		return -ENOMEM;
	}
	*ptr = 0x89;
	bs_debug("INDEX: %#lx\n", *ptr);

	/* free */
	free_bootmem_bs(__pa_bs(ptr), sizeof(unsigned long));
	return 0;
}
bootmem_initcall_bs(TestCase_boot_alloc_low);

/*
 * TestCase: alloc/free low pages
 */
static int TestCase_bootmem_alloc_low_pages(void)
{
	unsigned long pages;

	/* alloc */
	pages = (unsigned long)alloc_bootmem_low_pages_bs(PAGE_SIZE);
	if (pages < 0) {
		printk("%s alloc_bootmem_low_pages() failed\n", __func__);
		return -ENOMEM;
	}
	sprintf((char *)pages, "BiscuitOS-%s", __func__);
	bs_debug("%s\n", (char *)pages);

	/* free */
	free_bootmem_bs(__pa_bs(pages), PAGE_SIZE);
	return 0;
}
bootmem_initcall_bs(TestCase_bootmem_alloc_low_pages);

/*
 * TestCase: Reserved special memory region.
 */
static int TestCase_bootmem_reserve(void)
{
#define MEM_START 0x74000000
	/* Reserved memory region */
	reserve_bootmem_bs(MEM_START, 0x10);

	/* Cansole reserve */
	free_bootmem_bs(MEM_START, 0x10);
#undef MEM_START
	return 0;
}
bootmem_initcall_bs(TestCase_bootmem_reserve);
