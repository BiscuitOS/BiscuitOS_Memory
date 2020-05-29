#ifdef CONFIG_SLOB_BS
/*
 * SLOB Allocator: Simple List Of Blocks
 *
 * Matt Mackall <mpm@selenic.com> 12/30/03
 *
 * How SLOB works:
 *
 * The core of SLOB is a traditional K&R style heap allocator, with
 * support for returning aligned objects. The granularity of this
 * allocator is 8 bytes on x86, though it's perhaps possible to reduce
 * this to 4 if it's deemed worth the effort. The slob heap is a
 * singly-linked list of pages from __get_free_page, grown on demand
 * and allocation from the heap is currently first-fit.
 *
 * Above this is an implementation of kmalloc/kfree. Blocks returned
 * from kmalloc are 8-byte aligned and prepended with a 8-byte header.
 * If kmalloc is asked for objects of PAGE_SIZE or larger, it calls
 * __get_free_pages directly so that it can return page-aligned blocks
 * and keeps a linked list of such pages and their orders. These
 * objects are detected in kfree() by their page alignment.
 *
 * SLAB is emulated on top of SLOB by simply calling constructors and
 * destructors for every SLAB allocation. Objects are returned with
 * the 8-byte alignment unless the SLAB_MUST_HWCACHE_ALIGN flag is
 * set, in which case the low-level allocator will fragment blocks to
 * create the proper alignment. Again, objects of page-size or greater
 * are allocated by calling __get_free_pages. As SLAB objects know
 * their size, no separate size bookkeeping is necessary and there is
 * essentially no allocation space overhead.
 */
#include <linux/kernel.h>
#include <linux/spinlock.h>
#include "biscuitos/types.h"
#include "biscuitos/mm.h"
#include "biscuitos/init.h"
#include "biscuitos/slab.h"
#include "biscuitos/percpu.h"
#include "asm-generated/page.h"

struct slob_block_bs {
	int units;
	struct slob_block_bs *next;
};
typedef struct slob_block_bs slob_t_bs;

struct kmem_cache_bs {
	unsigned int size, align;
	const char *name;
	void (*ctor)(void *, struct kmem_cache_bs *, unsigned long);
	void (*dtor)(void *, struct kmem_cache_bs *, unsigned long);
};

#define SLOB_UNIT_BS		sizeof(slob_t_bs)
#define SLOB_UNITS_BS(size)	(((size) + SLOB_UNIT_BS - 1)/SLOB_UNIT_BS)
#define SLOB_ALIGN_BS		L1_CACHE_BYTES_BS

struct bigblock_bs {
	int order;
	void *pages;
	struct bigblock_bs *next;
};
typedef struct bigblock_bs bigblock_t_bs;

static slob_t_bs arena_bs = { .next = &arena_bs, .units = 1 };
static slob_t_bs *slobfree_bs = &arena_bs;
static bigblock_t_bs *bigblocks_bs;
static DEFINE_SPINLOCK(slob_lock_bs);
static DEFINE_SPINLOCK(block_lock_bs);
atomic_t slab_reclaim_pages_bs = ATOMIC_INIT(0);
EXPORT_SYMBOL_GPL(slab_reclaim_pages_bs);

static void slob_free_bs(void *block, int size);

static int FASTCALL_BS(find_order_bs(int size));
static int fastcall_bs find_order_bs(int size)
{
	int order = 0;

	for ( ; size > 4096 ; size >>= 1)
		order++;
	return order;
}

static void *slob_alloc_bs(size_t size, gfp_t_bs gfp, int align)
{
	slob_t_bs *prev, *cur, *aligned = 0;
	int delta = 0, units = SLOB_UNITS_BS(size);
	unsigned long flags;

	spin_lock_irqsave(&slob_lock_bs, flags);
	prev = slobfree_bs;
	for (cur = prev->next; ; prev = cur, cur = cur->next) {
		if (align) {
			aligned = (slob_t_bs *)ALIGN((unsigned long)cur, align);
			delta = aligned - cur;
		}
		if (cur->units >= units + delta) { /* room enough? */
			if (delta) { /* need to fragment head to align? */
				aligned->units = cur->units - delta;
				aligned->next = cur->next;
				cur->next = aligned;
				cur->units = delta;
				prev = cur;
				cur = aligned;
			}

			if (cur->units == units) /* exact fit? */
				prev->next = cur->next; /* unlink */
			else { /* fragment */
				prev->next = cur + units;
				prev->next->units = cur->units - units;
				prev->next->next = cur->next;
				cur->units = units;
			}

			slobfree_bs = prev;
			spin_unlock_irqrestore(&slob_lock_bs, flags);
			return cur;
		}
		if (cur == slobfree_bs) {
			spin_unlock_irqrestore(&slob_lock_bs, flags);

			if (size == PAGE_SIZE_BS) /* trying to shrink arena? */
				return 0;

			cur = (slob_t_bs *)__get_free_page_bs(gfp);
			if (!cur)
				return 0;

			slob_free_bs(cur, PAGE_SIZE_BS);
			spin_lock_irqsave(&slob_lock_bs, flags);
			cur = slobfree_bs;
		}
	}
}

