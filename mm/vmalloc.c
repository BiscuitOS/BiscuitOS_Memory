/*
 *  linux/mm/vmalloc.c
 *
 *  Copyright (C) 1993  Linus Torvalds
 *  Support of BIGMEM added by Gerhard Wichert, Siemens AG, July 1999
 *  SMP-safe vmalloc/vfree/ioremap, Tigran Aivazian <tigran@veritas.com>, May 2000
 *  Major rework to support vmap/vunmap, Christoph Hellwig, SGI, August 2002
 */

#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/errno.h>
#include "biscuitos/gfp.h"
#include "biscuitos/vmalloc.h"
#include "biscuitos/mm.h"
#include "biscuitos/slab.h"
#include "biscuitos/kernel.h"
#include "asm-generated/pgtable.h"
#include "asm-generated/cacheflush.h"
#include "asm-generated/tlbflush.h"

DEFINE_RWLOCK(vmlist_lock_bs);
struct vm_struct_bs *vmlist_bs;

static void 
vunmap_pte_range_bs(pmd_t_bs *pmd, unsigned long addr, unsigned long end)
{
	pte_t_bs *pte;

	pte = pte_offset_kernel_bs(pmd, addr);
	do {
		pte_t ptent = ptep_get_and_clear_bs(&init_mm_bs, addr, pte);
		WARN_ON_BS(!pte_none_bs(ptent) && !pte_present_bs(ptent));
	} while (pte++, addr += PAGE_SIZE_BS, addr != end);
}

static inline void vunmap_pmd_range_bs(pud_t_bs *pud, unsigned long addr,
							unsigned long end)
{
	pmd_t_bs *pmd;
	unsigned long next;

	pmd = pmd_offset_bs(pud, addr);
	do {
		next = pmd_addr_end_bs(addr, end);
		vunmap_pte_range_bs(pmd, addr, next);
	} while (pmd++, addr = next, addr != end);
}

static inline void vunmap_pud_range_bs(pgd_t_bs *pgd,
			unsigned long addr, unsigned long end)
{
	pud_t_bs *pud;
	unsigned long next;

	pud = pud_offset_bs(pgd, addr);
	do {
		next = pud_addr_end_bs(addr, end);
		if (pud_none_or_clear_bad_bs(pud))
			continue;
		vunmap_pmd_range_bs(pud, addr, next);
	} while (pud++, addr = next, addr != end);
}

void unmap_vm_area_bs(struct vm_struct_bs *area)
{
	pgd_t_bs *pgd;
	unsigned long next;
	unsigned long addr = (unsigned long)area->addr;
	unsigned long end = addr + area->size;

	BUG_ON_BS(addr >= end);
	pgd = pgd_offset_k_bs(addr);
	flush_cache_vunmap_bs(addr, end);
	do {
		next = pgd_addr_end_bs(addr, end);
		if (pgd_none_or_clear_bad_bs(pgd))
			continue;
		vunmap_pud_range_bs(pgd, addr, next);
	} while (pgd++, addr = next, addr != end);
	flush_tlb_kernel_range_bs((unsigned long)area->addr, end);
}

static int vmap_pte_range_bs(pmd_t_bs *pmd, unsigned long addr,
		unsigned long end, pgprot_t_bs prot, struct page_bs ***pages)
{
	pte_t_bs *pte;

	pte = pte_alloc_kernel_bs(&init_mm_bs, pmd, addr);
	if (!pte)
		return -ENOMEM;
	do {
		struct page_bs *page = **pages;

		WARN_ON_BS(!pte_none_bs(*pte));
		if (!page)
			return -ENOMEM;
		set_pte_at_bs(&init_mm_bs, addr, pte, mk_pte_bs(page, prot));
		(*pages)++;
	} while (pte++, addr += PAGE_SIZE, addr != end);
	return 0;
}

static inline int vmap_pmd_range_bs(pud_t_bs *pud, unsigned long addr,
		unsigned long end, pgprot_t_bs prot, struct page_bs ***pages)
{
	pmd_t_bs *pmd;
	unsigned long next;

	pmd = pmd_alloc_bs(&init_mm_bs, pud, addr);
	if (!pmd)
		return -ENOMEM;
	do {
		next = pmd_addr_end_bs(addr, end);
		if (vmap_pte_range_bs(pmd, addr, next, prot, pages))
			return -ENOMEM;
	} while (pmd++, addr = next, addr != end);
	return 0;
}

static inline int vmap_pud_range_bs(pgd_t_bs *pgd, unsigned long addr,
		unsigned long end, pgprot_t_bs prot, struct page_bs ***pages)
{
	pud_t_bs *pud;
	unsigned long next;

	pud = pud_alloc_bs(&init_mm_bs, pgd, addr);
	if (!pud)
		return -ENOMEM;
	do {
		next = pud_addr_end_bs(addr, end);
		if (vmap_pmd_range_bs(pud, addr, next, prot, pages))
			return -ENOMEM;
	} while (pud++, addr = next, addr != end);
	return 0;
}

