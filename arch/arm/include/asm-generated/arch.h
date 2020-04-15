#ifndef _BISCUITOS_ARM_ARCH_H
#define _BISCUITOS_ARM_ARCH_H

struct machine_desc {
	unsigned int phys_rams;		/* start of physical ram */
};

#define MAX_NUMNODES_BS	(1 << CONFIG_NODES_SHIFT)

#endif
