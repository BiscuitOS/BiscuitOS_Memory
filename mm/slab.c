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
#include <linux/cpumask.h>
#include <linux/rculist.h>
#include <linux/jiffies.h>
#include <linux/semaphore.h>
#include <linux/seq_file.h>
#include <linux/notifier.h>
#include <linux/workqueue.h>
#include <asm/uaccess.h>
#include "biscuitos/kernel.h"
#include "asm-generated/page.h"
#include "biscuitos/slab.h"
#include "biscuitos/mm.h"
#include "biscuitos/init.h"
#include "biscuitos/cpu.h"
#include "asm-generated/arch.h"
#include "asm-generated/types.h"
#include "asm-generated/semaphore.h"
#include "asm-generated/cputype.h"
#include "biscuitos/percpu.h"
#include "asm-generated/arch.h"
#include "asm-generated/percpu.h"

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

/* Max number of objs-per-slab for caches which use off-slab slabs.
 * Needed to avoid a possible looping condition in cache_grow().
 */
static unsigned long offslab_limit_bs;

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
 * struct slab_rcu
 *
 * slab_destroy on a SLAB_DESTROY_BY_RCU cache uses this structure to
 * arrange for kmem_freepages to be called via RCU.  This is useful if
 * we need to approach a kernel structure obliquely, from its address
 * obtained without the usual locking.  We can lock the structure to
 * stabilize it and check it's still at the given address, only if we
 * can be sure that the memory has not been meanwhile reused for some
 * other kind of object (which our subsystem's lock might corrupt).
 *
 * rcu_read_lock before reading the address, then rcu_read_unlock after
 * taking the spinlock within the structure expected at that address.
 *
 * We assume struct slab_rcu can overlay struct slab when destroying.
 */
struct slab_rcu_bs {
	struct rcu_head		head;
	kmem_cache_t_bs		*cachep;
	void			*addr;
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

/* NUMA: per-node */
#define list3_data_ptr_bs(cachep, ptr)					\
		list3_data_bs(cachep)

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

/* Optimization question: fewer reaps means less 
 * probability for unnessary cpucache drain/refill cycles.
 *
 * OTHO the cpuarrays can contain lots of objects,
 * which could lock up otherwise freeable slabs.
 */
#define REAPTIMEOUT_CPUC_BS	(2*HZ)
#define REAPTIMEOUT_LIST3_BS	(4*HZ)

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
#define STATS_SET_FREEABLE_BS(x, i)					\
	do {								\
		if ((x)->max_freeable < i)				\
			(x)->max_freeable = i;				\
	} while (0)

#define STATS_INC_ALLOCHIT_BS(x)	atomic_inc(&(x)->allochit)
#define STATS_INC_ALLOCMISS_BS(x)	atomic_inc(&(x)->allocmiss)
#define STATS_INC_ALLOCED_BS(x)		((x)->num_allocations++)
#define STATS_INC_FREEHIT_BS(x)		atomic_inc(&(x)->freehit)
#define STATS_INC_FREEMISS_BS(x)	atomic_inc(&(x)->freemiss)
#define STATS_INC_GROWN_BS(x)		((x)->grown++)
#define STATS_INC_ACTIVE_BS(x)		((x)->num_active++)
#define STATS_INC_REAPED_BS(x)		((x)->reaped++)
#define STATS_DEC_ACTIVE_BS(x)		((x)->num_active--)
#define STATS_SET_HIGH_BS(x)		do {				\
	if ((x)->num_active > (x)->high_mark)				\
		(x)->high_mark = (x)->num_active;			\
} while (0)
#else
#define STATS_INC_ALLOCHIT_BS(x)	do { } while (0)
#define STATS_INC_ALLOCMISS_BS(x)	do { } while (0)
#define STATS_INC_ALLOCED_BS(x)		do { } while (0)
#define STATS_INC_ACTIVE_BS(x)		do { } while (0)
#define STATS_DEC_ACTIVE_BS(x)		do { } while (0)
#define STATS_SET_HIGH_BS(x)		do { } while (0)
#define STATS_INC_REAPED_BS(x)		do { } while (0)
#define STATS_INC_GROWN_BS(x)		do { } while (0)
#define STATS_INC_FREEHIT_BS(x)		do { } while (0)
#define STATS_INC_FREEMISS_BS(x)	do { } while (0)
#define STATS_SET_FREEABLE_BS(x, i)	do { } while (0)
#endif

/*
 * Do not go above this order unless 0 objects fit into the slab.
 */
#define BREAK_GFP_ORDER_HI_BS	1
#define BREAK_GFP_ORDER_LO_BS	0
static int slab_break_gfp_order_bs = BREAK_GFP_ORDER_LO_BS;

/* Macros for storing/retrieving the cachep and or slab from the
 * global 'mem_map'. These are used to find the slab an obj belongs to.
 * With kfree(), these are used to find the cache which an obj belongs to.
 */
#define SET_PAGE_CACHE_BS(pg,x)	((pg)->lru.next = (struct list_head *)(x))
#define GET_PAGE_CACHE_BS(pg)	((kmem_cache_t_bs *)(pg)->lru.next)
#define SET_PAGE_SLAB_BS(pg,x)	((pg)->lru.prev = (struct list_head *)(x))
#define GET_PAGE_SLAB_BS(pg)	((struct slab_bs *)(pg)->lru.prev)

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
	.name		= "kmem_cache_bs",
#if DEBUG
	.reallen	= sizeof(kmem_cache_t_bs),
#endif
};

/* Guard access to the cache-chain. */
static struct semaphore		cache_chain_sem_bs;
static struct list_head		cache_chain_bs;

/*
 * vm_enough_memory() looks at this to determine how many
 * slab-allocated pages are possibly freeable under pressure
 *
 * SLAB_RECLAIM_ACCOUNT turns this on per-slab
 */
atomic_t slab_reclaim_pages_bs;
EXPORT_SYMBOL_GPL(slab_reclaim_pages_bs);

/*
 * chicken and egg problem: delay the per-cpu array allocation
 * until the general caches are up.
 */
static enum {
	NONE_BS,
	PARTIAL_BS,
	FULL_BS
} g_cpucache_up_bs;

static DEFINE_PER_CPU_BS(struct work_struct, reap_work_bs);

struct ccupdate_struct_bs {
	kmem_cache_t_bs *cachep;
	struct array_cache_bs *new[NR_CPUS_BS];
};

/* These are the default caches for kmalloc. Custom caches can have
 * other size. */
struct cache_sizes_bs malloc_sizes_bs[] = {
#define CACHE_BS(x)	{ .cs_size = (x) },
#include "biscuitos/kmalloc_sizes.h"
	CACHE_BS(ULONG_MAX)
#undef CACHE_BS
};
EXPORT_SYMBOL_GPL(malloc_sizes_bs);

static void cache_estimate_bs(unsigned long gfporder, size_t size,
	size_t align, int flags, size_t *left_over, unsigned int *num);

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
/* Magic nums for obj red zoning.
 * Placed in the first word before and the first word after an obj.
 */
#define RED_INACTIVE_BS		0x5A2CF071UL	/* when obj is inactive */
#define RED_ACTIVE_BS		0x170FC2A5UL	/* when obj is active */

/* ...and for poisoning */
#define POISON_INUSE_BS		0x5a	/* for use-uninitialised poisoning */
#define POISON_FREE_BS		0x6b	/* for use-after-free poisoning */
#define POISON_END_BS		0xa5	/* end-byte of poisoning */

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
		printk(KERN_INFO "slab: Internal list corruption detected "
			"in cache '%s'(%d), slabp %p(%d). Hexdump:\n",
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

static int obj_reallen_bs(kmem_cache_t_bs *cachep)
{
	return cachep->reallen;
}

static int obj_dbghead_bs(kmem_cache_t_bs *cachep)
{
	return cachep->dbghead;
}

static void **dbg_userword_bs(kmem_cache_t_bs *cachep, void *objp)
{
	BUG_ON(!(cachep->flags & SLAB_STORE_USER_BS));
	return (void **)(objp+cachep->objsize-BYTES_PER_WORD_BS);
}

static unsigned long *dbg_redzone1_bs(kmem_cache_t_bs *cachep, void *objp)
{
	BUG_ON(!(cachep->flags & SLAB_RED_ZONE_BS));
	return (unsigned long *)(objp+obj_dbghead_bs(cachep)-BYTES_PER_WORD_BS);
}

static unsigned long *dbg_redzone2_bs(kmem_cache_t_bs *cachep, void *objp)
{
	BUG_ON(!(cachep->flags & SLAB_RED_ZONE_BS));
	if (cachep->flags & SLAB_STORE_USER_BS)
		return (unsigned long *)(objp+cachep->objsize - 
							2*BYTES_PER_WORD_BS);
	return (unsigned long *)(objp+cachep->objsize-BYTES_PER_WORD_BS);
}

#else
#define dbg_redzone1_bs(cachep, objp)	({BUG_BS(); (unsigned long *)NULL;})
#define dbg_redzone2_bs(cachep, objp)	({BUG_BS(); (unsigned long *)NULL;})
#define dbg_userword_bs(cachep, objp)	({BUG_BS(); (void **)NULL;})
#define obj_dbghead_bs(x)		0
#define obj_reallen_bs(cachep)		(cachep->objsize)
#define check_irq_off_bs()		do { } while (0)
#define check_irq_on_bs()		do { } while (0)
#define check_spinlock_acquired_bs(x)	do { } while (0)
#define check_slabp_bs(x, y)		do { } while (0)
#define kfree_debugcheck_bs(x)		do { } while (0)
#endif

#define slab_error_bs(cachep, msg) __slab_error_bs(__FUNCTION__, cachep, msg)

static void __used __slab_error_bs(const char *function, 
				kmem_cache_t_bs *cachep, char *msg)
{
	printk(KERN_INFO "slab error in %s(): cahce '%s': %s\n",
			function, cachep->name, msg);
	dump_stack();
}

static void __used poison_obj_bs(kmem_cache_t_bs *cachep, void *addr, 
						unsigned char val)
{
	int size = obj_reallen_bs(cachep);

	addr = &((char *)addr)[obj_dbghead_bs(cachep)];

	memset(addr, val, size);
	*(unsigned char *)(addr+size-1) = POISON_END;
}

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
	} else {
		page = alloc_pages_node_bs(nodeid, flags, cachep->gfporder);
	}
	if (!page)
		return NULL;
	addr = page_address_bs(page);

	i = (1 << cachep->gfporder);
	if (cachep->flags & SLAB_RECLAIM_ACCOUNT_BS)
		atomic_add(i, &slab_reclaim_pages_bs);
	add_page_state_bs(nr_slab, i);
	while (i--) {
		SetPageSlab_bs(page);
		page++;
	}
	return addr;
}

