#ifndef _BISCUITOS_BOOTMEM_H
#define _BISCUITOS_BOOTMEM_H

#include "biscuitos/mmzone.h"
#include "biscuitos/cache.h"
#include "asm-generated/dma.h"

/*
 * simple boot-time physical memory area allocator.
 */

extern unsigned long max_low_pfn_bs;
extern unsigned long min_low_pfn_bs;

/*
 * higest page
 */
extern unsigned long max_pfn_bs;

/*
 * node_bootmem_map is a map pointer - the bits represent all physical
 * memory pages (including holes) on the node.
 */
typedef struct bootmem_data_bs {
	unsigned long node_boot_start;
	unsigned long node_low_pfn;
	void *node_bootmem_map;
	unsigned long last_offset;
	unsigned long last_pos;
	unsigned long last_success;	/* Previous allocation point.
					 * To speed up searching */
} bootmem_data_t_bs;

extern unsigned long __init bootmem_bootmap_pages_bs(unsigned long);
extern unsigned long __init init_bootmem_node_bs(pg_data_t_bs *,
				unsigned long, unsigned long, unsigned long);
extern void __init free_bootmem_node_bs(pg_data_t_bs *, 
					unsigned long, unsigned long);
extern void __init reserve_bootmem_node_bs(pg_data_t_bs *, unsigned long,
					unsigned long);
extern void * __init __alloc_bootmem_bs(unsigned long, 
					unsigned long, unsigned long);
extern void * __init __alloc_bootmem_node_bs(pg_data_t_bs *, 
	unsigned long, unsigned long, unsigned long);

#ifndef CONFIG_HAVE_ARCH_BOOTMEM_NODE
extern void __init reserve_bootmem_bs(unsigned long addr, unsigned long size);
#endif

#define alloc_bootmem_bs(x)						\
	__alloc_bootmem_bs((x), SMP_CACHE_BYTES_BS,			\
					__pa_bs(MAX_DMA_ADDRESS_BS))
#define alloc_bootmem_low_bs(x)						\
	__alloc_bootmem_bs((x), SMP_CACHE_BYTES_BS, 0)

#define alloc_bootmem_node_bs(pgdat, x)					\
	__alloc_bootmem_node_bs((pgdat), (x), SMP_CACHE_BYTES_BS,	\
					__pa_bs(MAX_DMA_ADDRESS_BS))
#define alloc_bootmem_low_pages_bs(x)					\
	__alloc_bootmem_bs((x), PAGE_SIZE_BS, 0)

#define alloc_bootmem_pages_bs(x)					\
	__alloc_bootmem_bs((x), PAGE_SIZE, __pa_bs(MAX_DMA_ADDRESS_BS))

extern unsigned long __init free_all_bootmem_node_bs(pg_data_t_bs *pgdat);
extern void __init free_bootmem_bs(unsigned long addr, unsigned long size);

extern void *__init alloc_large_system_hash_bs(const char *tablename,
				unsigned long bucketsize,
				unsigned long numentries,
				int scale,
				int flags,
				unsigned int *_hash_shift,
				unsigned int *_hash_mask,
				unsigned long limit);

#define HASH_HIGHMEM_BS	0x00000001	/* Consider highmem? */
#define HASH_EARLY_BS	0x00000002	/* Allocating during early boot? */

/* Only NUMA needs hash distribution.
 * IA64 is known to have sufficient vmalloc space.
 */
#if defined(CONFIG_NUMA) && defined(CONFIG_IA64)
#define HASHDIST_DEFAULT_BS	1
#else
#define HASHDIST_DEFAULT_BS	0
#endif
extern int __initdata hashdist_bs; /* Distribute hashes across NUMA nodes? */


#endif