static void slob_free_bs(void *block, int size)
{
	slob_t_bs *cur, *b = (slob_t_bs *)block;
	unsigned long flags;

	if (!block)
		return;

	if (size)
		b->units = SLOB_UNITS_BS(size);

	/* Find reinsertion point */
	spin_lock_irqsave(&slob_lock_bs, flags);
	for (cur = slobfree_bs; !(b > cur && b < cur->next); cur = cur->next)
		if (cur >= cur->next && (b > cur || b < cur->next))
			break;

	if (b + b->units == cur->next) {
		b->units += cur->next->units;
		b->next = cur->next->next;
	} else
		b->next = cur->next;


	if (cur + cur->units == b) {
		cur->units += b->units;
		cur->next = b->next;
	} else
		cur->next = b;

	slobfree_bs = cur;

	spin_unlock_irqrestore(&slob_lock_bs, flags);
}

struct kmem_cache_bs *kmem_cache_create_bs(const char *name, size_t size,
	size_t align, unsigned long flags,
	void (*ctor)(void *, struct kmem_cache_bs *, unsigned long),
	void (*dtor)(void *, struct kmem_cache_bs *, unsigned long))
{
	struct kmem_cache_bs *c;

	c = slob_alloc_bs(sizeof(struct kmem_cache_bs), flags, 0);

	if (c) {
		c->name = name;
		c->size	= size;
		c->ctor = ctor;
		c->dtor	= dtor;
		/* ignore alignment unless it's forced */
		c->align = (flags & SLAB_MUST_HWCACHE_ALIGN_BS) ? 
					SLOB_ALIGN_BS : 0;
		if (c->align < align)
			c->align = align;
	}

	return c;
}
EXPORT_SYMBOL_GPL(kmem_cache_create_bs);

int kmem_cache_destroy_bs(struct kmem_cache_bs *c)
{
	slob_free_bs(c, sizeof(struct kmem_cache_bs));
	return 0;
}

void *kmem_cache_alloc_bs(struct kmem_cache_bs *c, gfp_t_bs flags)
{
	void *b;

	if (c->size < PAGE_SIZE_BS)
		b = slob_alloc_bs(c->size, flags, c->align);
	else
		b = (void *)__get_free_pages_bs(flags, find_order_bs(c->size));

	if (c->ctor)
		c->ctor(b, c, SLAB_CTOR_CONSTRUCTOR_BS);

	return b;
}
EXPORT_SYMBOL_GPL(kmem_cache_alloc_bs);

void kmem_cache_free_bs(struct kmem_cache_bs *c, void *b)
{
	if (c->dtor)
		c->dtor(b, c, 0);

	if (c->size < PAGE_SIZE_BS)
		slob_free_bs(b, c->size);
	else
		free_pages_bs((unsigned long)b, find_order_bs(c->size));
}
EXPORT_SYMBOL_GPL(kmem_cache_free_bs);

void *kmalloc_bs(size_t size, gfp_t_bs gfp)
{
	slob_t_bs *m;
	bigblock_t_bs *bb;
	unsigned long flags;

	if (size < PAGE_SIZE_BS - SLOB_UNIT_BS) {
		m = slob_alloc_bs(size + SLOB_UNIT_BS, gfp, 0);
		return m ? (void *)(m + 1) : 0;
	}

	bb = slob_alloc_bs(sizeof(bigblock_t_bs), gfp, 0);
	if (!bb)
		return 0;

	bb->order = find_order_bs(size);
	bb->pages = (void *)__get_free_pages_bs(gfp, bb->order);

	if (bb->pages) {
		spin_lock_irqsave(&block_lock_bs, flags);
		bb->next = bigblocks_bs;
		bigblocks_bs = bb;
		spin_unlock_irqrestore(&block_lock_bs, flags);
		return bb->pages;
	}

	slob_free_bs(bb, sizeof(bigblock_t_bs));
	return 0;
}
EXPORT_SYMBOL_GPL(kmalloc_bs);