/* Get the memory for a slab management obj. */
static struct slab_bs *alloc_slabmgmt_bs(kmem_cache_t_bs *cachep,
		void *objp, int colour_off, unsigned int __nocast local_flags)
{
	struct slab_bs *slabp;

	if (OFF_SLAB_BS(cachep)) {
		/* Slab management obj is off-slab. */
		slabp = kmem_cache_alloc_bs(cachep->slabp_cache, local_flags);
		if (!slabp)
			return NULL;
	} else {
		slabp = objp + colour_off;
		colour_off += cachep->slab_size;
	}
	slabp->inuse = 0;
	slabp->colouroff = colour_off;
	slabp->s_mem = objp+colour_off;

	return slabp;
}

static void set_slab_attr_bs(kmem_cache_t_bs *cachep,
					struct slab_bs *slabp, void *objp)
{
	int i;
	struct page_bs *page;

	/* Nasty!!!!!! I hope this is OK */
	i = 1 << cachep->gfporder;
	page = virt_to_page_bs(objp);
	do {
		SET_PAGE_CACHE_BS(page, cachep);
		SET_PAGE_SLAB_BS(page, slabp);
		page++;
	} while (--i);
}

static void cache_init_objs_bs(kmem_cache_t_bs *cachep,
		struct slab_bs *slabp, unsigned long ctor_flags)
{
	int i;

	for (i = 0; i < cachep->num; i++) {
		void *objp = slabp->s_mem+cachep->objsize*i;
#if DEBUG
		/* need to poison the objs? */
		if (cachep->flags & SLAB_POISON_BS)
			poison_obj_bs(cachep, objp, POISON_FREE_BS);
		if (cachep->flags & SLAB_STORE_USER_BS)
			*dbg_userword_bs(cachep, objp) = NULL;
		if (cachep->flags & SLAB_RED_ZONE_BS) {
			*dbg_redzone1_bs(cachep, objp) = RED_INACTIVE_BS;
			*dbg_redzone2_bs(cachep, objp) = RED_INACTIVE_BS;
		}
		/*
		 * Constructors are not allowed to allocate memory from
		 * the same cache which they are a constructor for.
		 * Otherwise, deadlock. They must also be threaded.
		 */
		if (cachep->ctor && !(cachep->flags & SLAB_POISON_BS))
			cachep->ctor(objp+obj_dbghead_bs(cachep), 
							cachep, ctor_flags);

		if (cachep->flags & SLAB_RED_ZONE_BS) {
			if (*dbg_redzone2_bs(cachep, objp) != RED_INACTIVE_BS)
				slab_error_bs(cachep, "constructor overwrote "
					"the end of an object");

			if (*dbg_redzone1_bs(cachep, objp) != RED_INACTIVE_BS)
				slab_error_bs(cachep, "constructor overwrote "
					"the start of an object");
		}
		if ((cachep->objsize % PAGE_SIZE_BS) == 0 &&
			OFF_SLAB_BS(cachep) && cachep->flags & SLAB_POISON_BS)
			kernel_map_pages_bs(virt_to_page_bs(objp),
					cachep->objsize/PAGE_SIZE_BS, 0);

#else
		if (cachep->ctor)
			cachep->ctor(objp, cachep, ctor_flags);
#endif
		slab_bufctl_bs(slabp)[i] = i+1;
	}
	slab_bufctl_bs(slabp)[i-1] = BUFCTL_END_BS;
	slabp->free = 0;
}

/*
 * Interface to system's page release.
 */
static void kmem_freepages_bs(kmem_cache_t_bs *cachep, void *addr)
{
	unsigned long i = (1 << cachep->gfporder);
	struct page_bs *page = virt_to_page_bs(addr);
	const unsigned long nr_freed = i;

	while (i--) {
		if (!TestClearPageSlab_bs(page))
			BUG_BS();
		page++;
	}
	sub_page_state_bs(nr_slab, nr_freed);
#if 0
	if (current->reclaim_state)
		current->reclaim_state->reclaimed_slab += nr_freed;
#endif
	free_pages_bs((unsigned long)addr, cachep->gfporder);
	if (cachep->flags & SLAB_RECLAIM_ACCOUNT_BS)
		atomic_sub(1 << cachep->gfporder, &slab_reclaim_pages_bs);
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

	/* Get slab management. */
	if (!(slabp = alloc_slabmgmt_bs(cachep, objp, offset, local_flags)))
		goto opps1;

	set_slab_attr_bs(cachep, slabp, objp);

	cache_init_objs_bs(cachep, slabp, ctor_flags);

	if (local_flags & __GFP_WAIT_BS)
		local_irq_disable();
	check_irq_off_bs();
	spin_lock(&cachep->spinlock);

	/* Make slab active */
	list_add_tail(&slabp->list, &(list3_data_bs(cachep)->slabs_free));
	STATS_INC_GROWN_BS(cachep);
	list3_data_bs(cachep)->free_objects += cachep->num;
	spin_unlock(&cachep->spinlock);
	return 1;
opps1:
	kmem_freepages_bs(cachep, objp);
failed:
	if (local_flags & __GFP_WAIT_BS)
		local_irq_disable();
	return 0;
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

		// cache_grow can reenable interrupts, then ac could change.
		ac = ac_data_bs(cachep);
		if (!x && ac->avail == 0)	// no objects in sight? abort
			return NULL;

		if (!ac->avail)	// objects refilled by interrupt?
			goto retry;
	}
	ac->touched = 1;
	return ac_entry_bs(ac)[--ac->avail];
}

#if DEBUG

static void dump_line_bs(char *data, int offset, int limit)
{
	int i;

	printk(KERN_INFO "%03x:", offset);
	for (i=0;i<limit;i++) {
		printk(" %02x", (unsigned char)data[offset+i]);
	}
	printk("\n");
}

static void print_objinfo_bs(kmem_cache_t_bs *cachep, void *objp, int lines)
{
	int i, size;
	char *realobj;

	if (cachep->flags & SLAB_RED_ZONE_BS) {
		printk(KERN_INFO "Redzone: 0x%lx/0x%lx.\n",
					*dbg_redzone1_bs(cachep, objp),
					*dbg_redzone2_bs(cachep, objp));
	}

	if (cachep->flags & SLAB_STORE_USER_BS) {
		printk(KERN_INFO "Last user: [<%p>]",
					*dbg_userword_bs(cachep, objp));
#if 0
		print_symbol("(%s)",
			(unsigned long)*dbg_userword_bs(cachep, objp));
#endif
		printk("\n");
	}
	realobj = (char*)objp+obj_dbghead_bs(cachep);
	size = obj_reallen_bs(cachep);
	for (i=0; i<size && lines;i+=16, lines--) {
		int limit;

		limit = 16;
		if (i+limit > size)
			limit = size-i;
		dump_line_bs(realobj, i, limit);
	}
}

