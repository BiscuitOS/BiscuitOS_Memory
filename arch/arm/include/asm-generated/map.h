#ifndef _BISCUITOS_ARM_MAP_H
#define _BISCUITOS_ARM_MAP_H

struct map_desc {
	unsigned long virtual;
	unsigned long physical;
	unsigned long length;
	unsigned int type;
};

struct meminfo;

#define MT_DEVICE		0
#define MT_CACHECLEAN		1
#define MT_MINICLEAN		2
#define MT_LOW_VECTORS		3
#define MT_HIGH_VECTORS		4
#define MT_MEMORY		5
#define MT_ROM			6
#define MT_IXP2000_DEVICE	7

extern void memtable_init_bs(struct meminfo *);

#endif
