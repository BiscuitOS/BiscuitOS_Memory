#ifndef _BISCUITOS_MM_TYPES_H
#define _BISCUTIOS_MM_TYPES_H

#include <linux/spinlock.h>

struct mm_struct_bs
{
	pgd_t_bs *pgd;
	spinlock_t page_table_lock;
};

#endif
