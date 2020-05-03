/*
 * BiscuitOS Memory Manager (MMU)
 *
 * (C) 2020.04.14 BuddyZhang1 <buddy.zhang@aliyun.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of_platform.h>
#include <linux/mm_types.h>
#include <asm/tlbflush.h>

#include "asm-generated/setup.h"
#include "asm-generated/pgtable.h"

#define DEV_NAME "BiscuitOS"

/* BiscuitOS Physical and Virtual space Layout information */
phys_addr_t BiscuitOS_ram_base;
static phys_addr_t BiscuitOS_ram_size;
phys_addr_t swapper_pg_dir_bs;
u32 BiscuitOS_PAGE_OFFSET;
u32 BiscuitOS_dma_size;
u32 BiscuitOS_vmalloc_size;
static u32 BiscuitOS_high_size;
static u32 BiscuitOS_normal_size;
static u32 BiscuitOS_pkmap_size;
static u32 BiscuitOS_pkmap_last;
static u32 BiscuitOS_fixmap_size;
u32 BiscuitOS_fixmap_top;

EXPORT_SYMBOL_GPL(BiscuitOS_ram_base);

/* BiscuitOS debug stuf */
int BiscuitOS_debug = 0;
EXPORT_SYMBOL_GPL(BiscuitOS_debug);

/* Command line from DTS */
char cmdline_dts[COMMAND_LINE_SIZE_BS];
/* FIXME: TLB information: From ARMv7 Hard-Code */
unsigned long BiscuitOS_tlb_flags = 0xd0091010;

extern unsigned long phys_initrd_start_bs;
extern unsigned long phys_initrd_size_bs;
extern asmlinkage void __init start_kernel_bs(void);

/* BiscuitOS Running Memory */
unsigned long _stext_bs, _end_bs;
unsigned long _etext_bs, _text_bs;
unsigned long __init_end_bs, __init_begin_bs;
unsigned long __data_start_bs, _edata_bs;

static int BiscuitOS_memory_probe(struct platform_device *pdev)
{
	struct device_node *np = pdev->dev.of_node;
	struct device_node *mem;
	char tmp_cmdline[128];
	const phandle *ph;
	const char *string;
	u32 array[8];
	int ret;

	/* Find memory node by phandle */
	ph = of_get_property(np, "ram", NULL);
	if (!ph) {
		printk("Unable to find 'phy-handle' on current device\n");
		return -EINVAL;
	}

	mem = of_find_node_by_phandle(be32_to_cpup(ph));
	if (!mem) {
		printk("Unable to find device node: BiscuitOS_memory\n");
		return -EINVAL;
	}

	/* Obtain memory information */
	ret = of_property_read_u32_array(mem, "reg", array, 2);
	if (ret) {
		printk("Unable to read BiscuitOS ram information\n");
		return -EINVAL;
	}
	BiscuitOS_ram_base = array[0];
	BiscuitOS_ram_size = array[1];

	/* Obtain PAGE_OFFSET_BS information */
	ret = of_property_read_u32(mem, "page-offset", &BiscuitOS_PAGE_OFFSET);
	if (ret) {
		printk("Unable to read BiscuitOS PAGE_OFFSET_BS\n");
		return -EINVAL;
	}

	/* Obtain dma-zone information */
	ret = of_property_read_u32(mem, "dma-zone", &BiscuitOS_dma_size);
	if (ret) {
		printk("Unable to read BiscuitOS dma-zone\n");
		return -EINVAL;
	}

	/* Obtain normal-zone information */
	ret = of_property_read_u32(mem, "normal-zone", &BiscuitOS_normal_size);
	if (ret) {
		printk("Unable to read BiscuitOS normal-zone\n");
		return -EINVAL;
	}

	/* Obtain high-zone information */
	ret = of_property_read_u32(mem, "high-zone", &BiscuitOS_high_size);
	if (ret) {
		printk("Unable to read BiscuitOS high-zone\n");
		return -EINVAL;
	}

	/* Obtain vmalloc-size information */
	ret = of_property_read_u32(mem, "vmalloc-size", 
						&BiscuitOS_vmalloc_size);
	if (ret) {
		printk("Unable to read BiscuitOS vmalloc-size\n");
		return -EINVAL;
	}

	/* Obtain pkmap-size information */
	ret = of_property_read_u32(mem, "pkmap-size", &BiscuitOS_pkmap_size);
	if (ret) {
		printk("Unable to read BiscuitOS pkmap-size\n");
		return -EINVAL;
	}
	/* last pkmap */
	BiscuitOS_pkmap_last = BiscuitOS_pkmap_size >> PAGE_SHIFT_BS;

	if ((BiscuitOS_normal_size + BiscuitOS_high_size + 
				BiscuitOS_dma_size) > BiscuitOS_ram_size) {
		panic("Incorrect BiscuitOS ARM on DTS\n");
	}

	/* Obtain fixmap-size information */
	ret = of_property_read_u32(mem, "fixmap-size", &BiscuitOS_fixmap_size);
	if (ret) {
		printk("Unable to read BiscuitOS fixmap-size\n");
		return -EINVAL;
	}

	/* Calculate Fixmap region */
	BiscuitOS_fixmap_top = BiscuitOS_PAGE_OFFSET + BiscuitOS_ram_size
					- BiscuitOS_fixmap_size;

	/* Obtain cmdline information */
	ret = of_property_read_string(np, "cmdline", &string);	
	if (ret) {
		printk("Unable to read cmdline information.\n");
		return -EINVAL;
	}

	/* Calculate memory map */
	sprintf((char *)tmp_cmdline, 
#ifndef CONFIG_HIGHMEM_BS
		"mem_bs=%#lx@%#lx", 
		(unsigned long)BiscuitOS_dma_size + BiscuitOS_normal_size,
		(unsigned long)BiscuitOS_ram_base);
#else
		"mem_bs=%#lx@%#lx highmem_bs=%#lx@%#lx", 
		(unsigned long)BiscuitOS_dma_size + BiscuitOS_normal_size,
		(unsigned long)BiscuitOS_ram_base,
		(unsigned long)BiscuitOS_high_size,
		(unsigned long)(BiscuitOS_ram_base +
				BiscuitOS_dma_size + BiscuitOS_normal_size));
