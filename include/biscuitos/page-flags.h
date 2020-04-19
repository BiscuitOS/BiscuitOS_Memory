#ifndef _BISCUITOS_PAGE_FLAGS_H
#define _BISCUITOS_PAGE_FLAGS_H

#define PG_reserved_bs			11


#define SetPageReserved_bs(page)	set_bit(PG_reserved_bs, &(page)->flags)

#endif