static void check_poison_obj_bs(kmem_cache_t_bs *cachep, void *objp)
{
	char *realobj;
	int size, i;
	int lines = 0;

	realobj = (char*)objp+obj_dbghead_bs(cachep);
	size = obj_reallen_bs(cachep);

	for (i=0;i<size;i++) {
		char exp = POISON_FREE_BS;

		if (i == size-1)
			exp = POISON_END_BS;
		if (realobj[i] != exp) {
			int limit;
			/* Mismatch ! */
			/* Print header */
			if (lines == 0) {
				printk(KERN_INFO "Slab corruption: "
					"start=%p, len=%d\n", realobj, size);
				print_objinfo_bs(cachep, objp, 0);
			}
			/* Hexdump the affected line */
			i = (i/16)*16;
			limit = 16;
			if (i+limit > size)
				limit = size-i;
			dump_line_bs(realobj, i, limit);
			i += 16;
			lines++;
			/* Limit to 5 lines */
			if (lines > 5)
				break;
		}
	}
	if (lines != 0) {
		/* Print some data about the neighboring objects, if they
		 * exist:
		 */
		struct slab_bs *slabp = GET_PAGE_SLAB_BS(virt_to_page_bs(objp));
		int objnr;

		objnr = (objp-slabp->s_mem)/cachep->objsize;
		if (objnr) {
			objp = slabp->s_mem+(objnr-1)*cachep->objsize;
			realobj = (char*)objp+obj_dbghead_bs(cachep);
			printk(KERN_INFO "Prev obj: start=%p, len=%d\n",
								realobj, size);
			print_objinfo_bs(cachep, objp, 2);
		}
		if (objnr+1 < cachep->num) {
			objp = slabp->s_mem+(objnr+1)*cachep->objsize;
			realobj = (char*)objp+obj_dbghead_bs(cachep);
			printk(KERN_INFO "Next obj: start=%p, len=%d\n",
								realobj, size);
			print_objinfo_bs(cachep, objp, 2);
		}
	}
}

static void *
cache_alloc_debugcheck_after_bs(kmem_cache_t_bs *cachep,
		unsigned long flags, void *objp, void *caller)
{
	if (!objp)
		return objp;
	if (cachep->flags & SLAB_POISON_BS) {
#ifdef CONFIG_DEBUG_PAGEALLOC
		if ((cachep->objsize % PAGE_SIZE_BS) == 0 &&
							OFF_SLAB_BS(cachep))
			kernel_map_pages_bs(virt_to_page_bs(objp),
					cachep->objsize/PAGE_SIZE_BS, 1);
		else
			check_poison_obj_bs(cachep, objp);
#else
		check_poison_obj_bs(cachep, objp);
#endif
	}
	if (cachep->flags & SLAB_STORE_USER_BS)
		*dbg_userword_bs(cachep, objp) = caller;

	if (cachep->flags & SLAB_RED_ZONE_BS) {
		if (*dbg_redzone1_bs(cachep, objp) != RED_INACTIVE_BS ||
			*dbg_redzone2_bs(cachep, objp) != RED_INACTIVE_BS) {
			slab_error_bs(cachep, "double free, or memory outside"
					" object was overwritten");
			printk(KERN_INFO "%p: redzone 1: 0x%lx, redzone 2: "
				"0x%lx\n", objp, 
				*dbg_redzone1_bs(cachep, objp),
				*dbg_redzone2_bs(cachep, objp));
		}
		*dbg_redzone1_bs(cachep, objp) = RED_ACTIVE_BS;
		*dbg_redzone2_bs(cachep, objp) = RED_ACTIVE_BS;
	}
	objp += obj_dbghead_bs(cachep);
	if (cachep->ctor && cachep->flags & SLAB_POISON_BS) {
		unsigned long ctor_flags = SLAB_CTOR_CONSTRUCTOR_BS;

		if (!(flags & __GFP_WAIT_BS))
			ctor_flags |= SLAB_CTOR_ATOMIC_BS;

		cachep->ctor(objp, cachep, ctor_flags);
	}
	return objp;
}

/*
 * Perform extra freeing checks:
 * - detect bad pointers.
 * - POISON/RED_ZONE checking
 * - destructor calls, for caches with POISON+dtor
 */
static void kfree_debugcheck_bs(const void *objp)
{
	struct page_bs *page;

	if (!virt_addr_valid_bs(objp)) {
		printk(KERN_INFO "kfree_debugcheck: out of range ptr %lxh.\n",
				(unsigned long)objp);
		BUG_BS();
	}
	page = virt_to_page_bs(objp);
	if (!PageSlab_bs(page)) {
		printk(KERN_INFO "kfree_debugcheck: bad ptr %lxh.\n",
				(unsigned long)objp);
		BUG_BS();
	}
}

static void *cache_free_debugcheck_bs(kmem_cache_t_bs *cachep,
					void *objp, void *caller)
{
	struct page_bs *page;
	unsigned int objnr;
	struct slab_bs *slabp;

	objp -= obj_dbghead_bs(cachep);
	kfree_debugcheck_bs(objp);
	page = virt_to_page_bs(objp);

	if (GET_PAGE_CACHE_BS(page) != cachep) {
		printk(KERN_INFO "mismatch in kmem_cache_free: expected "
			"cache %p, got %p\n", GET_PAGE_CACHE_BS(page), cachep);
		printk(KERN_INFO "%p is %s.\n", cachep, cachep->name);
		printk(KERN_INFO "%p is %s.\n", GET_PAGE_CACHE_BS(page),
					GET_PAGE_CACHE_BS(page)->name);
		WARN_ON_BS(1);
	}
	slabp = GET_PAGE_SLAB_BS(page);

	if (cachep->flags & SLAB_RED_ZONE_BS) {
		if (*dbg_redzone1_bs(cachep, objp) != RED_ACTIVE_BS ||
			  *dbg_redzone2_bs(cachep, objp) != RED_ACTIVE_BS) {
			slab_error_bs(cachep, "double free, or memory outside"
					" object was overwritten");
			printk(KERN_INFO "%p: redzone 1: 0x%lx, redzone 2: "
					"0x%lx.\n", objp, 
					*dbg_redzone1_bs(cachep, objp),
					*dbg_redzone2_bs(cachep, objp));
		}
		*dbg_redzone1_bs(cachep, objp) = RED_INACTIVE_BS;
		*dbg_redzone2_bs(cachep, objp) = RED_INACTIVE_BS;
	}
	if (cachep->flags & SLAB_STORE_USER_BS)
		*dbg_userword_bs(cachep, objp) = caller;

	objnr = (objp-slabp->s_mem)/cachep->objsize;

	BUG_ON_BS(objnr >= cachep->num);
	BUG_ON_BS(objp != slabp->s_mem + objnr*cachep->objsize);

	if (cachep->flags & SLAB_DEBUG_INITIAL_BS) {
		/* Need to call the slab's constructor so the
		 * caller can perform a verify of its state (debugging).
		 * Called without the cache-lock held.
		 */
		cachep->ctor(objp+obj_dbghead_bs(cachep),
				cachep, SLAB_CTOR_CONSTRUCTOR_BS |
					SLAB_CTOR_VERIFY_BS);
	}
	if (cachep->flags & SLAB_POISON_BS && cachep->dtor) {
		/* we want to cache poison the object,
		 * call the destruction callback
		 */
		cachep->dtor(objp+obj_dbghead_bs(cachep), cachep, 0);
	}
	if (cachep->flags & SLAB_POISON_BS) {
#ifdef CONFIG_DEBUG_PAGEALLOC_BS
		if ((cachep->objsize % PAGE_SIZE_BS) == 0 &&
						OFF_SLAB_BS(cachep)) {
			store_stackinfo_bs(cachep, objp, (unsigned long)caller);
			kernel_map_pages_bs(virt_to_page_bs(objp),
					cachep->objsize/PAGE_SIZE_BS, 0);
		} else {
			poison_obj_bs(cachep, objp, POISON_FREE_BS);
		}
#else
		poison_obj_bs(cachep, objp, POISON_FREE_BS);
#endif
	}
	return objp;
}

#else
#define cache_free_debugcheck_bs(x,objp,z)		(objp)
#define cache_alloc_debugcheck_after_bs(a,b,objp,d)	(objp)
#endif

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
	objp = cache_alloc_debugcheck_after_bs(cachep, flags, objp, __builtin_return_address(0));
	return objp;
}

static void kmem_rcu_free_bs(struct rcu_head *head)
{
	struct slab_rcu_bs *slab_rcu = (struct slab_rcu_bs *)head;
	kmem_cache_t_bs *cachep = slab_rcu->cachep;

	kmem_freepages_bs(cachep, slab_rcu->addr);
	if (OFF_SLAB_BS(cachep))
		kmem_cache_free_bs(cachep->slabp_cache, slab_rcu);

}

/* Destroy all the objs in a slab, and release the mem back to the system.
 * Before calling the slab must have been unlinked from the cache.
 * The cache-lock is not held/needed.
 */
