/*
 * linux/mm/slab.c
 * Written by Mark Hemment, 1996/97.
 * (markhe@nextd.demon.co.uk)
 *
 * kmem_cache_destroy() + some cleanup - 1999 Andrea Arcangeli
 *
 * Major cleanup, different bufctl logic, per-cpu arrays
 *      (c) 2000 Manfred Spraul
 *
 * Cleanup, make the head arrays unconditional, preparation for NUMA
 *      (c) 2002 Manfred Spraul
 *
 * An implementation of the Slab Allocator as described in outline in;
 *      UNIX Internals: The New Frontiers by Uresh Vahalia
 *      Pub: Prentice Hall      ISBN 0-13-101908-2
 * or with a little more detail in;
 *      The Slab Allocator: An Object-Caching Kernel Memory Allocator
 *      Jeff Bonwick (Sun Microsystems).
 *      Presented at: USENIX Summer 1994 Technical Conference
 *
 * The memory is organized in caches, one cache for each object type.
 * (e.g. inode_cache, dentry_cache, buffer_head, vm_area_struct)
 * Each cache consists out of many slabs (they are small (usually one
 * page long) and always contiguous), and each slab contains multiple
 * initialized objects.
 *
 * This means, that your constructor is used only for newly allocated
 * slabs and you must pass objects with the same intializations to
 * kmem_cache_free.
 *
 * Each cache can only support one memory type (GFP_DMA, GFP_HIGHMEM,
 * normal). If you need a special memory type, then must create a new
 * cache for that memory type.
 *
 * In order to reduce fragmentation, the slabs are sorted in 3 groups:
 *   full slabs with 0 free objects
 *   partial slabs
 *   empty slabs with no allocated objects
 *
 * If partial slabs exist, then new allocations come from these slabs,
 * otherwise from empty slabs or new slabs are allocated.
 *
 * kmem_cache_destroy() CAN CRASH if you try to allocate from the cache
 * during kmem_cache_destroy(). The caller must prevent concurrent allocs.
 *
 * Each cache has a short per-cpu head array, most allocs
 * and frees go into that array, and if that array overflows, then 1/2
 * of the entries in the array are given back into the global cache.
 * The head array is strictly LIFO and should improve the cache hit rates.
 * On SMP, it additionally reduces the spinlock operations.
 *
 * The c_cpuarray may not be read with enabled local interrupts - 
 * it's changed with a smp_call_function().
 *
 * SMP synchronization:
 *  constructors and destructors are called without any locking.
 *  Several members in kmem_cache_t and struct slab never change, they
 *      are accessed without any locking.
 *  The per-cpu arrays are never accessed from the wrong cpu, no locking,
 *      and local interrupts are disabled so slab code is preempt-safe.
 *  The non-constant members are protected with a per-cache irq spinlock.
 *
 * Many thanks to Mark Hemment, who wrote another per-cpu slab patch
 * in 2000 - many ideas in the current implementation are derived from
 * his patch.
 *
 * Further notes from the original documentation:
 *
 * 11 April '97.  Started multi-threading - markhe
 *      The global cache-chain is protected by the semaphore 'cache_chain_sem'.
 *      The sem is only needed when accessing/extending the cache-chain, which
 *      can never happen inside an interrupt (kmem_cache_create(),
 *      kmem_cache_shrink() and kmem_cache_reap()).
 *
 *      At present, each engine can be growing a cache.  This should be blocked.
 *
 */
#include <linux/kernel.h>
#include <linux/spinlock.h>
#include <linux/string.h>
#include <linux/smp.h>
#include "biscuitos/kernel.h"
#include "biscuitos/slab.h"
#include "biscuitos/mm.h"
#include "asm-generated/arch.h"
#include "asm-generated/page.h"
#include "asm-generated/types.h"
#include "asm-generated/semaphore.h"

/*
 * DEBUG        - 1 for kmem_cache_create() to honour; SLAB_DEBUG_INITIAL,
 *                SLAB_RED_ZONE & SLAB_POISON.
 *                0 for faster, smaller code (especially in the critical paths).
 *
 * STATS        - 1 to collect stats for /proc/slabinfo.
 *                0 for faster, smaller code (especially in the critical paths).
 *
 * FORCED_DEBUG - 1 enables SLAB_RED_ZONE and SLAB_POISON (if possible)
 */

#ifdef CONFIG_DEBUG_SLAB_BS
#define DEBUG		1
#define STATS		1
#define FORCED_DEBUG	1
#else
#define DEBUG		0
#define STATS		0
#define FORCED_DEBUG	0
#endif

