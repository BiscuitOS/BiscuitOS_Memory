/*
 *  linux/arch/arm/kernel/smp_tlb.c
 *
 *  Copyright (C) 2002 ARM Limited, All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/kernel.h>
#include "biscuitos/kernel.h"
#include "asm-generated/smp_plat.h"
#include "asm-generated/pgtable.h"
#include "asm-generated/tlbflush.h"

void flush_tlb_all_bs(void)
{
	if (tlb_ops_need_broadcast_bs())
		BS_DUP();
	else
		__flush_tlb_all_bs();
}
