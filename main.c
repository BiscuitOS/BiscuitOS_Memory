/*
 * BiscuitOS Memory Manager Design (MMU)
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

#define DEV_NAME "BiscuitOS"

/* BiscuitOS debug stuf */
int BiscuitOS_debug = 0;
EXPORT_SYMBOL_GPL(BiscuitOS_debug);

extern asmlinkage void __init start_kernel_bs(void);

static int BiscuitOS_memory_probe(struct platform_device *pdev)
{
	start_kernel_bs();
	printk("Hello BiscuitOS: BiscuitOS MMU Design Base\n");

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