int map_vm_area_bs(struct vm_struct_bs *area, pgprot_t_bs prot,
						struct page_bs ***pages)
{
	pgd_t_bs *pgd;
	unsigned long next;
	unsigned long addr = (unsigned long)area->addr;
	unsigned long end = addr + area->size - PAGE_SIZE_BS;
	int err;

	BUG_ON_BS(addr >= end);
	pgd = pgd_offset_k_bs(addr);
	spin_lock(&init_mm_bs.page_table_lock);
	do {
		next = pgd_addr_end_bs(addr, end);
		err = vmap_pud_range_bs(pgd, addr, next, prot, pages);
		if (err)
			break;
	} while (pgd++, addr = next, addr != end);
	spin_unlock(&init_mm_bs.page_table_lock);
	flush_cache_vmap_bs((unsigned long) area->addr, end);
	return err;
}

#define IOREMAP_MAX_ORDER_BS	(7 + PAGE_SHIFT_BS)	/* 128 pages */

struct vm_struct_bs *__get_vm_area_bs(unsigned long size, unsigned long flags,
			unsigned long start, unsigned long end)
{
	struct vm_struct_bs **p, *tmp, *area;
	unsigned long align = 1;
	unsigned long addr;

	if (flags & VM_IOREMAP_BS) {
		int bit = fls(size);

		if (bit > IOREMAP_MAX_ORDER_BS)
			bit = IOREMAP_MAX_ORDER_BS;
		else if (bit < PAGE_SHIFT_BS)
			bit = PAGE_SHIFT_BS;

		align = 1ul << bit;
	}
	addr = ALIGN(start, align);
	size = PAGE_ALIGN_BS(size);

	area = kmalloc_bs(sizeof(*area), GFP_KERNEL_BS);
	if (unlikely(!area))
		return NULL;

	if (unlikely(!size)) {
		kfree_bs(area);
		return NULL;
	}

	/*
	 * We always allocate a guard page.
	 */
	size += PAGE_SIZE;

	write_lock(&vmlist_lock_bs);
	for (p = &vmlist_bs; (tmp = *p) != NULL; p = &tmp->next) {
		if ((unsigned long)tmp->addr < addr) {
			if ((unsigned long)tmp->addr + tmp->size >= addr)
				addr = ALIGN(tmp->size +
					(unsigned long)tmp->addr, align);
			continue;
		}
		if ((size + addr) < addr)
			goto out;
		if (size + addr <= (unsigned long)tmp->addr)
			goto found;
		addr = ALIGN(tmp->size + (unsigned long)tmp->addr, align);
		if (addr > end - size)
			goto out;
	}

found:
	area->next = *p;
	*p = area;

	area->flags = flags;
	area->addr = (void *)addr;
	area->size = size;
	area->pages = NULL;
	area->nr_pages = 0;
	area->phys_addr = 0;
	write_unlock(&vmlist_lock_bs);

	return area;
out:
	write_unlock(&vmlist_lock_bs);
	kfree_bs(area);
	printk(KERN_WARNING "allocation failed: out of vmalloc space - "
				"use vmalloc=<size> to increase size.\n");
	return NULL;
}

/**
 *      get_vm_area  -  reserve a contingous kernel virtual area
 *
 *      @size:          size of the area
 *      @flags:         %VM_IOREMAP for I/O mappings or VM_ALLOC
 *
 *      Search an area of @size in the kernel virtual mapping area,
 *      and reserved it for out purposes.  Returns the area descriptor
 *      on success or %NULL on failure.
 */
struct vm_struct_bs *get_vm_area_bs(unsigned long size, unsigned long flags)
{
	return __get_vm_area_bs(size, flags, VMALLOC_START_BS, VMALLOC_END_BS);
}

/* Caller must hold vmlist_lock */
struct vm_struct_bs *__remove_vm_area_bs(void *addr)
{
	struct vm_struct_bs **p, *tmp;

	for (p = &vmlist_bs; (tmp = *p) != NULL; p = &tmp->next) {
		if (tmp->addr == addr)
			goto found;
	}
	return NULL;

found:
	unmap_vm_area_bs(tmp);
	*p = tmp->next;

	/*
	 * Remove the guard page.
	 */
	tmp->size -= PAGE_SIZE_BS;
	return tmp;
}

/**
 *      remove_vm_area  -  find and remove a contingous kernel virtual area
 *
 *      @addr:          base address
 *
 *      Search for the kernel VM area starting at @addr, and remove it.
 *      This function returns the found VM area, but using it is NOT safe
 *      on SMP machines, except for its size or flags.
 */
