#ifndef _BISCUITOS_ARM_PAGE_H
#define _BISCUITOS_ARM_PAGE_H

/* PAGE_SHIFT determines the page size */
#define PAGE_SHIFT_BS	12
#define PAGE_SIZE_BS	(1UL << PAGE_SHIFT_BS)
#define PAGE_MASK_BS	(~(PAGE_SIZE_BS-1))

/* to align the pointer to the (next) page boundary */
#define PAGE_ALIGN_BS(addr)	(((addr)+PAGE_SIZE_BS-1)&PAGE_MASK_BS)

#endif
