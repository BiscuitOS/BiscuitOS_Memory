#ifndef _BISCUITOS_ARM_MAP_H
#define _BISCUITOS_ARM_MAP_H

struct map_desc {
	unsigned long virtual;
	unsigned long physical;
	unsigned long length;
	unsigned int type;
};

struct meminfo;

/*
 * Architecture ioremap implementation.
 */
#define MT_DEVICE		0 
#define MT_DEVICE_NONSHARED	1 
#define MT_DEVICE_CACHED	2 
#define MT_DEVICE_WC		3

/* types 0-3 are defined in asm/io.h */
enum {
	MT_UNCACHED = 4,
	MT_CACHECLEAN,
	MT_MINICLEAN,
	MT_LOW_VECTORS,
	MT_HIGH_VECTORS,
	MT_MEMORY_RWX,
	MT_MEMORY_RW,
	MT_ROM,
	MT_MEMORY_RWX_NONCACHED,
	MT_MEMORY_RW_DTCM,
	MT_MEMORY_RWX_ITCM,
	MT_MEMORY_RW_SO,
	MT_MEMORY_DMA_READY,
};

extern void memtable_init_bs(struct meminfo *);
extern void __init create_memmap_holes_bs(struct meminfo *mi);

#endif