#ifndef cache_line_size_bs
#define cache_line_size_bs()	L1_CACHE_BYTES_BS
#endif

#ifndef ARCH_KMALLOC_MINALIGN_BS
/*
 * Enforce a minimum alignment for the kmalloc caches.
 * Usually, the kmalloc caches are cache_line_size() aligned, except when
 * DEBUG and FORCED_DEBUG are enabled, then they are BYTES_PER_WORD aligned.
 * Some archs want to perform DMA into kmalloc caches and need a guaranteed
 * alignment larger than BYTES_PER_WORD. ARCH_KMALLOC_MINALIGN allows that.
 * Note that this flag disables some debug features.
 */
#define ARCH_KMALLOC_MINALIGN_BS	0
#endif

#ifndef ARCH_SLAB_MINALIGN_BS
/*
 * Enforce a minimum alignment for all caches.
 * Intended for archs that get misalignment faults even for BYTES_PER_WORD
 * aligned buffers. Includes ARCH_KMALLOC_MINALIGN.
 * If possible: Do not enable this flag for CONFIG_DEBUG_SLAB, it disables
 * some debug features.
 */
#define ARCH_SLAB_MINALIGN_BS		0
#endif

#ifndef ARCH_KMALLOC_FLAGS_BS
#define ARCH_KMALLOC_FLAGS_BS		SLAB_HWCACHE_ALIGN_BS
#endif

/* Legal flag mask for kmem_cache_create(). */
#if DEBUG
#define CREATE_MASK_BS	(SLAB_DEBUG_INITIAL_BS | SLAB_RED_ZONE_BS |	\
			 SLAB_POISON_BS | SLAB_HWCACHE_ALIGN_BS |	\
			 SLAB_NO_REAP_BS | SLAB_CACHE_DMA_BS |		\
			 SLAB_MUST_HWCACHE_ALIGN_BS | 			\
			 SLAB_STORE_USER_BS | SLAB_RECLAIM_ACCOUNT_BS |	\
			 SLAB_PANIC_BS | SLAB_DESTROY_BY_RCU_BS)
#else
#define CREATE_MASK_BS	(SLAB_HWCACHE_ALIGN_BS | SLAB_NO_REAP_BS |	\
			 SLAB_CACHE_DMA_BS | 				\
			 SLAB_MUST_HWCACHE_ALIGN_BS | 			\
			 SLAB_RECLAIM_ACCOUNT_BS | SLAB_PANIC_BS |	\
			 SLAB_DESTROY_BY_RCU_BS)
#endif

/* Shouldn't this be in a header file somewhere? */
#define BYTES_PER_WORD_BS		sizeof(void *)

/*
 * kmem_bufctl_t:
 *
 * Bufctl's are used for linking objs within a slab
 * linked offsets.
 *
 * This implementation relies on "struct page" for locating the cache &
 * slab an object belongs to.
 * This allows the bufctl structure to be small (one int), but limits
 * the number of objects a slab (not a cache) can contain when off-slab
 * bufctls are used. The limit is the size of the largest general cache
 * that does not use off-slab slabs.
 * For 32bit archs with 4 kB pages, is this 56.
 * This is not serious, as it is only for large objects, when it is unwise
 * to have too many per slab.
 * Note: This limit can be raised by introducing a general cache whose size
 * is less than 512 (PAGE_SIZE<<3), but greater than 256.
 */

#define BUFCTL_END_BS		(((kmem_bufctl_t_bs)(~0UL))-0)
#define BUFCTL_FREE_BS		(((kmem_bufctl_t_bs)(~0UL))-1)
#define SLAB_LIMIT_BS		(((kmem_bufctl_t_bs)(~0UL))-2)

/*
 * struct slab
 *
 * Manages the objs in a slab. Placed either at the beginning of mem allocated
 * for a slab, or allocated from an general cache.
 * Slabs are chained into three list: fully used, partial, fully free slabs.
 */
struct slab_bs {
	struct list_head	list;
	unsigned long		colouroff;
	void			*s_mem;	/* including colour offset */
	unsigned int		inuse;	/* num of objs active in slab */
	kmem_bufctl_t_bs	free;
};

/*
 * struct array_cache
 *
 * Per cpu structures
 * Purpose:
 * - LIFO ordering, to hand out cache-warm objects from _alloc
 * - reduce the number of linked list operations
 * - reduce spinlock operations
 *
 * The limit is stored in the per-cpu structure to reduce the data cache
 * footprint.
 *
 */
