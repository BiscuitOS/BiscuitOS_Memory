/*
 * mm/map.c
 *
 * (C) 2020.04.14 BuddyZhang1 <buddy.zhang@aliyun.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/kernel.h>
#include "asm-generated/pgtable.h"

/* description of effects of mapping type and prot in current implementation.
 * this is due to the limited x86 page protection hardware.  The expected
 * behavior is in parens:
 *              
 * map_type     prot
 *              PROT_NONE       PROT_READ       PROT_WRITE      PROT_EXEC
 * MAP_SHARED   r: (no) no      r: (yes) yes    r: (no) yes     r: (no) yes
 *              w: (no) no      w: (no) no      w: (yes) yes    w: (no) no
 *              x: (no) no      x: (no) yes     x: (no) yes     x: (yes) yes
 *              
 * MAP_PRIVATE  r: (no) no      r: (yes) yes    r: (no) yes     r: (no) yes
 *              w: (no) no      w: (no) no      w: (copy) copy  w: (no) no
 *              x: (no) no      x: (no) yes     x: (no) yes     x: (yes) yes
 *
 */
pgprot_t protection_map_bs[16] = {
	__P000, __P001, __P010, __P011, __P100, __P101, __P110, __P111,
	__S000, __S001, __S010, __S011, __S100, __S101, __S110, __S111
};
