#ifndef _BISCUITOS_ARM_HIGHMEM_H
#define _BISCUITOS_ARM_HIGHMEM_H

#include "asm-generated/fixmap.h"
#include "asm-generated/vmalloc.h"

/*
 * Right now we initialize only a single pte table. It can be extended
 * easily, subsequent pte tables have to be allocated in one physical
 * chunk of RAM.
 */
#define LAST_PKMAP_BS	512

/*
 * Ordering is:
 *
 * FIXADDR_TOP
 *                      fixed_addresses
 * FIXADDR_START
 *                      temp fixed addresses
 * FIXADDR_BOOT_START
 *                      Persistent kmap area
 * PKMAP_BASE
 * VMALLOC_END
 *                      Vmalloc area
 * VMALLOC_START
 * high_memory
 */
#define PKMAP_BASE_BS		((VMALLOC_OFFSET_BS + VMALLOC_END_BS + \
				PMD_SIZE_BS - 1) & PMD_MASK_BS)
#define LAST_PKMAP_MASK_BS	(LAST_PKMAP_BS-1)
#define PKMAP_NR_BS(virt)	((virt-PKMAP_BASE_BS) >> PAGE_SHIFT_BS)
#define PKMAP_ADDR_BS(nr)	(PKMAP_BASE_BS + ((nr) << PAGE_SHIFT_BS))


#define flush_cache_kmaps_bs()		do { } while (0)
#define flush_tlb_kernel_range_bs(x,y)	do { } while (0)	

extern pgprot_t_bs kmap_prot_bs;
extern pte_t_bs *kmap_pte_bs;

#endif