static void slab_destroy_bs(kmem_cache_t_bs *cachep, struct slab_bs *slabp)
{
	void *addr = slabp->s_mem - slabp->colouroff;

#if DEBUG
	int i;

	for (i = 0; i < cachep->num; i++) {
		void *objp = slabp->s_mem + cachep->objsize * i;

		if (cachep->flags & SLAB_POISON_BS) {
#ifdef CONFIG_DEBUG_PAGEALLOC_BS
			if ((cachep->objsize%PAGE_SIZE_BS)==0 &&
					OFF_SLAB_BS(cachep))
				kernel_map_pages_bs(virt_to_page_bs(objp),
					cachep->objsize/PAGE_SIZE_BS, 1);
			else
				check_poison_obj_bs(cachep, objp);
#else
			check_poison_obj_bs(cachep, objp);
#endif
		}
		if (cachep->flags & SLAB_RED_ZONE_BS) {
			if (*dbg_redzone1_bs(cachep, objp) != RED_INACTIVE_BS)
				slab_error_bs(cachep, "start of a freed "
						"object was overwritten");

			if (*dbg_redzone2_bs(cachep, objp) != RED_INACTIVE_BS)
				slab_error_bs(cachep, "end of a freed "
						"object was overwritten");
		}
		if (cachep->dtor && !(cachep->flags & SLAB_POISON_BS))
			(cachep->dtor)(objp+obj_dbghead_bs(cachep), 
								cachep, 0);
	}
#else
	if (cachep->dtor) {
		int i;

		for (i = 0; i < cachep->num; i++) {
			void *objp = slabp->s_mem + cachep->objsize * i;

			(cachep->dtor)(objp, cachep, 0);
		}
	}
#endif
	if (unlikely(cachep->flags & SLAB_DESTROY_BY_RCU_BS)) {
		struct slab_rcu_bs *slab_rcu;

		slab_rcu = (struct slab_rcu_bs *)slabp;
		slab_rcu->cachep = cachep;
		slab_rcu->addr = addr;
		call_rcu(&slab_rcu->head, kmem_rcu_free_bs);
	} else {
		kmem_freepages_bs(cachep, addr);
		if (OFF_SLAB_BS(cachep))
			kmem_cache_free_bs(cachep->slabp_cache, slabp);
	}
}

/*
 * NUMA: different approach needed if the spinlock is moved into
 * the l3 structure.
 */
static void free_block_bs(kmem_cache_t_bs *cachep,
					void **objpp, int nr_objects)
{
	int i;

	check_spinlock_acquired_bs(cachep);

	/* NUMA: move and into loop */
	cachep->lists.free_objects += nr_objects;

	for (i = 0; i < nr_objects; i++) {
		void *objp = objpp[i];
		struct slab_bs *slabp;
		unsigned int objnr;

		slabp = GET_PAGE_SLAB_BS(virt_to_page_bs(objp));
		list_del(&slabp->list);
		objnr = (objp - slabp->s_mem) / cachep->objsize;
		check_slabp_bs(cachep, slabp);
#if DEBUG
		if (slab_bufctl_bs(slabp)[objnr] != BUFCTL_FREE_BS) {
			printk(KERN_INFO "slab: double free detected in "
			     "cache '%s', objp %p.\n", cachep->name, objp);
			BUG_BS();
		}
#endif
		slab_bufctl_bs(slabp)[objnr] = slabp->free;
		slabp->free = objnr;
		STATS_DEC_ACTIVE_BS(cachep);
		slabp->inuse--;
		check_slabp_bs(cachep, slabp);

		/* fixup slab chains */
		if (slabp->inuse == 0) {
			if (cachep->lists.free_objects > cachep->free_limit) {
				cachep->lists.free_objects -= cachep->num;
				slab_destroy_bs(cachep, slabp);
			} else {
				list_add(&slabp->list,
				&list3_data_ptr_bs(cachep, objp)->slabs_free);
			}
		} else {
			/* Unconditionally move a slab to the end of the
			 * partial list on free - maximum time for the
			 * other objects to be freed, too.
			 */
			list_add_tail(&slabp->list,
			&list3_data_ptr_bs(cachep, objp)->slabs_partial);
		}
	}
}

static void cache_flusharray_bs(kmem_cache_t_bs *cachep,
					struct array_cache_bs *ac)
{
	int batchcount;

	batchcount = ac->batchcount;
#if DEBUG
	BUG_ON_BS(!batchcount || batchcount > ac->avail);
#endif
	check_irq_off_bs();
	spin_lock(&cachep->spinlock);
	if (cachep->lists.shared) {
		struct array_cache_bs *shared_array = cachep->lists.shared;
		int max = shared_array->limit-shared_array->avail;

		if (max) {
			if (batchcount > max)
				batchcount = max;
			memcpy(&ac_entry_bs(shared_array)[shared_array->avail],
						&ac_entry_bs(ac)[0],
						sizeof(void *)*batchcount);
			shared_array->avail += batchcount;
			goto free_done;
		}
	}

	free_block_bs(cachep, &ac_entry_bs(ac)[0], batchcount);
free_done:
#if STATS
	{
		int i = 0;
		struct list_head *p;

		p = list3_data_bs(cachep)->slabs_free.next;
		while (p != &(list3_data_bs(cachep)->slabs_free)) {
			struct slab_bs *slabp;

			slabp = list_entry(p, struct slab_bs, list);
			BUG_ON_BS(slabp->inuse);

			i++;
			p = p->next;
		}
		STATS_SET_FREEABLE_BS(cachep, i);
	}
#endif
	spin_unlock(&cachep->spinlock);
	ac->avail -= batchcount;
	memmove(&ac_entry_bs(ac)[0], &ac_entry_bs(ac)[batchcount],
				sizeof(void *)*ac->avail);
}

/*
 * __cache_free
 * Release an obj back to its cache. If the obj has a constructed
 * state, it must be in this state _before_ it is released.
 *
 * Called with disabled ints.
 */
static inline void __cache_free_bs(kmem_cache_t_bs *cachep, void *objp)
{
	struct array_cache_bs *ac = ac_data_bs(cachep);

	check_irq_off_bs();
	objp = cache_free_debugcheck_bs(cachep, objp, 
						__builtin_return_address(0));

	if (likely(ac->avail < ac->limit)) {
		STATS_INC_FREEHIT_BS(cachep);
		ac_entry_bs(ac)[ac->avail++] = objp;
		return;
	} else {
		STATS_INC_FREEMISS_BS(cachep);
		cache_flusharray_bs(cachep, ac);
		ac_entry_bs(ac)[ac->avail++] = objp;
	}
}

static inline kmem_cache_t_bs *__find_general_cachep_bs(size_t size, 
								int gfpflags)
{
	struct cache_sizes_bs *csizep = malloc_sizes_bs;

#if DEBUG
	/*
	 * This happens if someone tries to call
	 * kmem_cache_create(), or __kmalloc(), before
	 * the generic caches are initialized.
	 */
	BUG_ON_BS(csizep->cs_cachep == NULL);
#endif
	while (size > csizep->cs_size)
		csizep++;

	/*
	 * Really subtile: The last entry with cs->cs_size == ULONG_MAX
	 * has cs_{dma,}cachep==NULL. Thus no special case
	 * for large kmalloc calls required.
	 */
	if (unlikely(gfpflags & GFP_DMA_BS))
		return csizep->cs_dmacachep;
	return csizep->cs_cachep;
}

static struct array_cache_bs *alloc_arraycache_bs(int cpu, int entries,
							int batchcount)
{
	int memsize = sizeof(void *) * entries + sizeof(struct array_cache_bs);
	struct array_cache_bs *nc = NULL;

	if (cpu == -1)
		nc = kmalloc_bs(memsize, GFP_KERNEL_BS);
	else
		nc = kmalloc_node_bs(memsize, GFP_KERNEL_BS, 
							cpu_to_node_bs(cpu));

	if (nc) {
		nc->avail = 0;
		nc->limit = entries;
		nc->batchcount = batchcount;
		nc->touched = 0;
	}
	return nc;
}

static void do_ccupdate_local_bs(void *info)
{
	struct ccupdate_struct_bs *new = (struct ccupdate_struct_bs *)info;
	struct array_cache_bs *old;

	check_irq_off_bs();
	old = ac_data_bs(new->cachep);

	new->cachep->array[smp_processor_id()] = new->new[smp_processor_id()];
	new->new[smp_processor_id()] = old;
}

/*
 * waits for all CPUs to execute func().
 */
static void smp_call_function_all_cpus_bs(void (*func)(void *arg), void *arg)
{
	check_irq_on_bs();
	preempt_disable();

	local_irq_disable();
	func(arg);
	local_irq_enable();

#if 0
	if (smp_call_function(func, arg, 1, 1))
		BUG_BS();
#endif

	preempt_enable();
}

static int do_tune_cpucache_bs(kmem_cache_t_bs *cachep, int limit, 
				int batchcount, int shared)
{
	struct ccupdate_struct_bs new;
	struct array_cache_bs *new_shared;
	int i;

	memset(&new.new, 0, sizeof(new.new));
	for (i = 0; i < NR_CPUS_BS; i++) {
		if (cpu_online(i)) {
			new.new[i] = alloc_arraycache_bs(i, limit, batchcount);
			if (!new.new[i]) {
				for (i--; i >= 0; i--)
					kfree_bs(new.new[i]);
				return -ENOMEM;
			}
		} else {
			new.new[i] = NULL;
		}
	}
	new.cachep = cachep;

	smp_call_function_all_cpus_bs(do_ccupdate_local_bs, (void *)&new);

	check_irq_on_bs();
	spin_lock_irq(&cachep->spinlock);
	cachep->batchcount = batchcount;
	cachep->limit = limit;
	cachep->free_limit = (1+num_online_cpus())*cachep->batchcount +
					cachep->num;
	spin_unlock_irq(&cachep->spinlock);

	for (i = 0; i < NR_CPUS_BS; i++) {
		struct array_cache_bs *ccold = new.new[i];

		if (!ccold)
			continue;
		spin_lock_irq(&cachep->spinlock);
		free_block_bs(cachep, ac_entry_bs(ccold), ccold->avail);
		spin_unlock_irq(&cachep->spinlock);
		kfree_bs(ccold);
	}
	new_shared = alloc_arraycache_bs(-1, batchcount*shared, 0xbaadf00d);
	if (new_shared) {
		struct array_cache_bs *old;

		spin_lock_irq(&cachep->spinlock);
		old = cachep->lists.shared;
		cachep->lists.shared = new_shared;
		if (old)
			free_block_bs(cachep, ac_entry_bs(old), old->avail);
		spin_unlock_irq(&cachep->spinlock);
		kfree_bs(old);
	}

	return 0;
}