struct vm_struct_bs *remove_vm_area_bs(void *addr)
{
	struct vm_struct_bs *v;

	write_lock(&vmlist_lock_bs);
	v = __remove_vm_area_bs(addr);
	write_unlock(&vmlist_lock_bs);
	return v;
}

void *__vmalloc_area_bs(struct vm_struct_bs *area, 
			unsigned int __nocast gfp_mask, pgprot_t_bs prot)
{
	struct page_bs **pages;
	unsigned int nr_pages, array_size, i;

	nr_pages = (area->size - PAGE_SIZE) >> PAGE_SHIFT;
	array_size = (nr_pages * sizeof(struct page_bs *));

	area->nr_pages = nr_pages;
	/* Please note that the recursion is strictly bounded. */
	if (array_size > PAGE_SIZE_BS)
		pages = __vmalloc_bs(array_size, gfp_mask, PAGE_KERNEL_BS);
	else
		pages = kmalloc_bs(array_size, (gfp_mask & ~__GFP_HIGHMEM_BS));
	area->pages = pages;
	if (!area->pages) {
		remove_vm_area_bs(area->addr);
		kfree_bs(area);
		return NULL;
	}
	memset(area->pages, 0, array_size);

	for (i = 0; i < area->nr_pages; i++) {
		area->pages[i] = alloc_page_bs(gfp_mask);
		if (unlikely(!area->pages[i])) {
			/* Successfully allocated i pages, free them in
			 * __vunmap()
			 */
			area->nr_pages = i;
			goto fail;
		}
	}

	if (map_vm_area_bs(area, prot, &pages))
		goto fail;
	return area->addr;

fail:
	vfree_bs(area->addr);
	return NULL;
}

void __vunmap_bs(void *addr, int deallocate_pages)
{
	struct vm_struct_bs *area;

	if (!addr)
		return;

	if ((PAGE_SIZE_BS - 1) & (unsigned long)addr) {
		printk(KERN_ERR "Trying to vfree() bad address (%p)\n", addr);
		WARN_ON_BS(1);
		return;
	}

	area = remove_vm_area_bs(addr);
	if (unlikely(!area)) {
		printk(KERN_ERR "Trying to vfree() nonexistent vm area (%p)\n",
					addr);
		WARN_ON_BS(1);
		return;
	}

	if (deallocate_pages) {
		int i;

		for (i = 0; i < area->nr_pages; i++) {
			if (unlikely(!area->pages[i]))
				BUG_BS();
			__free_page_bs(area->pages[i]);
		}

		if (area->nr_pages > PAGE_SIZE_BS/sizeof(struct page_bs *))
			vfree_bs(area->pages);
		else
			kfree_bs(area->pages);
	}

	kfree_bs(area);
	return;
}

/**
 *      vunmap  -  release virtual mapping obtained by vmap()
 *
 *      @addr:          memory base address
 *
 *      Free the virtually contiguous memory area starting at @addr,
 *      which was created from the page array passed to vmap().
 *
 *      May not be called in interrupt context.
 */
void vunmap_bs(void *addr)
{
	BUG_ON_BS(in_interrupt());
	__vunmap_bs(addr, 0);
}
EXPORT_SYMBOL_GPL(vunmap_bs);

/**
 *      vmap  -  map an array of pages into virtually contiguous space
 *
 *      @pages:         array of page pointers
 *      @count:         number of pages to map
 *      @flags:         vm_area->flags
 *      @prot:          page protection for the mapping
 *
 *      Maps @count pages from @pages into contiguous kernel virtual
 *      space.
 */
void *vmap_bs(struct page_bs **pages, unsigned int count,
				unsigned long flags, pgprot_t_bs prot)
{
	struct vm_struct_bs *area;

	if (count > num_physpages_bs)
		return NULL;

	area = get_vm_area_bs((count << PAGE_SHIFT_BS), flags);
	if (!area)
		return NULL;

	if (map_vm_area_bs(area, prot, &pages)) {
		vunmap_bs(area->addr);
		return NULL;
	}

	return area->addr;
}
EXPORT_SYMBOL_GPL(vmap_bs);

/**             
 *      vfree  -  release memory allocated by vmalloc()
 *
 *      @addr:          memory base address
 *              
 *      Free the virtually contiguous memory area starting at @addr, as
 *      obtained from vmalloc(), vmalloc_32() or __vmalloc().
 *
 *      May not be called in interrupt context.
 */
void vfree_bs(void *addr)
{
	BUG_ON_BS(in_interrupt());
	__vunmap_bs(addr, 1);
}
EXPORT_SYMBOL_GPL(vfree_bs);