struct array_cache_bs {
	unsigned int avail;
	unsigned int limit;
	unsigned int batchcount;
	unsigned int touched;
};

/* bootstrap: The caches do not work without cpuarrays anymore,
 * but the cpuarrays are allocated from the generic caches...
 */
#define BOOT_CPUCACHE_ENTRIES_BS	1
struct arraycache_init_bs {
	struct array_cache_bs cache;
	void *entries[BOOT_CPUCACHE_ENTRIES_BS];
};

/*
 * The slab lists of all objects.
 * Hopefully reduce the internal fragmentation
 * NUMA: The spinlock could be moved from the kmem_cache_t
 * into this structure, too. Figure out what causes
 * fewer cross-node spinlock operations.
 */
struct kmem_list3_bs {
	struct list_head	slabs_partial; /* partial list first, better asm code */
	struct list_head	slabs_full;
	struct list_head	slabs_free;
	unsigned long		free_objects;
	int			free_touched;
	unsigned long		next_reap;
	struct array_cache_bs	*shared;
};

#define LIST3_INIT_BS(parent)						\
{									\
	.slabs_full	= LIST_HEAD_INIT(parent.slabs_full),		\
	.slabs_partial	= LIST_HEAD_INIT(parent.slabs_partial),		\
	.slabs_free	= LIST_HEAD_INIT(parent.slabs_free)		\
}

#define list3_data_bs(cachep)						\
	(&(cachep)->lists)

/* Must match cache_sizes above. Out of line to keep cache footprint low. */
struct cache_names_bs {
	char *name;
	char *name_dma;
};

static struct cache_names_bs __initdata cache_names_bs[] = {
#define CACHE_BS(x)	{ .name = "size-" #x, .name_dma = "size-" #x "(DMA)"},
#include "biscuitos/kmalloc_sizes.h"
	{ NULL, }
#undef CACHE_BS
};

/*
 * kmem_cache_t
 *
 * manages a cache
 */
struct kmem_cache_s_bs {
/* 1) per-cpu data, touched during every alloc/free */
	struct array_cache_bs	*array[NR_CPUS_BS];
	unsigned int		batchcount;
	unsigned int		limit;
/* 2) touched by every alloc & free from the backend */
	struct kmem_list3_bs	lists;
	/* NUMA: kmem_3list_t	*nodelists[MAX_NUMNODES_BS] */
	unsigned int		objsize;
	unsigned int		flags;	/* constant flags */
	unsigned int		num;	/* # of objs per slab */
	unsigned int		free_limit; /* upper limit of objects in the lists */
	spinlock_t		spinlock;

/* 3) cache_grow/shrink */
	/* order of pgs per slab (2^n) */
	unsigned int		gfporder;

	/* force GFP flags, e.g. GFP_DMA */
	unsigned int		gfpflags;

	size_t			colour;		/* cache colouring range */
	unsigned int		colour_off;	/* colour offset */
	unsigned int		colour_next;	/* cache colouring */
	kmem_cache_t_bs		*slabp_cache;
	unsigned int		slab_size;
	unsigned int		dflags;		/* dynamic flags */

	/* constructor func */
	void (*ctor)(void *, kmem_cache_t_bs *, unsigned long);

	/* de-construct func */
	void (*dtor)(void *, kmem_cache_t_bs *, unsigned long);

/* 4) cache creation/removal */
	const char		*name;
	struct list_head	next;

/* 5) statistics */
#if STATS
	unsigned long		num_active;
	unsigned long		num_allocations;
	unsigned long		high_mark;
	unsigned long		grown;
	unsigned long		reaped;
	unsigned long		errors;
	unsigned long		max_freeable;
	unsigned long		node_allocs;
	atomic_t		allochit;
	atomic_t		allocmiss;
	atomic_t		freehit;
	atomic_t		freemiss;
#endif
#if DEBUG
	int			dbghead;
	int			reallen;
#endif
};

#define CFLGS_OFF_SLAB_BS	(0x80000000UL)
#define OFF_SLAB_BS(x)		((x)->flags & CFLGS_OFF_SLAB_BS)

#define BATCHREFILL_LIMIT_BS	16

/*
 * Maximum size of an obj (in 2^order pages)
 * and absolute limit for the gfp order.
 */
#if defined(CONFIG_LARGE_ALLOCS_BS)
#define MAX_OBJ_ORDER_BS	13	/* up to 32Mb */
#define MAX_GFP_ORDER_BS	13	/* up to 32Mb */
#elif defined(CONFIG_MMU)
#define MAX_OBJ_ORDER_BS	5	/* 32 pages */
#define MAX_GFP_ORDER_BS	5	/* 32 pages */
#else
#define MAX_OBJ_ORDER_BS	8	/* up to 1Mb */
#define MAX_GFP_ORDER_BS	8	/* up to 1Mb */
#endif