static void enable_cpucache_bs(kmem_cache_t_bs *cachep)
{
	int err;
	int limit, shared;

	/* The head array serves three purposes:
	 * - create a LIFO ordering, i.e. return objects that are cache-warm
	 * - reduce the number of spinlock operations.
	 * - reduce the number of linked list operations on the slab and 
	 *   bufctl chains: array operations are cheaper.
	 * The numbers are guessed, we should auto-tune as described by
	 * Bonwick.
	 */
	if (cachep->objsize > 131072)
		limit = 1;
	else if (cachep->objsize > PAGE_SIZE_BS)
		limit = 8;
	else if (cachep->objsize > 1024)
		limit = 24;
	else if (cachep->objsize > 256)
		limit = 54;
	else
		limit = 120;

	/* Cpu bound tasks (e.g. network routing) can exhibit cpu bound
	 * allocation behaviour: Most allocs on one cpu, most free operations
	 * on another cpu. For these cases, an efficient object passing between
	 * cpus is necessary. This is provided by a shared array. The array
	 * replaces Bonwick's magazine layer.
	 * On uniprocessor, it's functionally equivalent (but less efficient)
	 * to a larger limit. Thus disabled by default.
	 */
	shared = 0;
#ifdef CONFIG_SMP
	if (cachep->objsize <= PAGE_SIZE_BS)
		shared = 8;
#endif

#if DEBUG
	/* With debugging enabled, large batchcount lead to excessively
	 * long periods with disabled local interrupts. Limit the 
	 * batchcount
	 */
	if (limit > 32)
		limit = 32;
#endif
	err = do_tune_cpucache_bs(cachep, limit, (limit+1)/2, shared);
	if (err)
		printk(KERN_INFO "enable_cpucache failed for %s, error %d.\n",
				cachep->name, -err);
}

/**
 * kfree - free previously allocated memory
 * @objp: pointer returned by kmalloc.
 *
 * Don't free memory not originally allocated by kmalloc()
 * or you will run into trouble. 
 */
void kfree_bs(const void *objp)
{
	kmem_cache_t_bs *c;
	unsigned long flags;

	if (unlikely(!objp))
		return;
	local_irq_save(flags);
	kfree_debugcheck_bs(objp);
	c = GET_PAGE_CACHE_BS(virt_to_page_bs(objp));
	__cache_free_bs(c, (void *)objp);
	local_irq_restore(flags);
}
EXPORT_SYMBOL_GPL(kfree_bs);

/**
 * kmalloc - allocate memory
 * @size: how many bytes of memory are required.
 * @flags: the type of memory to allocate.
 *
 * kmalloc is the normal method of allocating memory
 * in the kernel.
 *
 * The @flags argument may be one of:
 *
 * %GFP_USER - Allocate memory on behalf of user.  May sleep.
 *
 * %GFP_KERNEL - Allocate normal kernel ram.  May sleep.
 *
 * %GFP_ATOMIC - Allocation will not sleep.  Use inside interrupt handlers.
 *
 * Additionally, the %GFP_DMA flag may be set to indicate the memory
 * must be suitable for DMA.  This can mean different things on different
 * platforms.  For example, on i386, it means that the memory must come
 * from the first 16MB.
 */
void *__kmalloc_bs(size_t size, unsigned int __nocast flags)
{
	kmem_cache_t_bs *cachep;

	/* If you want to save a few bytes .text space: replace
	 * __ with kmem_.
	 * Then kmalloc uses the uninlined functions instead of the
	 * inline functions.
	 */
	cachep = __find_general_cachep_bs(size, flags);
	if (unlikely(cachep == NULL))
		return NULL;
	return __cache_alloc_bs(cachep, flags);
}
EXPORT_SYMBOL_GPL(__kmalloc_bs);

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
 * kmem_cache_free - Deallocate an object
 * @cachep: The cache the allocation was from.
 * @objp: The previously allocated object.
 *
 * Free an object which was previously allocated from this
 * cache.
 */
void kmem_cache_free_bs(kmem_cache_t_bs *cachep, void *objp)
{
	unsigned long flags;

	local_irq_save(flags);
	__cache_free_bs(cachep, objp);
	local_irq_restore(flags);	
}
EXPORT_SYMBOL_GPL(kmem_cache_free_bs);

kmem_cache_t_bs *kmem_find_general_cachep_bs(size_t size, int gfpflags)
{
	return __find_general_cachep_bs(size, gfpflags);
}
EXPORT_SYMBOL_GPL(kmem_find_general_cachep_bs);

/**
 * kmem_ptr_validate - check if an untrusted pointer might
 *      be a slab entry.
 * @cachep: the cache we're checking against
 * @ptr: pointer to validate
 *
 * This verifies that the untrusted pointer looks sane:
 * it is _not_ a guarantee that the pointer is actually
 * part of the slab cache in question, but it at least
 * validates that the pointer can be dereferenced and
 * looks half-way sane.
 *      
 * Currently only used for dentry validation.
 */
int fastcall_bs kmem_ptr_validate_bs(kmem_cache_t_bs *cachep, void *ptr)
{
	unsigned long addr = (unsigned long)ptr;
	unsigned long min_addr = PAGE_OFFSET_BS;
	unsigned long align_mask = BYTES_PER_WORD_BS - 1;
	unsigned long size = cachep->objsize;
	struct page_bs *page;

	if (unlikely(addr < min_addr))
		goto out;
	if (unlikely(addr > (unsigned long)high_memory_bs - size))
		goto out;
	if (unlikely(addr & align_mask))
		goto out;
	if (unlikely(!kern_addr_valid_bs(addr)))
		goto out;
	if (unlikely(!kern_addr_valid_bs(addr + size - 1)))
		goto out;
	page = virt_to_page_bs(ptr);
	if (unlikely(!PageSlab_bs(page)))
		goto out;
	if (unlikely(GET_PAGE_CACHE_BS(page) != cachep))
		goto out;
	return 1;
out:
	return 0;
}

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
		printk(KERN_INFO "%s: Early error in slab %s\n",
				__FUNCTION__, name);
		BUG_BS();
	}

