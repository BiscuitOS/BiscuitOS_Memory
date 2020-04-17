/*
 *  linux/arch/arm/mm/cache-v7.S
 *
 *  Copyright (C) 2001 Deep Blue Solutions Ltd.
 *  Copyright (C) 2005 ARM Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 *  This is the "shell" of the ARMv7 processor support.
 */
#include <linux/linkage.h>
#include <asm/assembler.h>

/*
 *      v7_flush_dcache_all()
 *
 *      Flush the whole D-cache.
 *
 *      Corrupted registers: r0-r7, r9-r11 (r6 only in Thumb mode)
 *
 *      - mm    - mm_struct describing address space
 */
ENTRY(v7_flush_dcache_all_bs)
	dmb	@ ensure ordering with previous memory accesses
	mrc	p15, 1, r0, c0, c0, 1		@ read clidr
	mov	r3, r0, lsr #23			@ move LoC into position
	ands	r3, r3, #7 << 1			@ extract LoC*2 from clidr
	beq     finished		@ if loc is 0, then no need to clean
start_flush_levels:
	mov	r10, #0			@ start clean at cache level 0
flush_levels:
	add	r2, r10, r10, lsr #1	@ work out 3x current cache level
	mov	r1, r0, lsr r2		@ extract cache type bits from clidr
	and	r1, r1, #7	@ mask of the bits for current cache only
	cmp	r1, #2			@ see what cache we have at this level
	blt	skip			@ skip if no cache, or just i-cache
	mcr	p15, 2, r10, c0, c0, 0	@ select current cache level in cssr
	isb				@ isb to sych the new cssr&csidr
	mrc	p15, 1, r1, c0, c0, 0	@ read the new csidr
	and	r2, r1, #7		@ extract the length of the cache lines
	add	r2, r2, #4		@ add 4 (line length offset)
	movw	r4, #0x3ff
	ands	r4, r4, r1, lsr #3	@ find maximum number on the way size
	clz	r5, r4		@ find bit position of way size increment
	movw	r7, #0x7fff
	ands	r7, r7, r1, lsr #13	@ extract max number of the index size
loop1:
	mov	r9, r7			@ create working copy of max index
loop2:
	orr	r11, r10, r4, lsl r5	@ factor way and cache number into r11
	orr	r11, r11, r9, lsl r2	@ factor index number into r11
	mcr	p15, 0, r11, c7, c14, 2	@ clean & invalidate by set/way
	subs	r9, r9, #1		@ decrement the index
	bge	loop2
	subs	r4, r4, #1		@ decrement the way
	bge	loop1
skip:
	add	r10, r10, #2		@ increment cache number
	cmp	r3, r10
	bgt	flush_levels
finished:
	mov	r10, #0			@ switch back to cache level 0
	mcr	p15, 2, r10, c0, c0, 0	@ select current cache level in cssr
	dsb	st
	isb
	ret	lr
ENDPROC(v7_flush_dcache_all_bs)


/*
 *      v7_flush_cache_all()
 *      
 *      Flush the entire cache system.
 *  The data cache flush is now achieved using atomic clean / invalidates
 *  working outwards from L1 cache. This is done using Set/Way based cache
 *  maintenance instructions.
 *  The instruction cache can still be invalidated back to the point of
 *  unification in a single instruction.
 *              
 */             
ENTRY(v7_flush_kern_cache_all_bs)
	stmfd	sp!, {r4-r5, r7, r9-r11, lr}
	bl	v7_flush_dcache_all_bs
	mov	r0, #0 
	ALT_SMP(mcr	p15, 0, r0, c7, c1, 0)	@ invalidate I-cache inner shareable            
	ALT_UP(mcr	p15, 0, r0, c7, c5, 0)	@ I+BTB cache invalidate
	ldmfd	sp!, {r4-r5, r7, r9-r11, lr}
        ret     lr
ENDPROC(v7_flush_kern_cache_all_bs)
