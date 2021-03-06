#ifndef _BISCUITOS_VMALLOC_H
#define _BISCUITOS_VMALLOC_H

#include "asm-generated/vmalloc.h"
#include "asm-generated/pgtable.h"

#define VM_IOREMAP_BS	0x00000001	/* ioremap() and friends */
#define VM_ALLOC_BS	0x00000002	/* vmalloc() */
#define VM_MAP_BS	0x00000004	/* vmap()ed pages */

struct page_bs;
struct vm_struct_bs {
	void			*addr;
	unsigned long		size;
	unsigned long		flags;
	struct page_bs		**pages;
	unsigned int		nr_pages;
	unsigned long		phys_addr;
	struct vm_struct_bs	*next;
};

void *vmalloc_bs(unsigned long size);
void vfree_bs(void *addr);
void *__vmalloc_bs(unsigned long size, unsigned int __nocast gfp_mask, 
                                                        pgprot_t_bs prot);
void vunmap_bs(void *addr);
void *vmap_bs(struct page_bs **, unsigned int, unsigned long, pgprot_t_bs);

extern void *vmalloc_32_bs(unsigned long size);
extern void *vmalloc_exec_bs(unsigned long size);

#endif