#if DEBUG
	WARN_ON_BS(strchr(name, ' '));	/* It confuse parsers */
	if ((flags & SLAB_DEBUG_INITIAL_BS) && !ctor) {
		/* No constructor, but inital state check requested */
		printk(KERN_INFO "%s: No con, but init state check "
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
	if (!(flags & SLAB_DESTROY_BY_RCU_BS))
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
	if (!cachep)
		goto opps;
	memset(cachep, 0, sizeof(kmem_cache_t_bs));

#if DEBUG
	cachep->reallen = size;

	if (flags & SLAB_RED_ZONE_BS) {
		/* redzone only works with word aligned caches */
		align = BYTES_PER_WORD_BS;

		/* add space for red zone words */
		cachep->dbghead += BYTES_PER_WORD_BS;
		size += 2*BYTES_PER_WORD_BS;
	}
	if (flags & SLAB_STORE_USER_BS) {
		/* user store requires word alignment and
		 * one word storage behind the end of the real
		 * object.
		 */
		align = BYTES_PER_WORD_BS;
		size += BYTES_PER_WORD_BS;
	}
#if FORCED_DEBUG && defined (CONFIG_DEBUG_PAGEALLOC_BS)
	if (size > 128 && cachep->reallen > cache_line_size_bs() && 
							size < PAGE_SIZE) {
		cachep->dbghead += PAGE_SIZE_BS - size;
		size = PAGE_SIZE_BS;
	}
#endif
#endif

	/* Determine if the slab management is 'on' or 'off' slab */
	if (size >= (PAGE_SIZE_BS >> 3))
		/*
		 * Size is large, assume best to place the slab management obj
		 * off-slab (should allow better packing of objs).
		 */
		flags |= CFLGS_OFF_SLAB_BS;

	size = ALIGN(size, align);

	if ((flags & SLAB_RECLAIM_ACCOUNT_BS) && size <= PAGE_SIZE_BS) {
		/*
		 * A VFS-reclaimable slab tends to have most allocations
		 * as GFP_NOFS and we really don't want to have to be
		 * allocatiing higher-order pages when we are unable to
		 * shrink dcache.
		 */
		cachep->gfporder = 0;
		cache_estimate_bs(cachep->gfporder, size, align, flags,
						&left_over, &cachep->num);
	} else {
		/*
		 * Calculate size (in pages) of slabs, and the num of objs per
		 * slab.  This could be made much more intelligent.  For now,
		 * try to avoid using high page-orders for slabs.  When the
		 * gfp() funcs are more friendly towards high-order requests,
		 * this should be changed.
		 */
		do {
			unsigned int break_flag = 0;
cal_wastage:
			cache_estimate_bs(cachep->gfporder, size, align,
					   flags, &left_over, &cachep->num);
			if (break_flag)
				break;
			if (cachep->gfporder >= MAX_GFP_ORDER_BS)
				break;
			if (!cachep->num)
				goto next;
			if (flags & CFLGS_OFF_SLAB_BS &&
					cachep->num > offslab_limit_bs) {
				/* This num of objs will cause problems. */
				cachep->gfporder--;
				break_flag++;
				goto cal_wastage;
			}

			/*
			 * Large num of objs is good, but v. large slabs are
			 * currently bad for the gfp()s.
			 */
			if (cachep->gfporder >= slab_break_gfp_order_bs)
				break;

			if ((left_over*8) <= 
					(PAGE_SIZE_BS << cachep->gfporder))
				break; /* Acceptable internal fragmentation. */
next:
			cachep->gfporder++;
		} while (1);

	}

	if (!cachep->num) {
		printk("kmem_cache_create: couldn't create cache %s.\n", name);
		kmem_cache_free_bs(&cache_cache_bs, cachep);
		cachep = NULL;
		goto opps;
	}
	slab_size = ALIGN(cachep->num * sizeof(kmem_bufctl_t_bs) +
					sizeof(struct slab_bs), align);

	/*
	 * If the slab has been placed off-slab, and we have enough space
	 * then move it on-slab. This it at the expense of any extra
	 * colouring.
	 */
	if (flags & CFLGS_OFF_SLAB_BS && left_over >= slab_size) {
		flags &= ~CFLGS_OFF_SLAB_BS;
		left_over -= slab_size;
	}

	if (flags & CFLGS_OFF_SLAB_BS) {
		/* really off slab. No need for manual alignment */
		slab_size = cachep->num*sizeof(kmem_bufctl_t_bs)+
						sizeof(struct slab_bs);
	}

	cachep->colour_off = cache_line_size_bs();
	/* Offset must be a multiple of the alignment */
	if (cachep->colour_off < align)
		cachep->colour_off = align;
	cachep->colour = left_over / cachep->colour_off;
	cachep->slab_size = slab_size;
	cachep->flags = flags;
	cachep->gfpflags = 0;
	if (flags & SLAB_CACHE_DMA_BS)
		cachep->gfpflags |= GFP_DMA_BS;
	spin_lock_init(&cachep->spinlock);
	cachep->objsize = size;
	/* NUMA */
	INIT_LIST_HEAD(&cachep->lists.slabs_full);
	INIT_LIST_HEAD(&cachep->lists.slabs_partial);
	INIT_LIST_HEAD(&cachep->lists.slabs_free);

	if (flags & CFLGS_OFF_SLAB_BS)
		cachep->slabp_cache = kmem_find_general_cachep_bs(slab_size, 0);
	cachep->ctor = ctor;
	cachep->dtor = dtor;
	cachep->name = name;

	/* Don't let CPUs to come and go */
	//lock_cpu_hotplug();

	if (g_cpucache_up_bs == FULL_BS) {
		enable_cpucache_bs(cachep);
	} else {
		if (g_cpucache_up_bs == NONE_BS) {
			/* Note: the first kmem_cache_create must create
			 * the cache that's used by kmalloc(24), otherwise
			 * the creation of further caches will BUG().
			 */
			cachep->array[smp_processor_id()] = 
					&initarray_generic_bs.cache;
			g_cpucache_up_bs = PARTIAL_BS;
		} else {
			cachep->array[smp_processor_id()] =
				kmalloc_bs(sizeof(struct arraycache_init_bs),
							GFP_KERNEL_BS);
		}
		BUG_ON_BS(!ac_data_bs(cachep));
		ac_data_bs(cachep)->avail = 0;
		ac_data_bs(cachep)->limit = BOOT_CPUCACHE_ENTRIES_BS;
		ac_data_bs(cachep)->batchcount = 1;
		ac_data_bs(cachep)->touched = 1;
		cachep->batchcount = 1;
		cachep->limit = BOOT_CPUCACHE_ENTRIES_BS;
		cachep->free_limit = (1+num_online_cpus()) * 
				cachep->batchcount + cachep->num;
	}

	cachep->lists.next_reap = jiffies + REAPTIMEOUT_LIST3_BS +
				((unsigned long)cachep)%REAPTIMEOUT_LIST3_BS;

	/* Need the semaphore to access the chain */
	down(&cache_chain_sem_bs);
	{
		struct list_head *p;
		mm_segment_t old_fs;
	
		old_fs = get_fs();
		set_fs(KERNEL_DS);
		list_for_each(p, &cache_chain_bs) {
			kmem_cache_t_bs *pc = 
					list_entry(p, kmem_cache_t_bs, next);
			char tmp;

			/* This happens when the module gets unloaded and 
			 * doesn't destroy its slab cache and noone else
			 * reuses the vmalloc area of the module. Print a
			 * warning.
			 */
			if (__get_user(tmp, pc->name)) {
				printk("SLAB: cahce with size %d has lost "
						"its name\n", pc->objsize);
				continue;
			}
			if (!strcmp(pc->name, name)) {
				printk("kmem_cache_create: duplicate "
							"cache %s\n", name);
				up(&cache_chain_sem_bs);
				// unlock_cpu_hotplug
				BUG_BS();
			}
		}

		set_fs(old_fs);
	}

	/* cache setup completed, link it into the list */
	list_add(&cachep->next, &cache_chain_bs);
	up(&cache_chain_sem_bs);
	// unlock_cup_hotplug();
opps:
	if (!cachep && (flags & SLAB_PANIC_BS))
		panic("kmem_cache_create(): failed to create slab '%s'\n",
					name);
	return cachep;
}
EXPORT_SYMBOL_GPL(kmem_cache_create_bs);

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

static void drain_array_locked_bs(kmem_cache_t_bs *cachep,
				struct array_cache_bs *ac, int force)
{
	int tofree;

	check_spinlock_acquired_bs(cachep);
	if (ac->touched && !force) {
		ac->touched = 0;
	} else if (ac->avail) {
		tofree = force ? ac->avail : (ac->limit+4)/5;
		if (tofree > ac->avail) {
			tofree = (ac->avail+1)/2;
		}
		free_block_bs(cachep, ac_entry_bs(ac), tofree);
		ac->avail -= tofree;
		memmove(&ac_entry_bs(ac)[0], &ac_entry_bs(ac)[tofree],
		sizeof(void*)*ac->avail);
}
}

/**
 * cache_reap - Reclaim memory from caches.
 *
 * Called from workqueue/eventd every few seconds.
 * Purpose:
 * - clear the per-cpu caches for this CPU.
 * - return freeable pages to the main free memory pool.
 *
 * If we cannot acquire the cache chain semaphore then just give up - we'll
 * try again on the next iteration.
 */
static void cache_reap_bs(struct work_struct *w)
{
	struct list_head *walk;

	if (down_trylock(&cache_chain_sem_bs)) {
		/* Give up. Setup the next iteration */
		schedule_delayed_work(
			(struct delayed_work *)&__get_cpu_var_bs(reap_work_bs),
			REAPTIMEOUT_CPUC_BS + smp_processor_id());
	}

	list_for_each(walk, &cache_chain_bs) {
		kmem_cache_t_bs *searchp;
		struct list_head *p;
		int tofree;
		struct slab_bs *slabp;

		searchp = list_entry(walk, kmem_cache_t_bs, next);

		if (searchp->flags & SLAB_NO_REAP_BS)
			goto next;

		check_irq_on_bs();

		spin_lock_irq(&searchp->spinlock);

		drain_array_locked_bs(searchp, ac_data_bs(searchp), 0);

		if (time_after(searchp->lists.next_reap, jiffies))
			goto next_unlock;

		searchp->lists.next_reap = jiffies + REAPTIMEOUT_LIST3_BS;

		if (searchp->lists.shared)
			drain_array_locked_bs(searchp,
						searchp->lists.shared, 0);

		if (searchp->lists.free_touched) {
			searchp->lists.free_touched = 0;
			goto next_unlock;
		}

		tofree = (searchp->free_limit + 5 * searchp->num - 1) /
					(5 * searchp->num);

		do {
			p = list3_data_bs(searchp)->slabs_free.next;
			if (p == &(list3_data_bs(searchp)->slabs_free))
				break;

			slabp = list_entry(p, struct slab_bs, list);
			BUG_ON_BS(slabp->inuse);
			list_del(&slabp->list);
			STATS_INC_REAPED_BS(searchp);

			/* Safe to drop the lock. The slab is no longer
			 * linked to the cache.
			 * searchp cannot disappear, we hold
			 * cache_chain_lock
			 */
			searchp->lists.free_objects -= searchp->num;
			spin_unlock_irq(&searchp->spinlock);
			slab_destroy_bs(searchp, slabp);
			spin_lock_irq(&searchp->spinlock);
		} while (--tofree > 0);
next_unlock:
		spin_unlock_irq(&searchp->spinlock);
next:
		cond_resched();
	}
	check_irq_on_bs();
	up(&cache_chain_sem_bs);
	/* Setup the next iteration */
	schedule_delayed_work(
			(struct delayed_work *)&__get_cpu_var_bs(reap_work_bs), 
			REAPTIMEOUT_CPUC_BS + smp_processor_id());
}

/*
 * Initiate the reap timer running on the target CPU.  We run at around 1 to 2Hz
 * via the workqueue/eventd.
 * Add the CPU number into the expiration time to minimize the possibility of
 * the CPUs getting into lockstep and contending for the global cache chain
 * lock.
 */
static void __devinit start_cpu_timer_bs(int cpu)
{
	struct work_struct *reap_work = &per_cpu_bs(reap_work_bs, cpu);	

	/*
	 * When this gets called from do_initcalls via cpucache_init(),
	 * init_workqueues() has already run, so keventd will be setup
	 * at that time.
	 */
	if (reap_work->func == NULL) {
		INIT_WORK(reap_work, cache_reap_bs);
		schedule_delayed_work_on(cpu, 
			(struct delayed_work *)reap_work, HZ + 3 * cpu);
	}
}

static int __devinit cpuup_callback_bs(struct notifier_block *nfb,
					unsigned long action, void *hcpu)
{
	long cpu = (long)hcpu;
	kmem_cache_t_bs *cachep;

	switch (action) {
	case CPU_UP_PREPARE_BS:
		down(&cache_chain_sem_bs);
		list_for_each_entry(cachep, &cache_chain_bs, next) {
			struct array_cache_bs *nc;

			nc = alloc_arraycache_bs(cpu, cachep->limit, 
							cachep->batchcount);
			if (!nc)
				goto bad;
			spin_lock_irq(&cachep->spinlock);
			cachep->array[cpu] = nc;
			cachep->free_limit = (1 + num_online_cpus()) * 
					cachep->batchcount + cachep->num;
			spin_unlock_irq(&cachep->spinlock);
		}
		up(&cache_chain_sem_bs);
		break;
	case CPU_ONLINE_BS:
		start_cpu_timer_bs(cpu);
		break;
#ifdef CONFIG_HOTPLUG_CPU
	case CPU_DEAD_BS:
		/* fall thru */
	case CPU_UP_CANCELED_BS:
		down(&cache_chain_sem_bs);

		list_for_each_entry(cachep, &cache_chain_bs, next) {
			struct array_cache_bs *nc;

			spin_lock_irq(&cachep->spinlock);
			/* cpu is dead; no one can alloc from it. */
			nc = cachep->array[cpu];
			cachep->array[cpu] = NULL;
			cachep->free_limit -= cachep->batchcount;
			free_block_bs(cachep, ac_entry_bs(nc), nc->avail);
			spin_unlock_irq(&cachep->spinlock);
			kfree_bs(nc);
		}
		up(&cache_chain_sem_bs);
		break;
#endif
	}
	return NOTIFY_OK;

bad:
	up(&cache_chain_sem_bs);
	return NOTIFY_BAD;
}

static void do_drain_bs(void *arg)
{
	kmem_cache_t_bs *cachep = (kmem_cache_t_bs *)arg;
	struct array_cache_bs *ac;

	check_irq_off_bs();
	ac = ac_data_bs(cachep);
	spin_lock(&cachep->spinlock);
	free_block_bs(cachep, &ac_entry_bs(ac)[0], ac->avail);
	spin_unlock(&cachep->spinlock);
	ac->avail = 0;
}

static void drain_cpu_caches_bs(kmem_cache_t_bs *cachep)
{
	smp_call_function_all_cpus_bs(do_drain_bs, cachep);
	check_irq_on_bs();
	spin_lock_irq(&cachep->spinlock);
	if (cachep->lists.shared)
		drain_array_locked_bs(cachep, cachep->lists.shared, 1);
	spin_unlock_irq(&cachep->spinlock);
}

/* NUMA shrink all list3s */
static int __cache_shrink_bs(kmem_cache_t_bs *cachep)
{
	struct slab_bs *slabp;
	int ret;

	drain_cpu_caches_bs(cachep);

	check_irq_on_bs();
	spin_lock_irq(&cachep->spinlock);

	for (;;) {
		struct list_head *p;

		p = cachep->lists.slabs_free.prev;
		if (p == &cachep->lists.slabs_free)
			break;

		slabp = list_entry(cachep->lists.slabs_free.prev,
						struct slab_bs, list);
#if DEBUG
		if (slabp->inuse)
			BUS_BS();
#endif
		list_del(&slabp->list);

		cachep->lists.free_objects -= cachep->num;
		spin_unlock_irq(&cachep->spinlock);
		slab_destroy_bs(cachep, slabp);
		spin_lock_irq(&cachep->spinlock);
	}
	ret = !list_empty(&cachep->lists.slabs_full) ||
		!list_empty(&cachep->lists.slabs_partial);
	spin_unlock_irq(&cachep->spinlock);
	return ret;
}

/**
 * kmem_cache_destroy - delete a cache
 * @cachep: the cache to destroy
 *
 * Remove a kmem_cache_t object from the slab cache.
 * Returns 0 on success.
 *
 * It is expected this function will be called by a module when it is
 * unloaded.  This will remove the cache completely, and avoid a duplicate
 * cache being allocated each time a module is loaded and unloaded, if the
 * module doesn't have persistent in-kernel storage across loads and unloads.
 *
 * The cache must be empty before calling this function.
 *
 * The caller must guarantee that noone will allocate memory from the cache
 * during the kmem_cache_destroy().
 */
int kmem_cache_destroy_bs(kmem_cache_t_bs *cachep)
{
	int i;

	if (!cachep || in_interrupt())
		BUG_BS();

	/* Don't let CPUs to come and go */
	//lock_cpu_hotplug();

	/* Find the cache in the chain of caches. */
	down(&cache_chain_sem_bs);

	/*
	 * the chain is never empty, cache_cache is never destroyed
	 */
	list_del(&cachep->next);
	up(&cache_chain_sem_bs);

	if (__cache_shrink_bs(cachep)) {
		slab_error_bs(cachep, "Can't free all objects");
		down(&cache_chain_sem_bs);
		list_add(&cachep->next, &cache_chain_bs);
		up(&cache_chain_sem_bs);
		//unlock_cpu_hotplug();
		return 1;
	}

	if (unlikely(cachep->flags & SLAB_DESTROY_BY_RCU_BS))
		synchronize_rcu();

	/* no cpu_online check required here since we clear the percpu
	 * array on cpu offline and set this to NULL.
	 */
	for (i = 0; i < NR_CPUS_BS; i++)
		kfree_bs(cachep->array[i]);

	/* NUMA: free the list3 structures */
	kfree_bs(cachep->lists.shared);
	cachep->lists.shared = NULL;
	kmem_cache_free_bs(&cache_cache_bs, cachep);

	// unlock_cpu_hotplug();

	return 0;
}
EXPORT_SYMBOL_GPL(kmem_cache_destroy_bs);

/**
 * kmem_cache_shrink - Shrink a cache.
 * @cachep: The cache to shrink.
 *
 * Releases as many slabs as possible for a cache.
 * To help debugging, a zero exit status indicates all slabs were released.
 */
int kmem_cache_shrink_bs(kmem_cache_t_bs *cachep)
{
	if (!cachep || in_interrupt())
		BUG_BS();

	return __cache_shrink_bs(cachep);
}
EXPORT_SYMBOL_GPL(kmem_cache_shrink_bs);

static __unused struct notifier_block cpucache_notifier_bs = 
				{ &cpuup_callback_bs, NULL, 0};

/* FIXME: BiscuitOS slab debug stuf */
DEBUG_FUNC_T(slab);

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
	spin_lock_init(&cache_cache_bs.spinlock);
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

		/* Inc off-slab bufctl limit until the ceiling is hit. */
		if (!(OFF_SLAB_BS(sizes->cs_cachep))) {
			offslab_limit_bs = sizes->cs_size - 
						sizeof(struct slab_bs);
			offslab_limit_bs /= sizeof(kmem_bufctl_t_bs);
		}

		sizes->cs_dmacachep = kmem_cache_create_bs(
					names->name_dma,
					sizes->cs_size,
					ARCH_KMALLOC_MINALIGN_BS,
					(ARCH_KMALLOC_FLAGS_BS |
					SLAB_CACHE_DMA_BS | SLAB_PANIC_BS),
					NULL,
					NULL);

		sizes++;
		names++;
	}
	/* 4) Replace the bootstrap head arrays */
	{
		void *ptr;

		ptr = kmalloc_bs(sizeof(struct arraycache_init_bs), 
							GFP_KERNEL_BS);
		local_irq_disable();
		BUG_ON_BS(ac_data_bs(&cache_cache_bs) != 
						&initarray_cache_bs.cache);
		memcpy(ptr, ac_data_bs(&cache_cache_bs),
					sizeof(struct arraycache_init_bs));
		cache_cache_bs.array[smp_processor_id()] = ptr;
		local_irq_enable();

		ptr = kmalloc_bs(sizeof(struct arraycache_init_bs),
							GFP_KERNEL_BS);
		local_irq_disable();
		BUG_ON_BS(ac_data_bs(malloc_sizes_bs[0].cs_cachep) !=
						&initarray_generic_bs.cache);
		memcpy(ptr, ac_data_bs(malloc_sizes_bs[0].cs_cachep),
					sizeof(struct arraycache_init_bs));
		malloc_sizes_bs[0].cs_cachep->array[smp_processor_id()] = ptr;
		local_irq_enable();
	}

	/* 5) resize the head arrays to their final sizes */
	{
		kmem_cache_t_bs *cachep;
		down(&cache_chain_sem_bs);
		list_for_each_entry(cachep, &cache_chain_bs, next)
			enable_cpucache_bs(cachep);
		up(&cache_chain_sem_bs);
	}

	/* Done! */
	g_cpucache_up_bs = FULL_BS;

	/* Register a cpu startup notifier callback
	 * that initializes ac_data for all new cpus
	 */
	register_cpu_notifier(&cpucache_notifier_bs);

	/* The reap timers are started later, with a module init call:
	 * That part of the kernel is not yet operational.
	 */

	/* FIXME: slab_initcall entry, used to debug slab,
	 * This code isn't default code */
	DEBUG_CALL(slab);
}

