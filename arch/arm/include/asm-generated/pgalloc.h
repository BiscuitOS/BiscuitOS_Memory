#ifndef _BISCUITOS_ARM_PGALLOC_H
#define _BISCUITOS_ARM_PGALLOC_H
/*
 *  arch/arm/include/asm/pgalloc.h
 *
 *  Copyright (C) 2000-2001 Russell King
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include "asm-generated/pgtable.h"
#include "asm-generated/domain.h"

#define _PAGE_USER_TABLE	(PMD_TYPE_TABLE | PMD_BIT4 | \
					PMD_DOMAIN(DOMAIN_USER))
#define _PAGE_KERNEL_TABLE	(PMD_TYPE_TABLE | PMD_BIT4 | \
					PMD_DOMAIN(DOMAIN_KERNEL))


#endif