#if STATS
#define STATS_INC_ALLOCHIT_BS(x)	atomic_inc(&(x)->allochit)
#define STATS_INC_ALLOCMISS_BS(x)	atomic_inc(&(x)->allocmiss)
#define STATS_INC_ALLOCED_BS(x)		((x)->num_allocations++)
#define STATS_INC_ACTIVE_BS(x)		((x)->num_active++)
#define STATS_SET_HIGH_BS(x)		do {				\
	if ((x)->num_active > (x)->high_mark)				\
		(x)->high_mark = (x)->num_active;			\
} while (0)
#else
#define STATS_INC_ALLOCHIT_BS(x)	do { } while (0)
#define STATS_INC_ALLOCMISS_BS(x)	do { } while (0)
#define STATS_INC_ALLOCED_BS(x)		do { } while (0)
#define STATS_INC_ACTIVE_BS(x)		do { } while (0)
#define STATS_SET_HIGH_BS(x)		do { } while (0)
#endif

/*
 * Do not go above this order unless 0 objects fit into the slab.
 */
#define BREAK_GFP_ORDER_HI_BS	1
#define BREAK_GFP_ORDER_LO_BS	0
static int slab_break_gfp_order_bs = BREAK_GFP_ORDER_LO_BS;

static struct arraycache_init_bs initarray_cache_bs __initdata = {
	{ 0, BOOT_CPUCACHE_ENTRIES_BS, 1, 0} 
};
static struct arraycache_init_bs initarray_generic_bs = {
	{ 0, BOOT_CPUCACHE_ENTRIES_BS, 1, 0}
};

/* internal cache of cache description objs */
static kmem_cache_t_bs cache_cache_bs = {
	.lists		= LIST3_INIT_BS(cache_cache_bs.lists),
	.batchcount	= 1,
	.limit		= BOOT_CPUCACHE_ENTRIES_BS,
	.objsize	= sizeof(kmem_cache_t_bs),
	.flags		= SLAB_NO_REAP_BS,
	.spinlock	= __ARCH_SPIN_LOCK_UNLOCKED,
	.name		= "kmem_cache_bs",
#if DEBUG
	.reallen	= sizeof(kmem_cache_t_bs),
#endif
};

/* Guard access to the cache-chain. */
static struct semaphore_bs	cache_chain_sem_bs;
static struct list_head		cache_chain_bs;

/* These are the default caches for kmalloc. Custom caches can have
 * other size. */
struct cache_sizes_bs malloc_sizes_bs[] = {
#define CACHE_BS(x)	{ .cs_size = (x) },
#include "biscuitos/kmalloc_sizes.h"
	CACHE_BS(ULONG_MAX)
#undef CACHE_BS
};
EXPORT_SYMBOL_GPL(malloc_sizes_bs);

static void kmem_flagcheck_bs(kmem_cache_t_bs *cachep, unsigned int flags)
{
	if (flags & SLAB_DMA_BS) {
		if (!(cachep->gfpflags & GFP_DMA_BS))
			BUG_BS();
	} else {
		if (cachep->gfpflags & GFP_DMA_BS)
			BUG_BS();
	}
}

static inline void
cache_alloc_debugcheck_before_bs(kmem_cache_t_bs *cachep,
					unsigned int __nocast flags)
{
	might_sleep_if(flags & __GFP_WAIT_BS);
#if DEBUG
	kmem_flagcheck_bs(cachep, flags);
#endif
}

static inline struct array_cache_bs *ac_data_bs(kmem_cache_t_bs *cachep)
{
	return cachep->array[smp_processor_id()];
}

static inline void **ac_entry_bs(struct array_cache_bs *ac)
{
	return (void **)(ac+1);
}

static inline kmem_bufctl_t_bs *slab_bufctl_bs(struct slab_bs *slabp)
{
	return (kmem_bufctl_t_bs *)(slabp+1);
}

#if DEBUG
static void check_irq_off_bs(void)
{
	BUG_ON_BS(!irqs_disabled());
}

static void check_irq_on_bs(void)
{
	BUG_ON_BS(irqs_disabled());
}

static void check_spinlock_acquired_bs(kmem_cache_t_bs *cachep)
{
#ifndef CONFIG_SMP
	check_irq_off_bs();
	BUG_ON_BS(spin_trylock(&cachep->spinlock));
#endif
}

