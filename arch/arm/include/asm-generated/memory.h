#ifndef _BISCUITOS_ARM_MEMORY_H
#define _BISCUITOS_ARM_MEMORY_H

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

#define PHYS_TO_NID(addr)	(0)

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

#endif
