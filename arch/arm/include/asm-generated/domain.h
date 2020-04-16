/*
 *  linux/include/asm-arm/domain.h
 *
 *  Copyright (C) 1999 Russell King.
 *              
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#ifndef _BISCUITOS_ARM_DOMAIN_H
#define _BISCUITOS_ARM_DOMAIN_H

/*
 * Domain numbers
 *
 * DOMAIN_IO	 - domain 2 includes all IO only
 * DOMAIN_USER   - domain 1 includes all user memory only
 * DOMAIN_KERNEL - domain 0 includes all kernel memory only
 */
#define DOMAIN_KERNEL	0
#define DOMAIN_TABLE	0
#define DOMAIN_USER	1
#define DOMAIN_IO	2

#endif