/**
 *      __vmalloc  -  allocate virtually contiguous memory
 *
 *      @size:          allocation size
 *      @gfp_mask:      flags for the page level allocator
 *      @prot:          protection mask for the allocated pages
 *
 *      Allocate enough pages to cover @size from the page level
 *      allocator with @gfp_mask flags.  Map them into contiguous
 *      kernel virtual space, using a pagetable protection of @prot.
 */
void *__vmalloc_bs(unsigned long size, unsigned int __nocast gfp_mask, 
							pgprot_t_bs prot)
{
	struct vm_struct_bs *area;

	size = PAGE_ALIGN_BS(size);
	if (!size || (size >> PAGE_SHIFT_BS) > num_physpages_bs)
		return NULL;

	area = get_vm_area_bs(size, VM_ALLOC_BS);

	return __vmalloc_area_bs(area, gfp_mask, prot);
}
EXPORT_SYMBOL_GPL(__vmalloc_bs);

/**     
 *      vmalloc  -  allocate virtually contiguous memory
 *              
 *      @size:          allocation size
 *      
 *      Allocate enough pages to cover @size from the page level
 *      allocator and map them into contiguous kernel virtual space.
 *
 *      For tight cotrol over page level allocator and protection flags
 *      use __vmalloc() instead.
 */ 
void *vmalloc_bs(unsigned long size)
{
	return __vmalloc_bs(size, GFP_KERNEL_BS | __GFP_HIGHMEM_BS,
					PAGE_KERNEL_BS);
}
EXPORT_SYMBOL_GPL(vmalloc_bs);

/**
 *      vmalloc_32  -  allocate virtually contiguous memory (32bit addressable)
 *
 *      @size:          allocation size
 *
 *      Allocate enough 32bit PA addressable pages to cover @size from the
 *      page level allocator and map them into contiguous kernel virtual space.
 */
void *vmalloc_32_bs(unsigned long size)
{
	return __vmalloc_bs(size, GFP_KERNEL_BS, PAGE_KERNEL_BS);
}
EXPORT_SYMBOL_GPL(vmalloc_32_bs);

#ifndef PAGE_KERNEL_EXEC_BS
#define PAGE_KERNEL_EXEC_BS	PAGE_KERNEL_BS
#endif
/**
 *      vmalloc_exec  -  allocate virtually contiguous, executable memory
 *
 *      @size:          allocation size
 *
 *      Kernel-internal function to allocate enough pages to cover @size
 *      the page level allocator and map them into contiguous and
 *      executable kernel virtual space.
 *
 *      For tight cotrol over page level allocator and protection flags
 *      use __vmalloc() instead.
 */
void *vmalloc_exec_bs(unsigned long size)
{
	return __vmalloc_bs(size, GFP_KERNEL_BS | __GFP_HIGHMEM_BS, 
					PAGE_KERNEL_EXEC_BS);
}

long vread_bs(char *buf, char *addr, unsigned long count)
{
	struct vm_struct_bs *tmp;
	char *vaddr, *buf_start = buf;
	unsigned long n;

	/* Don't allow overflow */
	if ((unsigned long)addr + count < count)
		count = -(unsigned long)addr;

	read_lock(&vmlist_lock_bs);
	for (tmp = vmlist_bs; tmp; tmp = tmp->next) {
		vaddr = (char *)tmp->addr;
		if (addr >= vaddr + tmp->size - PAGE_SIZE_BS)
			continue;
		while (addr < vaddr) {
			if (count == 0)
				goto finished;
			*buf = '\0';
			buf++;
			addr++;
			count--;
		}
		n = vaddr + tmp->size - PAGE_SIZE_BS - addr;
		do {
			if (count == 0)
				goto finished;
			*buf = *addr;
			buf++;
			addr++;
			count--;
		} while (--n > 0);
	}
finished:
	read_unlock(&vmlist_lock_bs);
	return buf - buf_start;
}

long vwrite_bs(char *buf, char *addr, unsigned long count)
{
	struct vm_struct_bs *tmp;
	char *vaddr, *buf_start = buf;
	unsigned long n;

	/* Don't allow overflow */
	if ((unsigned long)addr + count < count)
		count -= (unsigned long) addr;

	read_lock(&vmlist_lock_bs);
	for (tmp = vmlist_bs; tmp; tmp = tmp->next) {
		vaddr = (char *)tmp->addr;
		if (addr >= vaddr + tmp->size - PAGE_SIZE_BS)
			continue;
		while (addr < vaddr) {
			if (count == 0)
				goto finished;
			buf++;
			addr++;
			count--;
		}
		n = vaddr + tmp->size - PAGE_SIZE_BS - addr;
		do {
			if (count == 0)
				goto finished;
			*addr = *buf;
			buf++;
			addr++;
			count--;
		} while (--n > 0);
	}
finished:
	read_unlock(&vmlist_lock_bs);
	return buf - buf_start;
}
