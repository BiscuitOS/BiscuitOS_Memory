/*
 *  linux/arch/arm/kernel/head-common.S
 *
 *  Copyright (C) 1994-2002 Russell King
 *  Copyright (c) 2003 ARM Limited
 *  All Rights Reserved
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */
#include <linux/linkage.h>
#include <asm/assembler.h>

/*
 * This provides a C-API version of __lookup_processor_type_bs
 */
ENTRY(lookup_processor_type_bs)
	stmfd	sp!, {r4 - r6, r9, lr}
	mov	r9, r0
	ldmfd	sp!, {r4 - r6, r9, pc}
ENDPROC(lookup_processor_type_bs)