#ifdef CONFIG_SMP
/**
 * __alloc_percpu - allocate one copy of the object for every present
 * cpu in the system, zeroing them.
 * Objects should be dereferenced using the per_cpu_ptr macro only.
 *
 * @size: how many bytes of memory are required.
 * @align: the alignment, which can't be greater than SMP_CACHE_BYTES.
 */
void *__alloc_percpu_bs(size_t size, size_t align)
{
	int i;
	struct percpu_data_bs *pdata = 
			kmalloc_bs(sizeof (*pdata), GFP_KERNEL_BS);

	if (!pdata)
		return NULL;

	for (i = 0; i < NR_CPUS_BS; i++) {
		if (!cpu_possible(i))
			continue;
		pdata->ptrs[i] = kmalloc_node_bs(size, GFP_KERNEL_BS,
						cpu_to_node_bs(i));

		if (!pdata->ptrs[i])
			goto unwind_oom;
		memset(pdata->ptrs[i], 0, size);
	}

	/* Catch derefs w/o wrappers */
	return (void *) (~(unsigned long) pdata);

unwind_oom:
	while (--i >= 0) {
		if (!cpu_possible(i))
			continue;
		kfree_bs(pdata->ptrs[i]);
	}
	kfree_bs(pdata);
	return NULL;
}
EXPORT_SYMBOL(__alloc_percpu_bs);

