#ifndef _BISCUITOS_HIGHMEM_H
#define _BSICUTIOS_HIGHMEM_H

#include "asm-generated/kmap_types.h"
#include "asm-generated/fixmap.h"

#define LAST_PKMAP_BS			512
#define LAST_PKMAP_MASK_BS		(LAST_PKMAP_BS-1)

extern void *kmap_atomic_bs(struct page_bs *page, enum km_type_bs type);
extern void kunmap_atomic_bs(void *kvaddr, enum km_type_bs type);
extern struct page_bs *kmap_atomic_to_page_bs(void *ptr);

static inline void clear_highpage_bs(struct page_bs *page)
{
	void *kaddr = kmap_atomic_bs(page, KM_USER0_BS);
	clear_page_bs(kaddr);
	kunmap_atomic_bs(kaddr, KM_USER0_BS);
}

extern pte_t_bs *pkmap_page_table_bs;

extern void *FASTCALL_BS(kmap_high_bs(struct page_bs *page));
extern void FASTCALL_BS(kunmap_high_bs(struct page_bs *page));
extern void *kmap_bs(struct page_bs *page);
extern void kunmap_bs(struct page_bs *page);

static inline void check_highest_zone_bs(int k) {}

#endif