static void check_slabp_bs(kmem_cache_t_bs *cachep, struct slab_bs *slabp)
{
	kmem_bufctl_t_bs i;
	int entries = 0;

	check_spinlock_acquired_bs(cachep);
	/* Check slab's freelist to see if this obj is there. */
	for (i = slabp->free; i != BUFCTL_END_BS;
					i = slab_bufctl_bs(slabp)[i]) {
		entries++;
		if (entries > cachep->num || i >= cachep->num)
			goto bad;
	}
	if (entries != cachep->num - slabp->inuse) {
bad:
		printk(KERN_ERR "slab: Internal list corruption detected "
			"in cache '%s', slabp %p(%d). Hexdump:\n",
			cachep->name, cachep->num, slabp, slabp->inuse);
		for (i = 0; i < sizeof(slabp) + 
				cachep->num * sizeof(kmem_bufctl_t_bs); i++) {
			if ((i % 16) == 0)
				printk("\n%03x:", i);
			printk(" %02x", ((unsigned char *)slabp)[i]);
		}
		printk("\n");
		BUG_BS();
	}
}

#else
#define check_irq_off_bs()	do { } while (0)
#define check_irq_on_bs()	do { } while (0)
#define check_spinlock_acquired_bs(x)	do { } while (0)
#define check_slabp_bs(x, y)	do { } while (0)
#endif

/*
 * Interface to system's page allocator. No need to hold the cache-lock.
 *
 * If we requested dmaable memory, we will get it. Even if we
 * did not request dmaable memory, we might get it, but that
 * would be relatively rare and ignorable.
 */
static void *kmem_getpages_bs(kmem_cache_t_bs *cachep,
			unsigned int __nocast flags, int nodeid)
{
	struct page_bs *page;
	void *addr;
	int i;

	flags |= cachep->gfpflags;
	if (likely(nodeid == -1)) {
		page = alloc_pages_bs(flags, cachep->gfporder);
	}
}

/*
 * Grow (by 1) the number of slabs within a cache.  This is called by
 * kmem_cache_alloc() when there are no active objs left in a cache.
 */
static int cache_grow_bs(kmem_cache_t_bs *cachep,
			unsigned int __nocast flags, int nodeid)
{
	struct slab_bs	*slabp;
	void		*objp;
	size_t		offset;
	unsigned int	local_flags;
	unsigned long	ctor_flags;

	/* Be lazy and only check for valid flags here,
	 * keeping it out of the critical path in kmem_cache_alloc()
	 */
	if (flags & ~(SLAB_DMA_BS | SLAB_LEVEL_MASK_BS | SLAB_NO_GROW_BS))
		BUG_BS();
	if (flags & SLAB_NO_GROW_BS)
		return 0;

	ctor_flags = SLAB_CTOR_CONSTRUCTOR_BS;
	local_flags = (flags & SLAB_LEVEL_MASK_BS);
	if (!(local_flags & __GFP_WAIT_BS))
		/*
		 * Not allowed to sleep.  Need to tell a constructor about
		 * this - it might need to know...
		 */
		ctor_flags |= SLAB_CTOR_ATOMIC_BS;

	/* About to mess with non-constant members - lock. */
	check_irq_off_bs();
	spin_lock(&cachep->spinlock);

	/* Get colour for the slab, and cal the next value. */
	offset = cachep->colour_next;
	cachep->colour_next++;
	if (cachep->colour_next >= cachep->colour)
		cachep->colour_next = 0;
	offset *= cachep->colour_off;

	spin_unlock(&cachep->spinlock);

	if (local_flags & __GFP_WAIT_BS)
		local_irq_enable();

	/*
	 * The test for missing atomic flag is performed here, rather than
	 * the more obvious place, simply to reduce the critical path length
	 * in kmem_cache_alloc(). If a caller is seriously mis-behaving they
	 * will eventually be caught here (where it matters).
	 */
	kmem_flagcheck_bs(cachep, flags);

	/* Get mem for the objs. */
	if (!(objp = kmem_getpages_bs(cachep, flags, nodeid)))
		goto failed;

failed:
	;
}