/**
 * free_percpu - free previously allocated percpu memory
 * @objp: pointer returned by alloc_percpu.
 *
 * Don't free memory not originally allocated by alloc_percpu()
 * The complemented objp is to check for that.
 */
void
free_percpu_bs(const void *objp)
{
	int i;
	struct percpu_data_bs *p = 
			(struct percpu_data_bs *) (~(unsigned long) objp);

	for (i = 0; i < NR_CPUS_BS; i++) {
		if (!cpu_possible(i))
			continue;
		kfree_bs(p->ptrs[i]);
	}
	kfree_bs(p);
}
EXPORT_SYMBOL_GPL(free_percpu_bs);
#endif

static int __init __unused cpucache_init_bs(void)
{
	int cpu;

	/* 
	 * Register the timers that return unneeded
	 * pages to gfp.
	 */
	for (cpu = 0; cpu < NR_CPUS_BS; cpu++) {
		if (cpu_online(cpu))
			start_cpu_timer_bs(cpu);
	}

	return 0;
}
__initcall_bs(cpucache_init_bs);

static void *s_start_bs(struct seq_file *m, loff_t *pos)
{
	loff_t n = *pos;
	struct list_head *p;

	down(&cache_chain_sem_bs);
	if (!n) {
		/*
		 * Output format version, so at least we can change it
		 * without _too_ many complaints.
		 */
#if STATS
		seq_puts(m, "slabinfo - version: 2.1 (statistics)\n");
#else           
		seq_puts(m, "slabinfo - version: 2.1\n");
#endif          
		seq_puts(m, "# name            <active_objs> <num_objs> "
				"<objsize> <objperslab> <pagesperslab>");
		seq_puts(m, " : tunables <limit> <batchcount> <sharedfactor>");
		seq_puts(m, " : slabdata <active_slabs> <num_slabs> "
				"<sharedavail>");
#if STATS
		seq_puts(m, " : globalstat <listallocs> <maxobjs> <grown> "
				"<reaped> <error> <maxfreeable> <freelimit> "
				"<nodeallocs>");             
		seq_puts(m, " : cpustat <allochit> <allocmiss> <freehit> "
				"<freemiss>");
#endif
		seq_putc(m, '\n');
	}
	p = cache_chain_bs.next;
	while (n--) {
		p = p->next;
		if (p == &cache_chain_bs)
			return NULL;
	}
	return list_entry(p, kmem_cache_t_bs, next);
}

static void *s_next_bs(struct seq_file *m, void *p, loff_t *pos)
{
	kmem_cache_t_bs *cachep = p;
	++*pos;
	return cachep->next.next == &cache_chain_bs ? NULL :
		list_entry(cachep->next.next, kmem_cache_t_bs, next);
}

static void s_stop_bs(struct seq_file *m, void *p)
{
	up(&cache_chain_sem_bs);
}

static int s_show_bs(struct seq_file *m, void *p)
{
	kmem_cache_t_bs *cachep = p;
	struct list_head *q;
	struct slab_bs *slabp;
	unsigned long active_objs;
	unsigned long num_objs;
	unsigned long active_slabs = 0;
	unsigned long num_slabs;
	const char *name;
	char *error = NULL;

	check_irq_on_bs();
	spin_lock_irq(&cachep->spinlock);
	active_objs = 0;
	num_slabs = 0;
	list_for_each(q, &cachep->lists.slabs_full) {
		slabp = list_entry(q, struct slab_bs, list);
		if (slabp->inuse != cachep->num && !error)
			error = "slabs_full accounting error";
		active_objs += cachep->num;
		active_slabs++;
	}
	list_for_each(q, &cachep->lists.slabs_partial) {
		slabp = list_entry(q, struct slab_bs, list);
		if (slabp->inuse == cachep->num && !error)
			error = "slabs_partial inuse accounting error";
		if (!slabp->inuse && !error)
			error = "slabs_partial/inuse accounting error";
		active_objs += slabp->inuse;
		active_slabs++;
	}
	list_for_each(q, &cachep->lists.slabs_free) {
		slabp = list_entry(q, struct slab_bs, list);
		if (slabp->inuse && !error)
			error = "slabs_free/inuse accounting error";
		num_slabs++;
	}
	num_slabs += active_slabs;
	num_objs = num_slabs * cachep->num;
	if (num_objs - active_objs != cachep->lists.free_objects && !error)
		error = "free_objects accounting error";

	name = cachep->name;
	if (error)
		printk(KERN_ERR "slab: cache %s error: %s\n", name, error);

	seq_printf(m, "%-17s %6lu %6lu %6u %4u %4d",
		name, active_objs, num_objs, cachep->objsize,
		cachep->num, (1<<cachep->gfporder));
	seq_printf(m, " : tunables %4u %4u %4u",
		cachep->limit, cachep->batchcount,
		cachep->lists.shared->limit/cachep->batchcount);
	seq_printf(m, " : slabdata %6lu %6lu %6u",
		active_slabs, num_slabs, cachep->lists.shared->avail);

#if STATS
	{	/* list3 stats */
		unsigned long high = cachep->high_mark;
		unsigned long allocs = cachep->num_allocations;
		unsigned long grown = cachep->grown;
		unsigned long reaped = cachep->reaped;
		unsigned long errors = cachep->errors;
		unsigned long max_freeable = cachep->max_freeable;
		unsigned long free_limit = cachep->free_limit;
		unsigned long node_allocs = cachep->node_allocs;

		seq_printf(m, " : globalstat %7lu %6lu %5lu %4lu %4lu "
				"%4lu %4lu %4lu",
			allocs, high, grown, reaped, errors,
			max_freeable, free_limit, node_allocs);
	}
	/* cpu stats */
	{
		unsigned long allochit = atomic_read(&cachep->allochit);
		unsigned long allocmiss = atomic_read(&cachep->allocmiss);
		unsigned long freehit = atomic_read(&cachep->freehit);
		unsigned long freemiss = atomic_read(&cachep->freemiss);

		seq_printf(m, " : cpustat %6lu %6lu %6lu %6lu",
		allochit, allocmiss, freehit, freemiss);
	}
#endif
	seq_putc(m, '\n');
	spin_unlock_irq(&cachep->spinlock);
	return 0;
}

/*
 * slabinfo_op - iterator that generates /proc/slabinfo
 *
 * Output layout:
 * cache-name
 * num-active-objs
 * total-objs
 * object size
 * num-active-slabs
 * total-slabs
 * num-pages-per-slab
 * + further values on SMP and with statistics enabled
 */
struct seq_operations slabinfo_op_bs = {
	.start	= s_start_bs,
	.next	= s_next_bs,
	.stop	= s_stop_bs,
	.show	= s_show_bs,
};

