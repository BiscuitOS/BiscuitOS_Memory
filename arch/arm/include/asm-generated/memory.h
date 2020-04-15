#ifndef _BISCUITOS_ARM_MEMORY_H
#define _BISCUITOS_ARM_MEMORY_H

/*
 * Physical DRAM offset.
 */
#define PHYS_OFFSET	CONFIG_PHYS_OFFSET_BS

/*
 * Page offset
 */
#ifndef PAGE_OFFSET
#define PAGE_OFFSET	CONFIG_PAGE_OFFSET_BS
#endif

#define PHYS_TO_NID(addr)	(0)

/*
 * Physical vs virtual RAM address space conversion. These are
 * private definitions which should NOT be used outside memory.h
 * files. Use virt_to_phys/phys_to_virt/__pa/__va instead.
 */
#ifndef _virt_to_phys
#define __virt_to_phys_bs(x)	((x) - PAGE_OFFSET + PHYS_OFFSET)
#define __phys_to_virt_bs(x)	((x) - PHYS_OFFSET + PAGE_OFFSET)
#endif

/*
 * Drivers should NOT use these either.
 */
#define __pa_bs(x)	__virt_to_phys_bs((unsigned long)(x))
#define __va_bs(x)	((void *)__phys_to_virt_bs((unsigned long)(x)))

#endif
