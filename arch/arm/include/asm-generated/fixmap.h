#ifndef _BISCUITOS_ARM_FIXMAP_H
#define _BISCUITOS_ARM_FIXMAP_H

#include "asm-generated/kmap_types.h"
/*
 * fixmap.h: compile-time virtual memory allocation
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 1998 Ingo Molnar
 *
 * Support of BIGMEM added by Gerhard Wichert, Siemens AG, July 1999
 */

/* used by vmalloc.c, vsyscall.lds.S.
 *
 * Leave one empty page between vmalloc'ed areas and
 * the start of the fixmap.
 */
extern u32 BiscuitOS_fixmap_top;
#define __FIXADDR_TOP_BS	((unsigned long)BiscuitOS_fixmap_top)

/*
 * Here we define all the compile-time 'special' virtual
 * addresses. The point is to have a constant address at
 * compile time, but to set the physical address only
 * in the boot process. We allocate these special addresses
 * from the end of virtual memory (0xfffff000) backwards.
 * Also this lets us do fail-safe vmalloc(), we
 * can guarantee that these special addresses and
 * vmalloc()-ed addresses never overlap.
 *
 * these 'compile-time allocated' memory buffers are
 * fixed-size 4k pages. (or larger if used with an increment
 * highger than 1) use fixmap_set(idx,phys) to associate
 * physical memory with fixmap indices.
 *
 * TLB entries of such buffers will not be flushed across
 * task switches.
 */
enum fixed_addresses_bs {
        FIX_HOLE_BS,
        FIX_VSYSCALL_BS,
#ifdef CONFIG_HIGHMEM_BS
        FIX_KMAP_BEGIN_BS, /* reserved pte's for temporary kernel mappings */
        FIX_KMAP_END_BS = FIX_KMAP_BEGIN_BS+(KM_TYPE_NR_BS*NR_CPUS_BS)-1,
#endif
        __end_of_permanent_fixed_addresses_bs,
        /* temporary boot-time mappings, used before ioremap() is functional */
#define NR_FIX_BTMAPS_BS   16
        FIX_BTMAP_END_BS = __end_of_permanent_fixed_addresses_bs,
        FIX_BTMAP_BEGIN_BS = FIX_BTMAP_END_BS + NR_FIX_BTMAPS_BS - 1,
        __end_of_fixed_addresses_bs
};

#define FIXADDR_TOP_BS		((unsigned long)__FIXADDR_TOP_BS)
#define __FIXADDR_SIZE_BS	(__end_of_permanent_fixed_addresses_bs << \
							PAGE_SHIFT_BS)
#define __FIXADDR_BOOT_SIZE_BS	(__end_of_fixed_addresses_bs << \
							PAGE_SHIFT_BS)
#define FIXADDR_START_BS	(FIXADDR_TOP_BS - __FIXADDR_SIZE_BS)
#define FIXADDR_BOOT_START_BS	(FIXADDR_TOP_BS - __FIXADDR_BOOT_SIZE_BS)

#define __fix_to_virt_bs(x)	(FIXADDR_TOP_BS - ((x) << PAGE_SHIFT_BS))
#define __virt_to_fix_bs(x)	((FIXADDR_TOP_BS - ((x)&PAGE_MASK_BS)) >> \
							PAGE_SHIFT_BS)

/*
 * 'index to address' translation. If anyone tries to use the idx
 * directly without tranlation, we catch the bug with a NULL-deference
 * kernel oops. Illegal ranges of incoming indices are caught too.
 */
static inline unsigned long fix_to_virt_bs(const unsigned int idx)
{
	/*
	 * this branch gets completely eliminated after inlining,
	 * except when someone tries to use fixaddr indices in an
	 * illegal way. (such as mixing up address types or using
	 * out-of-range indices).
	 *
	 * If it doesn't get removed, the linker will complain
	 * loudly with a reasonably clear error message..
	 */
	if (idx >= __end_of_fixed_addresses_bs)
		BUG_BS();

	return __fix_to_virt_bs(idx);
}

static inline unsigned long virt_to_fix_bs(const unsigned long vaddr)
{
	BUG_ON_BS(vaddr >= FIXADDR_TOP_BS || vaddr < FIXADDR_START_BS);
	return __virt_to_fix_bs(vaddr);
}

#endif
