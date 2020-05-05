#ifndef _BISCUITOS_ARM_MAP_H
#define _BISCUITOS_ARM_MAP_H

struct map_desc {
	unsigned long virtual;
	unsigned long pfn;
	unsigned long length;
	unsigned int type;
};

struct meminfo;

/*
 * Architecture ioremap implementation.
 */
#define MT_DEVICE_BS		0 
#define MT_DEVICE_NONSHARED_BS	1 
#define MT_DEVICE_CACHED_BS	2 
#define MT_DEVICE_WC_BS		3

/* types 0-3 are defined in asm/io.h */
enum {
	MT_UNCACHED_BS = 4,
	MT_CACHECLEAN_BS,
	MT_MINICLEAN_BS,
	MT_LOW_VECTORS_BS,
	MT_HIGH_VECTORS_BS,
	MT_MEMORY_RWX_BS,
	MT_MEMORY_RW_BS,
	MT_ROM_BS,
	MT_MEMORY_RWX_NONCACHED_BS,
	MT_MEMORY_RW_DTCM_BS,
	MT_MEMORY_RWX_ITCM_BS,
	MT_MEMORY_RW_SO_BS,
	MT_MEMORY_DMA_READY_BS,
};

#define __phys_to_pfn_bs(paddr)		((paddr) >> PAGE_SHIFT_BS)
#define __pfn_to_phys_bs(pfn)		((pfn) << PAGE_SHIFT_BS)

extern void memtable_init_bs(struct meminfo *);
extern void __init create_memmap_holes_bs(struct meminfo *mi);

#endif