static void *cache_alloc_refill_bs(kmem_cache_t_bs *cachep,
						unsigned int __nocast flags)
{
	int batchcount;
	struct kmem_list3_bs *l3;
	struct array_cache_bs *ac;

	check_irq_off_bs();
	ac = ac_data_bs(cachep);
retry:
	batchcount = ac->batchcount;
	if (!ac->touched && batchcount > BATCHREFILL_LIMIT_BS) {
		/* if there was little recent activity on this 
		 * cache, then perform only a partial refill.
		 * Otherwise we could generate refill bouncing.
		 */
		batchcount = BATCHREFILL_LIMIT_BS;
	}
	l3 = list3_data_bs(cachep);

	BUG_ON_BS(ac->avail > 0);
	spin_lock(&cachep->spinlock);
	if (l3->shared) {
		struct array_cache_bs *shared_array = l3->shared;

		if (shared_array->avail) {
			if (batchcount > shared_array->avail)
				batchcount = shared_array->avail;
			shared_array->avail -= batchcount;
			ac->avail = batchcount;
			memcpy(ac_entry_bs(ac),
		 	     &ac_entry_bs(shared_array)[shared_array->avail],
		 	     sizeof(void *)*batchcount);
			shared_array->touched = 1;
			goto alloc_done;
		}
	}
	while (batchcount > 0) {
		struct list_head *entry;
		struct slab_bs *slabp;

		/* Get slab alloc is to come from */
		entry = l3->slabs_partial.next;
		if (entry == &l3->slabs_partial) {
			l3->free_touched = 1;
			entry = l3->slabs_free.next;
			if (entry == &l3->slabs_free)
				goto must_grow;
		}

		slabp = list_entry(entry, struct slab_bs, list);
		check_slabp_bs(cachep, slabp);
		check_spinlock_acquired_bs(cachep);
		while (slabp->inuse < cachep->num && batchcount--) {
			kmem_bufctl_t_bs next;
			STATS_INC_ALLOCED_BS(cachep);
			STATS_INC_ACTIVE_BS(cachep);
			STATS_SET_HIGH_BS(cachep);

			/* get obj pointer */
			ac_entry_bs(ac)[ac->avail++] =
				slabp->s_mem + slabp->free * cachep->objsize;
			slabp->inuse++;
			next = slab_bufctl_bs(slabp)[slabp->free];
#if DEBUG
			slab_bufctl_bs(slabp)[slabp->free] = BUFCTL_FREE_BS;
#endif
			slabp->free = next;
		}
		check_slabp_bs(cachep, slabp);

		/* move slabp to correct slabp list; */
		list_del(&slabp->list);
		if (slabp->free == BUFCTL_END_BS)
			list_add(&slabp->list, &l3->slabs_full);
		else
			list_add(&slabp->list, &l3->slabs_partial);
	}

must_grow:
	l3->free_objects -= ac->avail;
alloc_done:
	spin_unlock(&cachep->spinlock);

	if (unlikely(!ac->avail)) {
		int x;

		x = cache_grow_bs(cachep, flags, -1);
	}
}

static inline void *__cache_alloc_bs(kmem_cache_t_bs *cachep,
					unsigned int __nocast flags)
{
	unsigned long save_flags;
	void *objp;
	struct array_cache_bs *ac;

	cache_alloc_debugcheck_before_bs(cachep, flags);

	local_irq_save(save_flags);
	ac = ac_data_bs(cachep);
	if (likely(ac->avail)) {
		STATS_INC_ALLOCHIT_BS(cachep);
		ac->touched = 1;
		objp = ac_entry_bs(ac)[--ac->avail];
	} else {
		STATS_INC_ALLOCMISS_BS(cachep);
		objp = cache_alloc_refill_bs(cachep, flags);
	}
	local_irq_restore(save_flags);
}

/**
 * kmem_cache_alloc - Allocate an object
 * @cachep: The cache to allocate from.
 * @flags: See kmalloc().
 *
 * Allocate an object from this cache.  The flags are only relevant
 * if the cache has no available objects.
 */
void *kmem_cache_alloc_bs(kmem_cache_t_bs *cachep,
				unsigned int __nocast flags)
{
	return __cache_alloc_bs(cachep, flags);
}
EXPORT_SYMBOL_GPL(kmem_cache_alloc_bs);

