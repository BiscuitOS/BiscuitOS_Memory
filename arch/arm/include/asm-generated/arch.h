#ifndef _BISCUITOS_ARM_ARCH_H
#define _BISCUITOS_ARM_ARCH_H

struct machine_desc {
	unsigned int phys_rams;		/* start of physical ram */
	void (*map_io)(void);
};

#define MAX_NUMNODES_BS		(1 << CONFIG_NODES_SHIFT)
#define NODES_SHIFT_BS		CONFIG_NODES_SHIFT

#ifdef CONFIG_SMP
#define NR_CPUS_BS	CONFIG_NR_CPUS_BS
#else
#define NR_CPUS_BS	1
#endif

#endif
