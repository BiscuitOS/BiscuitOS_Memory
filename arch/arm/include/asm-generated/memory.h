#ifndef _BISCUITOS_ARM_MEMORY_H
#define _BISCUITOS_ARM_MEMORY_H

#include <asm-generated/page.h>

#ifndef TASK_SIZE_BS
/*
 * TASK_SIZE_BS - the maximum size of a user space task.
 * TASK_UNMAPPED_BASE - the lower boundary of the mmap VM area
 */
#define TASK_SIZE_BS		(0x7f000000UL)
#define TASK_UNMAPPED_BASE_BS	(0x40000000UL)
#endif

/*
 * Physical DRAM offset.
 */
extern phys_addr_t BiscuitOS_ram_base;
#define PHYS_OFFSET_BS	BiscuitOS_ram_base

/*
 * Page offset
 */
extern u32 BiscuitOS_PAGE_OFFSET;
#ifndef PAGE_OFFSET_BS
#define PAGE_OFFSET_BS	BiscuitOS_PAGE_OFFSET
#endif

/*
 * PFNs are used to describe any physical page; this means
 * PFN 0 == physical address 0.
 *
 * This is the PFN of the first RAM page in the kernel
 * direct-mapped view.  We assume this is the first page
 * of RAM in the mem_map as well.
 */
#define PHYS_PFN_OFFSET_BS	(PHYS_OFFSET_BS >> PAGE_SHIFT_BS)

/*
 * Conversion between a struct page and a physical address.
 *
 * Note: when converting an unknown physical address to a
 * struct page, the resulting pointer must be validated
 * using VALID_PAGE().  It must return an invalid struct page
 * for any physical address not corresponding to a system
 * RAM address.
 *
 *  page_to_pfn(page)   convert a struct page * to a PFN number
 *  pfn_to_page(pfn)    convert a _valid_ PFN number to struct page *
 *  pfn_valid(pfn)      indicates whether a PFN number is valid
 *
 *  virt_to_page(k)     convert a _valid_ virtual address to struct page *
 *  virt_addr_valid(k)  indicates whether a virtual address is valid
 */
#ifndef CONFIG_DISCONTIGMEM

#define page_to_pfn_bs(page)	(((page) - mem_map_bs) + PHYS_PFN_OFFSET_BS)
#define pfn_to_page_bs(pfn)	((mem_map_bs + (pfn)) - PHYS_PFN_OFFSET_BS)
#define pfn_valid_bs(pfn)	((pfn) >= PHYS_PFN_OFFSET_BS && \
				(pfn) < (PHYS_PFN_OFFSET_BS + max_mapnr_bs))

#define virt_to_page_bs(kaddr)	(pfn_to_page_bs(__pa_bs(kaddr) >> \
							PAGE_SHIFT_BS))
#define virt_addr_valid_bs(kaddr) ((unsigned long)(kaddr) >= PAGE_OFFSET_BS &&\
		 (unsigned long)(kaddr) < (unsigned long)high_memory_bs)

#define PHYS_TO_NID_BS(addr)	(0)

#endif

/*
 * Physical vs virtual RAM address space conversion. These are
 * private definitions which should NOT be used outside memory.h
 * files. Use virt_to_phys/phys_to_virt/__pa/__va instead.
 */
#ifndef _virt_to_phys
#define __virt_to_phys_bs(x)	((x) - PAGE_OFFSET_BS + PHYS_OFFSET_BS)
#define __phys_to_virt_bs(x)	((x) - PHYS_OFFSET_BS + PAGE_OFFSET_BS)
#endif

/*
 * These are *only* valid on the kernel direct mapped RAM memory.
 * Note: Drivers should NOT use these. They are the wrong
 * translation for translating DMA addresses. Use the driver
 * DMA support - see dma-mapping.h.
 */
static inline unsigned long virt_to_phys_bs(void *x)
{
	return __virt_to_phys_bs((unsigned long)(x));
}

static inline void *phys_to_virt_bs(unsigned long x)
{
	return (void *)(__phys_to_virt_bs((unsigned long)(x)));
}

/*
 * Drivers should NOT use these either.
 */
#define __pa_bs(x)	__virt_to_phys_bs((unsigned long)(x))
#define __va_bs(x)	((void *)__phys_to_virt_bs((unsigned long)(x)))

#define PHYS_RELATIVE(v_data, v_text)	((v_data) - (v_text))

#include "asm-generated/setup.h"

extern u32 BiscuitOS_dma_size;
extern struct meminfo highmeminfo_bs;
/*
 * Only first 4MB of memory can be accessed via PCI.
 * We use GFP_DMA to allocate safe buffers to do map/unmap.
 * This is really ugly and we need a better way of specifying
 * DMA-capable regions of memory.
 */
static inline void __arch_adjust_zones_bs(int node,
		unsigned long *zone_size, unsigned long *zhole_size)
{
	unsigned int sz = BiscuitOS_dma_size >> PAGE_SHIFT_BS;

	/*
	 * Only adjust if > 4M on current system
	 */
	if (node || (zone_size[0] < sz))
		return;

	zone_size[1] = zone_size[0] - sz;
	zone_size[0] = sz;
	zhole_size[1] = zhole_size[0];
	zhole_size[0] = 0;

#ifdef CONFIG_HIGHMEM_BS
	/* FIXME: Default ARM doesn't support HighMem Zone,
	 * BiscuitOS support HighMem Zone.
	 */
	zone_size[2] = highmeminfo_bs.bank[0].size >> PAGE_SHIFT_BS;
	zhole_size[2] = 0;
	zone_size[1] -= zone_size[2];
	zhole_size[0] = zhole_size[1] = 0;
#endif

}


#ifndef arch_adjust_zones_bs
#define arch_adjust_zones_bs(node,size,holes)	\
		__arch_adjust_zones_bs(node, size, holes)
#endif

extern unsigned long max_mapnr_bs;

#endif