/**
 * kmem_cache_create - Create a cache.
 * @name: A string which is used in /proc/slabinfo to identify this cache.
 * @size: The size of objects to be created in this cache.
 * @align: The required alignment for the objects.
 * @flags: SLAB flags
 * @ctor: A constructor for the objects.
 * @dtor: A destructor for the objects.
 *
 * Returns a ptr to the cache on success, NULL on failure.
 * Cannot be called within a int, but can be interrupted.
 * The @ctor is run when new pages are allocated by the cache
 * and the @dtor is run before the pages are handed back.
 *
 * @name must be valid until the cache is destroyed. This implies that
 * the module calling this has to destroy the cache before getting 
 * unloaded.
 * 
 * The flags are
 *
 * %SLAB_POISON - Poison the slab with a known test pattern (a5a5a5a5)
 * to catch references to uninitialised memory.
 *
 * %SLAB_RED_ZONE - Insert `Red' zones around the allocated memory to check
 * for buffer overruns.
 *
 * %SLAB_NO_REAP - Don't automatically reap this cache when we're under
 * memory pressure.
 *
 * %SLAB_HWCACHE_ALIGN - Align the objects in this cache to a hardware
 * cacheline.  This can be beneficial if you're counting cycles as closely
 * as davem.
 */
kmem_cache_t_bs *
kmem_cache_create_bs(const char *name, size_t size, size_t align,
		unsigned long flags, 
		void (*ctor)(void *, kmem_cache_t_bs *, unsigned long),
		void (*dtor)(void *, kmem_cache_t_bs *, unsigned long))
{
	size_t left_over, slab_size, ralign;
	kmem_cache_t_bs *cachep = NULL;

	/*
	 * Sanity checks... these are all serious usage bugs.
	 */
	if (	(!name) ||
		in_interrupt() ||
		(size < BYTES_PER_WORD_BS) ||
		(size > (1 << MAX_OBJ_ORDER_BS)*PAGE_SIZE) ||
		(dtor && !ctor)) {
		printk(KERN_ERR "%s: Early error in slab %s\n",
				__FUNCTION__, name);
		BUG_BS();
	}

#if DEBUG
	WARN_ON_BS(strchr(name, ' '));	/* It confuse parsers */
	if ((flags & SLAB_DEBUG_INITIAL_BS) && !ctor) {
		/* No constructor, but inital state check requested */
		printk(KERN_ERR "%s: No con, but init state check "
				"requested - %s\n", __FUNCTION__, name);
		flags &= ~SLAB_DEBUG_INITIAL_BS;
	}

#if FORCED_DEBUG
	/*
	 * Enable redzoning and last user accounting, except for caches with
	 * large objects, if the increased size would increase the object size
	 * above the next power of two: caches with object sizes just above a
	 * power of two have a significant amount of internal fragmentation.
	 */
	if ((size < 4096 || fls(size-1) == fls(size-1+3*BYTES_PER_WORD_BS)))
		flags |= SLAB_RED_ZONE_BS|SLAB_STORE_USER_BS;
	if (!flags & SLAB_DESTROY_BY_RCU_BS)
		flags |= SLAB_POISON_BS;
#endif
	if (flags & SLAB_DESTROY_BY_RCU_BS)
		BUG_ON_BS(flags & SLAB_POISON_BS);
#endif
	if (flags & SLAB_DESTROY_BY_RCU_BS)
		BUG_ON_BS(dtor);

	/*
	 * Always checks flags, a caller might be expecting debug
	 * support which isn't available.
	 */
	if (flags & ~CREATE_MASK_BS)
		BUG_BS();

	/*
	 * Check that size is in terms of words.  This is needed to avoid
	 * unaligned accesses for some archs when redzoning is used, and
	 * makes sure any on-slab bufctl's are also correctly aligned.
	 */
	if (size & (BYTES_PER_WORD_BS-1)) {
		size += (BYTES_PER_WORD_BS-1);
		size &= ~(BYTES_PER_WORD_BS-1);
	}

	/* calculate out the final buffer alignment: */
	/* 1) arch recommendation: can be overridden for debug */
	if (flags & SLAB_HWCACHE_ALIGN_BS) {
		/* Default alignment: as specified by the arch code.
		 * Except if an object is really small, then squeeze multiple
		 * objects into one cacheline.
		 */
		ralign = cache_line_size_bs();
		while (size <= ralign/2)
			ralign /= 2;
	} else {
		ralign = BYTES_PER_WORD_BS;
	}

	/* 2) arch mandated alignment: disables debug if necessary */
	if (ralign < ARCH_SLAB_MINALIGN_BS) {
		ralign = ARCH_SLAB_MINALIGN_BS;
		if (ralign > BYTES_PER_WORD_BS)
			flags &= ~(SLAB_RED_ZONE_BS|SLAB_STORE_USER_BS);
	}

	/* 3) caller mandated alignment: disables debug if necessary */
	if (ralign < align) {
		ralign = align;
		if (ralign > BYTES_PER_WORD_BS)
			flags &= ~(SLAB_RED_ZONE_BS|SLAB_STORE_USER_BS);
	}

	/* 4) Store it. Note that the debug code below can reduce
	 *    the alignment to BYTES_PER_WORD_BS.
	 */
	align = ralign;

	/* Get cache's description obj. */
	cachep = (kmem_cache_t_bs *)kmem_cache_alloc_bs(&cache_cache_bs,
					SLAB_KERNEL_BS);
}

