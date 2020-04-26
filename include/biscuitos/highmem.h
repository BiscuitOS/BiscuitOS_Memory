#ifndef _BISCUITOS_HIGHMEM_H
#define _BSICUTIOS_HIGHMEM_H

#define kmap_atomic_bs(page, idx)	page_address_bs(page)
#define kunmap_atomic_bs(addr, idx)	do { } while (0)

static inline void clear_highpage_bs(struct page_bs *page)
{
	void *kaddr = kmap_atomic_bs(page, KM_USER0_BS);
	clear_page_bs(kaddr);
	kunmap_atomic_bs(kaddr, KM_USER0_BS);
}

#endif
