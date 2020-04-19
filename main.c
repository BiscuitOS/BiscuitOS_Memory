/*
 * BiscuitOS (mm)
 *
 * (C) 2020.04.14 BuddyZhang1 <buddy.zhang@aliyun.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

/*
 * BiscuitOS RAM on DTS:
 *
 *       reserved-memory {
 *               #address-cells = <1>;
 *               #size-cells = <1>;
 *               ranges;
 *
 *               BiscuitOS_memory: memory@70000000 {
 *                       reg = <0x70000000 0x4000000>;
 *               };
 *
 *               vram: vram@4c000000 {
 *                       compatible = "shared-dma-pool";
 *                       reg = <0x4c000000 0x00800000>;
 *                       no-map;
 *               };
 *       };
 *
 *       BiscuitOS {
 *               compatible = "BiscuitOS,mm";
 *               status = "okay";
 *               ram = <&BiscuitOS_memory>;
 *       };
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

/* RAM base address */
phys_addr_t BiscuitOS_ram_base;
phys_addr_t BiscuitOS_ram_size;
phys_addr_t swapper_pg_dir_bs;
u32 BiscuitOS_PAGE_OFFSET;
u32 BiscuitOS_high_size;
u32 BiscuitOS_normal_size;
u32 BiscuitOS_dma_size;
u32 BiscuitOS_vmalloc_size;
u32 BiscuitOS_pkmap_size;
u32 BiscuitOS_fixmap_size;

/* Untouched command line (eg. for /proc) saved by arch-specific code. */
char saved_command_line_bs[COMMAND_LINE_SIZE];
/* Emulate Kernel image */
unsigned long _stext_bs, _end_bs;
/* Command line from DTS */
const char cmdline_dts[COMMAND_LINE_SIZE];
/* init mem */
extern struct mm_struct init_mm;
struct mm_struct_bs init_mm_bs;
/* FIXME: TLB information: From ARMv7 Hard-Code */
unsigned long BiscuitOS_tlb_flags = 0xd0091010;

/* setup */
extern void setup_arch_bs(char **);
extern unsigned long phys_initrd_start_bs;
extern unsigned long phys_initrd_size_bs;
extern void *high_memory_bs;

/* FIXME: BiscuitOS architecture */
static void start_kernel(void)
{
	const char *cmdline;

	setup_arch_bs((char **)&cmdline);
}

static int BiscuitOS_memory_probe(struct platform_device *pdev)
{
	struct device_node *np = pdev->dev.of_node;
	struct device_node *mem;
	const phandle *ph;
	u32 array[2];
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

	/* Calculate memory map */
	sprintf((char *)cmdline_dts, 
		"mem_bs=%#lx@%#lx", 
		(unsigned long)BiscuitOS_dma_size + BiscuitOS_normal_size,
		(unsigned long)BiscuitOS_ram_base);
	printk("CMDLINE: [%s]\n", cmdline_dts);

	/* Obtain kernel image information */
	ret = of_property_read_u32_array(mem, "kernel-image", array, 2);
	if (ret) {
		printk("Unable to read BiscuitOS kernel image.\n");
		return -EINVAL;
	}
	_stext_bs = array[0];
	_end_bs  = array[0] + array[1];
	swapper_pg_dir_bs = _stext_bs - 0x4000;

	/* Obtain initrd information */
	ret = of_property_read_u32_array(mem, "initrd", array, 2);
	if (ret) {
		printk("Unable to read BiscuitOS initrd.\n");
		return -EINVAL;
	}
	phys_initrd_start_bs = array[0];
	phys_initrd_size_bs = array[1];

	/* swapper_pg_dir and init_mm */
	init_mm_bs.pgd = (pgd_t_bs *)init_mm.pgd;

	start_kernel();

	printk("Hello BiscuitOS\n");

	return 0;
}

static int BiscuitOS_memory_remove(struct platform_device *pdev)
{
	return 0;
}

static const struct of_device_id BiscuitOS_memory_of_match[] = {
	{ .compatible = "BiscuitOS,memory", },
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
MODULE_DESCRIPTION("BiscuitOS Memory Project");
