/*
 * BiscuitOS Memory Manager: PERCPU
 *
 * (C) 2019.10.01 BuddyZhang1 <buddy.zhang@aliyun.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/smp.h>
#include "biscuitos/kernel.h"
#include "biscuitos/init.h"
#include "biscuitos/percpu.h"
#include "asm-generated/arch.h"
#include "asm-generated/percpu.h"

struct node_percpu {
	unsigned long index;
	unsigned long offset;
};

/*
 * TestCase: alloc percpu
 */
static __unused DEFINE_PER_CPU_BS(struct node_percpu, node_percpu_bs);

static int __unused TestCase_percpu(void)
{
	struct node_percpu *node;
	int cpu = 0;

	/* set percpu */
	while (cpu < NR_CPUS_BS) {
		node = &per_cpu_bs(node_percpu_bs, cpu);
		node->index = cpu + 0x80;
		node->offset = cpu * 0x100;

		cpu++;
		if (cpu < NR_CPUS_BS)
			prefetch(&per_cpu_bs(node_percpu_bs, cpu));
	}

	/* get current cpu percpu */
	node = &__get_cpu_var_bs(node_percpu_bs);
	bs_debug("%d index %#lx\n", smp_processor_id(), node->index);

	/* get special cpu percpu */
	cpu = 2;
	BUG_ON_BS(cpu >= NR_CPUS_BS);
	node = &per_cpu_bs(node_percpu_bs, cpu);
	bs_debug("%d index %#lx\n", cpu, node->index);

	return 0;
}
percpu_initcall_bs(TestCase_percpu);