void kfree_bs(const void *block)
{
	bigblock_t_bs *bb, **last = &bigblocks_bs;
	unsigned long flags;

	if (!block)
		return;

	if (!((unsigned long)block & (PAGE_SIZE_BS - 1))) {
		/* might be on the big block list */
		spin_lock_irqsave(&block_lock_bs, flags);
		for (bb = bigblocks_bs; bb; last = &bb->next, bb = bb->next) {
			if (bb->pages == block) {
				*last = bb->next;
				spin_unlock_irqrestore(&block_lock_bs, flags);
				free_pages_bs((unsigned long)block, bb->order);
				slob_free_bs(bb, sizeof(bigblock_t_bs));
				return;
			}
		}
		spin_unlock_irqrestore(&block_lock_bs, flags);
	}

	slob_free_bs((slob_t_bs *)block - 1, 0);
	return;
}
EXPORT_SYMBOL_GPL(kfree_bs);

unsigned int ksize_bs(const void *block)
{
	bigblock_t_bs *bb;
	unsigned long flags;

	if (!block)
		return 0;

	if (!((unsigned long)block & (PAGE_SIZE_BS - 1))) {
		spin_lock_irqsave(&block_lock_bs, flags);
		for (bb = bigblocks_bs; bb; bb = bb->next)
			if (bb->pages == block) {
				spin_unlock_irqrestore(&slob_lock_bs, flags);
				return PAGE_SIZE_BS << bb->order;
			}
		spin_unlock_irqrestore(&block_lock_bs, flags);
	}
	return ((slob_t_bs *)block - 1)->units * SLOB_UNIT_BS;
}

unsigned int kmem_cache_size_bs(struct kmem_cache_bs *c)
{
	return c->size;
}
EXPORT_SYMBOL_GPL(kmem_cache_size_bs);

const char *kmem_cache_name_bs(struct kmem_cache_bs *c)
{
	return c->name;
}
EXPORT_SYMBOL_GPL(kmem_cache_name_bs);

/* FIXME: BiscuitOS slob debug stuf */
DEBUG_FUNC_T(slob);

void kmem_cache_init_bs(void)
{
	void *p = slob_alloc_bs(PAGE_SIZE_BS, 0, PAGE_SIZE_BS - 1);

	if (p)
		free_page_bs((unsigned long)p);

	/* FIXME: slab_initcall entry, used to debug slab,
	 * This code isn't default code */
	DEBUG_CALL(slob);
}

#ifdef CONFIG_SMP

void *__alloc_percpu_bs(size_t size)
{
	int i;
	struct percpu_data_bs *pdata = 
			kmalloc_bs(sizeof(*pdata), GFP_KERNEL_BS);

	if (!pdata)
		return NULL;

	for (i = 0; i < NR_CPUS_BS; i++) {
		if (!cpu_possible(i))
			continue;
		pdata->ptrs[i] = kmalloc_bs(size, GFP_KERNEL_BS);
		if (!pdata->ptrs[i])
			goto unwind_oom;
		memset(pdata->ptrs[i], 0, size);
	}

	/* Catch derefs w/o wrappers */
	return (void *)(~(unsigned long)pdata);

unwind_oom:
	while (--i >= 0) {
		if (!cpu_possible(i))
			continue;
		kfree_bs(pdata->ptrs[i]);
	}
	kfree_bs(pdata);
	return NULL;
}
EXPORT_SYMBOL_GPL(__alloc_percpu_bs);

void free_percpu_bs(const void *objp)
{
	int i;
	struct percpu_data_bs *p = 
			(struct percpu_data_bs *)(~(unsigned long) objp);

	for (i = 0; i < NR_CPUS_BS; i++) {
		if (!cpu_possible(i))
			continue;
		kfree_bs(p->ptrs[i]);
	}
	kfree_bs(p);
}
EXPORT_SYMBOL_GPL(free_percpu_bs);

#endif

#endif