/* Cal the num objs, wastage, and bytes left over for a given slab size. */
static void cache_estimate_bs(unsigned long gfporder, size_t size,
	size_t align, int flags, size_t *left_over, unsigned int *num)
{
	int i;
	size_t wastage = PAGE_SIZE_BS << gfporder;
	size_t extra = 0;
	size_t base = 0;

	if (!(flags & CFLGS_OFF_SLAB_BS)) {
		base = sizeof(struct slab_bs);
		extra = sizeof(kmem_bufctl_t_bs);
	}
	i = 0;
	while (i*size + ALIGN(base+i*extra, align) <= wastage)
		i++;
	if (i > 0)
		i--;

	if (i > SLAB_LIMIT_BS)
		i = SLAB_LIMIT_BS;

	*num = i;
	wastage -= i*size;
	wastage -= ALIGN(base+i*extra, align);
	*left_over = wastage;
}

/*
 * Initialisation
 * Called after the gfp() functions have been enabled, and before smp_init().
 */
void __init kmem_cache_init_bs(void)
{
	size_t left_over;
	struct cache_sizes_bs *sizes;
	struct cache_names_bs *names;

	/*
	 * Fragmentation resistance on low memory - only use bigger
	 * page orders on machines with more than 32MB of memory.
	 */
	if (num_physpages_bs > (32 << 20) >> PAGE_SHIFT_BS)
		slab_break_gfp_order_bs = BREAK_GFP_ORDER_HI_BS;

	/* Bootstrap is tricky, because several objects are allocated
	 * from caches that do not exist yet:
	 * 1) initialize the cache_cache cache: it contains the kmem_cache_t
	 *    structures of all caches, except cache_cache itself: cache_cache
	 *    is statically allocated.
	 *    Initially an __init data area is used for the head array, it's
	 *    replaced with a kmalloc allocated array at the end of the 
	 *    bootstrap.
	 * 2) Create the first kmalloc cache.
	 *    The kmem_cache_t for the new cache is allocated normally. An 
	 *    __init data area is used for the head array.
	 * 3) Create the remaining kmalloc caches, with minimally sized 
	 *    head arrays.
	 * 4) Replace the __init data head arrays for cache_cache and the first
	 *    kmalloc cache with kmalloc allocated arrays.
	 * 5) Resize the head arrays of the kmalloc caches to their final sizes.
	 */

	/* 1) create the cache_cache */
	init_MUTEX_bs(&cache_chain_sem_bs);
	INIT_LIST_HEAD(&cache_chain_bs);
	list_add(&cache_cache_bs.next, &cache_chain_bs);
	cache_cache_bs.colour_off = cache_line_size_bs();
	cache_cache_bs.array[smp_processor_id()] = &initarray_cache_bs.cache;

	cache_cache_bs.objsize = ALIGN(cache_cache_bs.objsize, 
						cache_line_size_bs());
	cache_estimate_bs(0, cache_cache_bs.objsize, cache_line_size_bs(),
				0, &left_over, &cache_cache_bs.num);
	if (!cache_cache_bs.num)
		BUG_BS();

	cache_cache_bs.colour = left_over/cache_cache_bs.colour_off;
	cache_cache_bs.colour_next = 0;
	cache_cache_bs.slab_size = 
		ALIGN(cache_cache_bs.num * sizeof(kmem_bufctl_t_bs) + 
		sizeof(struct slab_bs), cache_line_size_bs());

	/* 2+3) Create the kmalloc caches */
	sizes = malloc_sizes_bs;
	names = cache_names_bs;

	while (sizes->cs_size != ULONG_MAX) {
		/* For performance, all the general caches are L1 aligned.
		 * This should be particularly beneficial on SMP boxes, as it
		 * eliminates "false sharing"
		 * Note for sytems short on memory removing the alignment will
		 * allow tighter packing of the smaller caches.
		 **/
		sizes->cs_cachep = kmem_cache_create_bs(
					names->name,
					sizes->cs_size,
					ARCH_KMALLOC_MINALIGN_BS,
					(ARCH_KMALLOC_FLAGS_BS | SLAB_PANIC_BS),
					NULL,
					NULL);
		sizes++;
		names++;
	}
}