#endif
	if (!string) {
		strcpy(cmdline_dts, tmp_cmdline);
	} else {
		strcpy(cmdline_dts, string);
		strcat((char *)cmdline_dts, " ");
		strcat((char *)cmdline_dts, tmp_cmdline);
	}

	/* Obtain kernel image information */
	ret = of_property_read_u32_array(mem, "kernel-image", array, 8);
	if (ret) {
		printk("Unable to read BiscuitOS kernel image.\n");
		return -EINVAL;
	}
	/* Running setup */
	_stext_bs = array[0];
	_end_bs = array[0] + array[1];
	__init_begin_bs = array[2];
	__init_end_bs = array[2] + array[3];
	_text_bs = array[4];
	_etext_bs = array[4] + array[5];
	__data_start_bs = array[6];
	_edata_bs = array[6] + array[7];
	swapper_pg_dir_bs = array[0] - 0x4000;

	/* Obtain initrd information */
	ret = of_property_read_u32_array(mem, "initrd", array, 2);
	if (ret) {
		printk("Unable to read BiscuitOS initrd.\n");
		return -EINVAL;
	}
	phys_initrd_start_bs = array[0];
	phys_initrd_size_bs = array[1];

	start_kernel_bs();

	printk("Hello BiscuitOS: Linux 2.6.12.3 MMU\n");

	return 0;
}

static int BiscuitOS_memory_remove(struct platform_device *pdev)
{
	return 0;
}

static const struct of_device_id BiscuitOS_memory_of_match[] = {
	{ .compatible = "BiscuitOS,Memory-Manager", },
	{ },
};
MODULE_DEVICE_TABLE(of, BiscuitOS_memory_of_match);

/* Platform Driver Information */
static struct platform_driver BiscuitOS_memory_driver = {
	.probe    = BiscuitOS_memory_probe,
	.remove   = BiscuitOS_memory_remove,
	.driver	= {
		.owner	= THIS_MODULE,
		.name	= DEV_NAME,
		.of_match_table	= BiscuitOS_memory_of_match,
	},
};
module_platform_driver(BiscuitOS_memory_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("BiscuitOS <buddy.zhang@aliyun.com>");
MODULE_DESCRIPTION("BiscuitOS Memory Manager Project");
