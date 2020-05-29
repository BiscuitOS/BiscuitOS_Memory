#ifndef CONFIG_SLOB_BS
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
 * 15 March 2005. NUMA slab allocator.
 *	Shai Fultheim <shai@scalex86.org>.
 *	Shobhit Dayal <shobhit@calsoftinc.com>
 *	Alok N Kataria <alokk@calsoftinc.com>
 *	Christoph Lameter <christoph@lameter.com>
 *
 *	Modified the slab allocator to be node aware on NUMA systems.
 *	Each node has its own list of partial, free and full slabs.
 *	All object allocations for a node occur from node specific slab lists.
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
#include "biscuitos/nodemask.h"
#include "asm-generated/arch.h"
#include "asm-generated/types.h"
#include "asm-generated/semaphore.h"
#include "asm-generated/cputype.h"
#include "biscuitos/percpu.h"
#include "asm-generated/arch.h"
#include "asm-generated/percpu.h"
#include "asm-generated/smp_plat.h"
#include "asm-generated/memory.h"

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

typedef unsigned int kmem_bufctl_t_bs;
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
	unsigned short		nodeid;
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
	struct kmem_cache_bs	*cachep;
	void			*addr;
};

/*
 * struct array_cache
 *
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
	spinlock_t lock;
	void *entry[0];		/*
				 * Must have this definition in here for the
				 * proper alignment of array_cache. Also 
				 * simplifies accessing the entries.
				 * [0] is for gcc 2.95. It should really be [].
				 */
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
 * The slab lists for all objects.
 */
struct kmem_list3_bs {
	struct list_head	slabs_partial;	/* partial list first, better asm code */
	struct list_head	slabs_full;
	struct list_head	slabs_free;
	unsigned long	free_objects;
	unsigned long	next_reap;
	int		free_touched;
	unsigned int 	free_limit;
	unsigned int	colour_next;		/* Per-node cache coloring */
	spinlock_t      list_lock;
	struct array_cache_bs	*shared;	/* shared per node */
	struct array_cache_bs	**alien;	/* on other nodes */
};


/*
 * Need this for bootstrapping a per node allocator.
 */
#define NUM_INIT_LISTS_BS (2 * MAX_NUMNODES_BS + 1)
struct kmem_list3_bs __initdata initkmem_list3_bs[NUM_INIT_LISTS_BS];
#define	CACHE_CACHE_BS 0
#define	SIZE_AC_BS 1
#define	SIZE_L3_BS (1 + MAX_NUMNODES_BS)

/*
 * This function must be completely optimized away if
 * a constant is passed to it. Mostly the same as
 * what is in linux/slab.h except it returns an
 * index.
 */
static __always_inline int index_of_bs(const size_t size)
{
	extern void __bad_size_bs(void);

	if (__builtin_constant_p(size)) {
		int i = 0;

#define CACHE_BS(x) \
	if (size <=x) \
		return i; \
	else \
		i++;
#include "biscuitos/kmalloc_sizes.h"
#undef CACHE_BS
		__bad_size_bs();
	} else
		__bad_size_bs();
	return 0;
}

#define INDEX_AC_BS index_of_bs(sizeof(struct arraycache_init_bs))
#define INDEX_L3_BS index_of_bs(sizeof(struct kmem_list3_bs))

static void kmem_list3_init_bs(struct kmem_list3_bs *parent)
{
	INIT_LIST_HEAD(&parent->slabs_full);
	INIT_LIST_HEAD(&parent->slabs_partial);
	INIT_LIST_HEAD(&parent->slabs_free);
	parent->shared = NULL;
	parent->alien = NULL;
	parent->colour_next = 0;
	spin_lock_init(&parent->list_lock);
	parent->free_objects = 0;
	parent->free_touched = 0;
}

#define MAKE_LIST_BS(cachep, listp, slab, nodeid)	\
	do {	\
		INIT_LIST_HEAD(listp);		\
		list_splice(&(cachep->nodelists[nodeid]->slab), listp); \
	} while (0)

#define	MAKE_ALL_LISTS_BS(cachep, ptr, nodeid)				\
	do {								\
	MAKE_LIST_BS((cachep), (&(ptr)->slabs_full), slabs_full, nodeid); \
	MAKE_LIST_BS((cachep), (&(ptr)->slabs_partial), slabs_partial, nodeid);\
	MAKE_LIST_BS((cachep), (&(ptr)->slabs_free), slabs_free, nodeid);\
	} while (0)

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
 * struct kmem_cache_bs
 *
 * manages a cache
 */
struct kmem_cache_bs {
/* 1) per-cpu data, touched during every alloc/free */
	struct array_cache_bs	*array[NR_CPUS_BS];
	unsigned int		batchcount;
	unsigned int		limit;
	unsigned int		shared;
	unsigned int		buffer_size;
/* 2) touched by every alloc & free from the backend */
	struct kmem_list3_bs	*nodelists[MAX_NUMNODES_BS];
	unsigned int		flags;	/* constant flags */
	unsigned int		num;	/* # of objs per slab */
	spinlock_t		spinlock;

/* 3) cache_grow/shrink */
	/* order of pgs per slab (2^n) */
	unsigned int		gfporder;

	/* force GFP flags, e.g. GFP_DMA */
	gfp_t_bs		gfpflags;

	size_t			colour;		/* cache colouring range */
	unsigned int		colour_off;	/* colour offset */
	struct kmem_cache_bs	*slabp_cache;
	unsigned int		slab_size;
	unsigned int		dflags;		/* dynamic flags */

	/* constructor func */
	void (*ctor)(void *, struct kmem_cache_bs *, unsigned long);

	/* de-construct func */
	void (*dtor)(void *, struct kmem_cache_bs *, unsigned long);

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
	unsigned long		node_frees;
	atomic_t		allochit;
	atomic_t		allocmiss;
	atomic_t		freehit;
	atomic_t		freemiss;
#endif
#if DEBUG
	/*
	 * If debugging is enabled, then the allocator can add additional
	 * fields and/or padding to every object. buffer_size contains the total
	 * object size including these internal fields, the following two
	 * variables contain the offset to the user object and its size.
	 */
	int obj_offset;
	int obj_size;
#endif
};

#define CFLGS_OFF_SLAB_BS	(0x80000000UL)
#define OFF_SLAB_BS(x)		((x)->flags & CFLGS_OFF_SLAB_BS)

#define BATCHREFILL_LIMIT_BS	16

/* Optimization question: fewer reaps means less 
 * probability for unnessary cpucache drain/refill cycles.
 *
 * OTOH the cpuarrays can contain lots of objects,
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
#define	STATS_INC_NODEFREES(x)		((x)->node_frees++)
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
#define	STATS_INC_NODEFREES(x)		do { } while (0)
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

#define init_reap_node_bs(cpu) do { } while (0)
#define next_reap_node_bs(void) do { } while (0)

static void *slab_get_obj_bs(
        struct kmem_cache_bs *cachep, struct slab_bs *slabp, int nodeid);

/* Functions for storing/retrieving the cachep and or slab from the
 * global 'mem_map'. These are used to find the slab an obj belongs to.
 * With kfree(), these are used to find the cache which an obj belongs to.
 */
static inline void page_set_cache_bs(struct page_bs *page, 
					struct kmem_cache_bs *cache)
{
	page->lru.next = (struct list_head *)cache;
}

static inline struct kmem_cache_bs *page_get_cache_bs(struct page_bs *page)
{
	return (struct kmem_cache_bs *)page->lru.next;
}

static inline void page_set_slab_bs(struct page_bs *page, struct slab_bs *slab)
{
	page->lru.prev = (struct list_head *)slab;
}

static inline struct slab_bs *page_get_slab_bs(struct page_bs *page)
{
	return (struct slab_bs *)page->lru.prev;
}

static inline struct slab_bs *virt_to_slab_bs(const void *obj)
{
	struct page_bs *page = virt_to_page_bs(obj);
	return page_get_slab_bs(page);
}

static inline struct kmem_cache_bs *virt_to_cache_bs(const void *obj)
{
	struct page_bs *page = virt_to_page_bs(obj);
	return page_get_cache_bs(page);
}

static struct arraycache_init_bs initarray_cache_bs __initdata = {
	{ 0, BOOT_CPUCACHE_ENTRIES_BS, 1, 0} 
};
static struct arraycache_init_bs initarray_generic_bs = {
	{ 0, BOOT_CPUCACHE_ENTRIES_BS, 1, 0}
};

/* internal cache of cache description objs */
static struct kmem_cache_bs cache_cache_bs = {
	.batchcount	= 1,
	.limit		= BOOT_CPUCACHE_ENTRIES_BS,
	.shared		= 1,
	.buffer_size	= sizeof(struct kmem_cache_bs),
	.flags		= SLAB_NO_REAP_BS,
	.name		= "kmem_cache_bs",
#if DEBUG
	.reallen	= sizeof(struct kmem_cache_bs),
#endif
};

/* Guard access to the cache-chain. */
static DEFINE_MUTEX(cache_chain_mutex_bs);
static struct list_head		cache_chain_bs;

/*
 * vm_enough_memory() looks at this to determine how many
 * slab-allocated pages are possibly freeable under pressure
 *
 * SLAB_RECLAIM_ACCOUNT turns this on per-slab
 */
atomic_t slab_reclaim_pages_bs;

/*
 * chicken and egg problem: delay the per-cpu array allocation
 * until the general caches are up.
 */
static enum {
	NONE_BS,
	PARTIAL_AC_BS,
	PARTIAL_L3_BS,
	FULL_BS
} g_cpucache_up_bs;

static DEFINE_PER_CPU_BS(struct work_struct, reap_work_bs);

struct ccupdate_struct_bs {
	struct kmem_cache_bs *cachep;
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
static int alloc_kmemlist_bs(struct kmem_cache_bs *cachep);
static int __node_shrink_bs(struct kmem_cache_bs *cachep, int node);
static void free_block_bs(struct kmem_cache_bs *cachep,
                       void **objpp, int nr_objects, int node);

static void kmem_flagcheck_bs(struct kmem_cache_bs *cachep, gfp_t_bs flags)
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
cache_alloc_debugcheck_before_bs(struct kmem_cache_bs *cachep,
					gfp_t_bs flags)
{
	might_sleep_if(flags & __GFP_WAIT_BS);
#if DEBUG
	kmem_flagcheck_bs(cachep, flags);
#endif
}

static inline struct array_cache_bs *
cpu_cache_get_bs(struct kmem_cache_bs *cachep)
{
	return cachep->array[smp_processor_id()];
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

/* memory layout of objects:
 * 0		: objp
 * 0 .. cachep->obj_offset - BYTES_PER_WORD - 1: padding. This ensures that
 * 		the end of an object is aligned with the end of the real
 * 		allocation. Catches writes behind the end of the allocation.
 * cachep->obj_offset - BYTES_PER_WORD .. cachep->obj_offset - 1:
 * 		redzone word.
 * cachep->obj_offset: The real object.
 * cachep->buffer_size - 2* BYTES_PER_WORD: redzone word [BYTES_PER_WORD long]
 * cachep->buffer_size - 1* BYTES_PER_WORD: last caller address [BYTES_PER_WORD long]
 */

static void check_irq_off_bs(void)
{
	BUG_ON_BS(!irqs_disabled());
}

static void check_irq_on_bs(void)
{
	BUG_ON_BS(irqs_disabled());
}

static void check_spinlock_acquired_bs(struct kmem_cache_bs *cachep)
{
#ifndef CONFIG_SMP
	check_irq_off_bs();
	assert_spin_locked(&cachep->nodelists[numa_node_id_bs()]->list_lock);
#endif
}

static inline void 
check_spinlock_acquired_node_bs(struct kmem_cache_bs *cachep, int node)
{
#ifdef CONFIG_SMP
	check_irq_off();
	assert_spin_locked(&cachep->nodelists[node]->list_lock);
#endif
}

static void check_slabp_bs(struct kmem_cache_bs *cachep, struct slab_bs *slabp)
{
	kmem_bufctl_t_bs i;
	int entries = 0;

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

static int obj_size_bs(struct kmem_cache_bs *cachep)
{
	return cachep->obj_size;
}

static int obj_offset_bs(struct kmem_cache_bs *cachep)
{
	return cachep->obj_offset;
}

static void **dbg_userword_bs(struct kmem_cache_bs *cachep, void *objp)
{
	BUG_ON(!(cachep->flags & SLAB_STORE_USER_BS));
	return (void **)(objp + cachep->buffer_size-BYTES_PER_WORD_BS);
}

static unsigned long *dbg_redzone1_bs(struct kmem_cache_bs *cachep, void *objp)
{
	BUG_ON(!(cachep->flags & SLAB_RED_ZONE_BS));
	return (unsigned long *)(objp+obj_offset_bs(cachep)-BYTES_PER_WORD_BS);
}

static unsigned long *dbg_redzone2_bs(struct kmem_cache_bs *cachep, void *objp)
{
	BUG_ON(!(cachep->flags & SLAB_RED_ZONE_BS));
	if (cachep->flags & SLAB_STORE_USER_BS)
		return (unsigned long *)(objp + cachep->buffer_size - 
							2 * BYTES_PER_WORD_BS);
	return (unsigned long *)(objp + cachep->buffer_size-BYTES_PER_WORD_BS);
}

#else
#define dbg_redzone1_bs(cachep, objp)	({BUG_BS(); (unsigned long *)NULL;})
#define dbg_redzone2_bs(cachep, objp)	({BUG_BS(); (unsigned long *)NULL;})
#define dbg_userword_bs(cachep, objp)	({BUG_BS(); (void **)NULL;})
#define obj_offset_bs(x)		0
#define obj_size_bs(cachep)		(cachep->buffer_size)
#define check_irq_off_bs()		do { } while (0)
#define check_irq_on_bs()		do { } while (0)
#define check_spinlock_acquired_bs(x)	do { } while (0)
#define check_slabp_bs(x, y)		do { } while (0)
#define kfree_debugcheck_bs(x)		do { } while (0)
#define check_spinlock_acquired_node_bs(x, y) do { } while(0)
#endif

#define slab_error_bs(cachep, msg) __slab_error_bs(__FUNCTION__, cachep, msg)

static void __used __slab_error_bs(const char *function, 
				struct kmem_cache_bs *cachep, char *msg)
{
	printk(KERN_INFO "slab error in %s(): cahce '%s': %s\n",
			function, cachep->name, msg);
	dump_stack();
}

static void __used poison_obj_bs(struct kmem_cache_bs *cachep, void *addr, 
						unsigned char val)
{
	int size = obj_size_bs(cachep);

	addr = &((char *)addr)[obj_offset_bs(cachep)];

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
static void *kmem_getpages_bs(struct kmem_cache_bs *cachep,
			gfp_t_bs __nocast flags, int nodeid)
{
	struct page_bs *page;
	void *addr;
	int i;

	flags |= cachep->gfpflags;
	page = alloc_pages_node_bs(nodeid, flags, cachep->gfporder);
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
static struct slab_bs *alloc_slabmgmt_bs(struct kmem_cache_bs *cachep,
		void *objp, int colour_off, gfp_t_bs __nocast local_flags)
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

static void set_slab_attr_bs(struct kmem_cache_bs *cachep,
					struct slab_bs *slabp, void *objp)
{
	int i;
	struct page_bs *page;

	/* Nasty!!!!!! I hope this is OK */
	i = 1 << cachep->gfporder;
	page = virt_to_page_bs(objp);
	do {
		page_set_cache_bs(page, cachep);
		page_set_slab_bs(page, slabp);
		page++;
	} while (--i);
}

static void cache_init_objs_bs(struct kmem_cache_bs *cachep,
		struct slab_bs *slabp, unsigned long ctor_flags)
{
	int i;

	for (i = 0; i < cachep->num; i++) {
		void *objp = slabp->s_mem+cachep->buffer_size*i;
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
			cachep->ctor(objp+obj_offset_bs(cachep), 
							cachep, ctor_flags);

		if (cachep->flags & SLAB_RED_ZONE_BS) {
			if (*dbg_redzone2_bs(cachep, objp) != RED_INACTIVE_BS)
				slab_error_bs(cachep, "constructor overwrote "
					"the end of an object");

			if (*dbg_redzone1_bs(cachep, objp) != RED_INACTIVE_BS)
				slab_error_bs(cachep, "constructor overwrote "
					"the start of an object");
		}
		if ((cachep->buffer_size % PAGE_SIZE_BS) == 0 &&
			OFF_SLAB_BS(cachep) && cachep->flags & SLAB_POISON_BS)
			kernel_map_pages_bs(virt_to_page_bs(objp),
					cachep->buffer_size/PAGE_SIZE_BS, 0);

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
static void kmem_freepages_bs(struct kmem_cache_bs *cachep, void *addr)
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
static int cache_grow_bs(struct kmem_cache_bs *cachep,
			gfp_t_bs __nocast flags, int nodeid)
{
	struct slab_bs	*slabp;
	void		*objp;
	size_t		offset;
	gfp_t_bs	local_flags;
	unsigned long	ctor_flags;
	struct kmem_list3_bs *l3;

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

	/* Take the l3 list lock to change the colour_next on this node */
	check_irq_off_bs();
	l3 = cachep->nodelists[nodeid];
	spin_lock(&l3->list_lock);

	/* Get colour for the slab, and cal the next value. */
	offset = l3->colour_next;
	l3->colour_next++;
	if (l3->colour_next >= cachep->colour)
		l3->colour_next = 0;
	spin_unlock(&l3->list_lock);

	offset *= cachep->colour_off;

	if (local_flags & __GFP_WAIT_BS)
		local_irq_enable();

	/*
	 * The test for missing atomic flag is performed here, rather than
	 * the more obvious place, simply to reduce the critical path length
	 * in kmem_cache_alloc(). If a caller is seriously mis-behaving they
	 * will eventually be caught here (where it matters).
	 */
	kmem_flagcheck_bs(cachep, flags);

	/* Get mem for the objs.
	 * Attempt to allocate a physical page from 'nodeid',
	 */
	if (!(objp = kmem_getpages_bs(cachep, flags, nodeid)))
		goto failed;

	/* Get slab management. */
	if (!(slabp = alloc_slabmgmt_bs(cachep, objp, offset, local_flags)))
		goto opps1;

	slabp->nodeid = nodeid;
	set_slab_attr_bs(cachep, slabp, objp);

	cache_init_objs_bs(cachep, slabp, ctor_flags);

	if (local_flags & __GFP_WAIT_BS)
		local_irq_disable();
	check_irq_off_bs();
	spin_lock(&l3->list_lock);

	/* Make slab active. */
	list_add_tail(&slabp->list, &(l3->slabs_free));
	STATS_INC_GROWN_BS(cachep);
	l3->free_objects += cachep->num;
	spin_unlock(&l3->list_lock);

	return 1;
opps1:
	kmem_freepages_bs(cachep, objp);
failed:
	if (local_flags & __GFP_WAIT_BS)
		local_irq_disable();
	return 0;
}

static void *cache_alloc_refill_bs(struct kmem_cache_bs *cachep,
						 gfp_t_bs flags)
{
	int batchcount;
	struct kmem_list3_bs *l3;
	struct array_cache_bs *ac;

	check_irq_off_bs();
	ac = cpu_cache_get_bs(cachep);
retry:
	batchcount = ac->batchcount;
	if (!ac->touched && batchcount > BATCHREFILL_LIMIT_BS) {
		/* if there was little recent activity on this 
		 * cache, then perform only a partial refill.
		 * Otherwise we could generate refill bouncing.
		 */
		batchcount = BATCHREFILL_LIMIT_BS;
	}
	l3 = cachep->nodelists[numa_node_id_bs()];

	BUG_ON_BS(ac->avail > 0 || !l3);
	spin_lock(&l3->list_lock);	

	if (l3->shared) {
		struct array_cache_bs *shared_array = l3->shared;

		if (shared_array->avail) {
			if (batchcount > shared_array->avail)
				batchcount = shared_array->avail;
			shared_array->avail -= batchcount;
			ac->avail = batchcount;
			memcpy(ac->entry,
			&(shared_array->entry[shared_array->avail]),
				sizeof(void*)*batchcount);
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
			STATS_INC_ALLOCED_BS(cachep);
			STATS_INC_ACTIVE_BS(cachep);
			STATS_SET_HIGH_BS(cachep);

			ac->entry[ac->avail++] = slab_get_obj_bs(cachep, slabp,
							    numa_node_id_bs());
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
	spin_unlock(&l3->list_lock);

	if (unlikely(!ac->avail)) {
		int x;

		x = cache_grow_bs(cachep, flags, numa_node_id_bs());

		// cache_grow can reenable interrupts, then ac could change.
		ac = cpu_cache_get_bs(cachep);
		if (!x && ac->avail == 0)	// no objects in sight? abort
			return NULL;

		if (!ac->avail)	// objects refilled by interrupt?
			goto retry;
	}
	ac->touched = 1;
	return ac->entry[--ac->avail];
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

static void print_objinfo_bs(struct kmem_cache_bs *cachep, void *objp, int lines)
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
	realobj = (char*)objp+obj_offset_bs(cachep);
	size = obj_size_bs(cachep);
	for (i=0; i<size && lines;i+=16, lines--) {
		int limit;

		limit = 16;
		if (i+limit > size)
			limit = size-i;
		dump_line_bs(realobj, i, limit);
	}
}

static void check_poison_obj_bs(struct kmem_cache_bs *cachep, void *objp)
{
	char *realobj;
	int size, i;
	int lines = 0;

	realobj = (char*)objp+obj_offset_bs(cachep);
	size = obj_size_bs(cachep);

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
		struct slab_bs *slabp = page_get_slab_bs(virt_to_page_bs(objp));
		int objnr;

		objnr = (objp-slabp->s_mem)/cachep->buffer_size;
		if (objnr) {
			objp = slabp->s_mem+(objnr-1)*cachep->buffer_size;
			realobj = (char*)objp+obj_offset_bs(cachep);
			printk(KERN_INFO "Prev obj: start=%p, len=%d\n",
								realobj, size);
			print_objinfo_bs(cachep, objp, 2);
		}
		if (objnr+1 < cachep->num) {
			objp = slabp->s_mem+(objnr+1)*cachep->buffer_size;
			realobj = (char*)objp+obj_offset_bs(cachep);
			printk(KERN_INFO "Next obj: start=%p, len=%d\n",
								realobj, size);
			print_objinfo_bs(cachep, objp, 2);
		}
	}
}

static void *
cache_alloc_debugcheck_after_bs(struct kmem_cache_bs *cachep,
		gfp_t_bs flags, void *objp, void *caller)
{
	if (!objp)
		return objp;
	if (cachep->flags & SLAB_POISON_BS) {
#ifdef CONFIG_DEBUG_PAGEALLOC
		if ((cachep->buffer_size % PAGE_SIZE_BS) == 0 &&
							OFF_SLAB_BS(cachep))
			kernel_map_pages_bs(virt_to_page_bs(objp),
					cachep->buffer_size/PAGE_SIZE_BS, 1);
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
	objp += obj_offset_bs(cachep);
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

static void *cache_free_debugcheck_bs(struct kmem_cache_bs *cachep,
					void *objp, void *caller)
{
	struct page_bs *page;
	unsigned int objnr;
	struct slab_bs *slabp;

	objp -= obj_offset_bs(cachep);
	kfree_debugcheck_bs(objp);
	page = virt_to_page_bs(objp);

	if (page_get_cache_bs(page) != cachep) {
		printk(KERN_INFO "mismatch in kmem_cache_free: expected "
			"cache %p, got %p\n", page_get_cache_bs(page), cachep);
		printk(KERN_INFO "%p is %s.\n", cachep, cachep->name);
		printk(KERN_INFO "%p is %s.\n", page_get_cache_bs(page),
					page_get_cache_bs(page)->name);
		WARN_ON_BS(1);
	}
	slabp = page_get_slab_bs(page);

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

	objnr = (objp-slabp->s_mem)/cachep->buffer_size;

	BUG_ON_BS(objnr >= cachep->num);
	BUG_ON_BS(objp != slabp->s_mem + objnr*cachep->buffer_size);

	if (cachep->flags & SLAB_DEBUG_INITIAL_BS) {
		/* Need to call the slab's constructor so the
		 * caller can perform a verify of its state (debugging).
		 * Called without the cache-lock held.
		 */
		cachep->ctor(objp+obj_offset_bs(cachep),
				cachep, SLAB_CTOR_CONSTRUCTOR_BS |
					SLAB_CTOR_VERIFY_BS);
	}
	if (cachep->flags & SLAB_POISON_BS && cachep->dtor) {
		/* we want to cache poison the object,
		 * call the destruction callback
		 */
		cachep->dtor(objp+obj_offset_bs(cachep), cachep, 0);
	}
	if (cachep->flags & SLAB_POISON_BS) {
#ifdef CONFIG_DEBUG_PAGEALLOC_BS
		if ((cachep->buffer_size % PAGE_SIZE_BS) == 0 &&
						OFF_SLAB_BS(cachep)) {
			store_stackinfo_bs(cachep, objp, (unsigned long)caller);
			kernel_map_pages_bs(virt_to_page_bs(objp),
					cachep->buffer_size/PAGE_SIZE_BS, 0);
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

static inline void *____cache_alloc_bs(struct kmem_cache_bs *cachep, gfp_t_bs flags)
{
	void* objp;
	struct array_cache_bs *ac;

	check_irq_off_bs();
	ac = cpu_cache_get_bs(cachep);
	if (likely(ac->avail)) {
		STATS_INC_ALLOCHIT_BS(cachep);
		ac->touched = 1;
		objp = ac->entry[--ac->avail];
	} else {
		STATS_INC_ALLOCMISS_BS(cachep);
		objp = cache_alloc_refill_bs(cachep, flags);
	}
	return objp;
}

static inline void *__cache_alloc_bs(struct kmem_cache_bs *cachep, 
				gfp_t_bs flags, void *caller)
{
	unsigned long save_flags;
	void* objp;

	cache_alloc_debugcheck_before_bs(cachep, flags);

	local_irq_save(save_flags);
	objp = ____cache_alloc_bs(cachep, flags);
	local_irq_restore(save_flags);
	objp = cache_alloc_debugcheck_after_bs(cachep, flags, objp,
					caller);
	prefetchw(objp);
	return objp;
}

static void kmem_rcu_free_bs(struct rcu_head *head)
{
	struct slab_rcu_bs *slab_rcu = (struct slab_rcu_bs *)head;
	struct kmem_cache_bs *cachep = slab_rcu->cachep;

	kmem_freepages_bs(cachep, slab_rcu->addr);
	if (OFF_SLAB_BS(cachep))
		kmem_cache_free_bs(cachep->slabp_cache, slab_rcu);

}

static void slab_destroy_objs_bs(
			struct kmem_cache_bs *cachep, struct slab_bs *slabp)
{
	if (cachep->dtor) {
		int i;
		for (i = 0; i < cachep->num; i++) {
			void *objp = slabp->s_mem + cachep->buffer_size * i;
			(cachep->dtor) (objp, cachep, 0);
		}
	}
}

/**
 * Destroy all the objs in a slab, and release the mem back to the system.
 * Before calling the slab must have been unlinked from the cache.
 * The cache-lock is not held/needed.
 */
static void slab_destroy_bs(struct kmem_cache_bs *cachep, struct slab_bs *slabp)
{
	void *addr = slabp->s_mem - slabp->colouroff;

	slab_destroy_objs_bs(cachep, slabp);
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

static void *slab_get_obj_bs(
	struct kmem_cache_bs *cachep, struct slab_bs *slabp, int nodeid)
{
	void *objp = slabp->s_mem + (slabp->free * cachep->buffer_size);
	kmem_bufctl_t_bs next;

	slabp->inuse++;
	next = slab_bufctl_bs(slabp)[slabp->free];
#if DEBUG
	slab_bufctl_bs(slabp)[slabp->free] = BUFCTL_FREE_BS;
	WARN_ON_BS(slabp->nodeid != nodeid);
#endif
	slabp->free = next;

	return objp;
}

static void slab_put_obj_bs(
	struct kmem_cache_bs *cachep, struct slab_bs *slabp, void *objp,
                          int nodeid)
{
	unsigned int objnr = 
			(unsigned)(objp-slabp->s_mem) / cachep->buffer_size;

#if DEBUG
	/* Verify that the slab belongs to the intended node */
	WARN_ON_BS(slabp->nodeid != nodeid);

	if (slab_bufctl_bs(slabp)[objnr] != BUFCTL_FREE_BS) {
		printk(KERN_ERR "slab: double free detected in cache "
				"'%s', objp %p\n", cachep->name, objp);
		BUG_BS();
	}
#endif
	slab_bufctl_bs(slabp)[objnr] = slabp->free;
	slabp->free = objnr;
	slabp->inuse--;
}

/*
 * NUMA: different approach needed if the spinlock is moved into
 * the l3 structure.
 */
static void free_block_bs(struct kmem_cache_bs *cachep,
					void **objpp, int nr_objects, int node)
{
	int i;
	struct kmem_list3_bs *l3;

	for (i = 0; i < nr_objects; i++) {
		void *objp = objpp[i];
		struct slab_bs *slabp;

		slabp = virt_to_slab_bs(objp);
		l3 = cachep->nodelists[node];
		list_del(&slabp->list);
		check_spinlock_acquired_node_bs(cachep, node);
		check_slabp_bs(cachep, slabp);
		slab_put_obj_bs(cachep, slabp, objp, node);
		STATS_DEC_ACTIVE_BS(cachep);
		l3->free_objects++;
		check_slabp_bs(cachep, slabp);

		/* fixup slab chains */
		if (slabp->inuse == 0) {
			if (l3->free_objects > l3->free_limit) {
				l3->free_objects -= cachep->num;
				slab_destroy_bs(cachep, slabp);
			} else {
				list_add(&slabp->list,
				&l3->slabs_free);
			}
		} else {
			/* Unconditionally move a slab to the end of the
			 * partial list on free - maximum time for the
			 * other objects to be freed, too.
			 */
			list_add_tail(&slabp->list,
			&l3->slabs_partial);
		}
	}
}

static void cache_flusharray_bs(struct kmem_cache_bs *cachep,
					struct array_cache_bs *ac)
{
	int batchcount;
	struct kmem_list3_bs *l3;
	int node = numa_node_id_bs();

	batchcount = ac->batchcount;
#if DEBUG
	BUG_ON_BS(!batchcount || batchcount > ac->avail);
#endif
	check_irq_off_bs();
	l3 = cachep->nodelists[node];
	spin_lock(&l3->list_lock);
	if (l3->shared) {
		struct array_cache_bs *shared_array = l3->shared;
		int max = shared_array->limit-shared_array->avail;

		if (max) {
			if (batchcount > max)
				batchcount = max;
			memcpy(&(shared_array->entry[shared_array->avail]),
						ac->entry,
						sizeof(void *)*batchcount);
			shared_array->avail += batchcount;
			goto free_done;
		}
	}

	free_block_bs(cachep, ac->entry, batchcount, node);
free_done:
#if STATS
	{
		int i = 0;
		struct list_head *p;

		p = l3->slabs_free.next;
		while (p != &(l3->slabs_free)) {
			struct slab_bs *slabp;

			slabp = list_entry(p, struct slab_bs, list);
			BUG_ON_BS(slabp->inuse);

			i++;
			p = p->next;
		}
		STATS_SET_FREEABLE_BS(cachep, i);
	}
#endif
	spin_unlock(&l3->list_lock);
	ac->avail -= batchcount;
	memmove(ac->entry, &(ac->entry[batchcount]),
				sizeof(void *)*ac->avail);
}

/*
 * __cache_free
 * Release an obj back to its cache. If the obj has a constructed
 * state, it must be in this state _before_ it is released.
 *
 * Called with disabled ints.
 */
static inline void __cache_free_bs(struct kmem_cache_bs *cachep, void *objp)
{
	struct array_cache_bs *ac = cpu_cache_get_bs(cachep);

	check_irq_off_bs();
	objp = cache_free_debugcheck_bs(cachep, objp, 
						__builtin_return_address(0));

	if (likely(ac->avail < ac->limit)) {
		STATS_INC_FREEHIT_BS(cachep);
		ac->entry[ac->avail++] = objp;
		return;
	} else {
		STATS_INC_FREEMISS_BS(cachep);
		cache_flusharray_bs(cachep, ac);
		ac->entry[ac->avail++] = objp;
	}
}

static inline struct kmem_cache_bs *__find_general_cachep_bs(size_t size, 
								gfp_t_bs gfpflags)
{
	struct cache_sizes_bs *csizep = malloc_sizes_bs;

#if DEBUG
	/*
	 * This happens if someone tries to call
	 * kmem_cache_create(), or __kmalloc(), before
	 * the generic caches are initialized.
	 */
	BUG_ON_BS(malloc_sizes_bs[INDEX_AC_BS].cs_cachep == NULL);
#endif
	while (size > csizep->cs_size)
		csizep++;

	/*
	 * Really subtle: The last entry with cs->cs_size == ULONG_MAX
	 * has cs_{dma,}cachep==NULL. Thus no special case
	 * for large kmalloc calls required.
	 */
	if (unlikely(gfpflags & GFP_DMA_BS))
		return csizep->cs_dmacachep;
	return csizep->cs_cachep;
}

static struct array_cache_bs *alloc_arraycache_bs(int node, int entries,
							int batchcount)
{
	int memsize = sizeof(void *) * entries + sizeof(struct array_cache_bs);
	struct array_cache_bs *nc = NULL;

	nc = kmalloc_node_bs(memsize, GFP_KERNEL_BS, node);

	if (nc) {
		nc->avail = 0;
		nc->limit = entries;
		nc->batchcount = batchcount;
		nc->touched = 0;
		spin_lock_init(&nc->lock);
	}
	return nc;
}

#define drain_alien_cache_bs(cachep, l3)	do { } while (0)
#define reap_alien_bs(cachep, l3)		do { } while (0)

static inline struct array_cache_bs **alloc_alien_cache_bs(int node, int limit)
{
	return (struct array_cache_bs **) 0x01020304ul;
}

static inline void free_alien_cache_bs(struct array_cache_bs **ac_ptr)
{
}

static void do_ccupdate_local_bs(void *info)
{
	struct ccupdate_struct_bs *new = (struct ccupdate_struct_bs *)info;
	struct array_cache_bs *old;

	check_irq_off_bs();
	old = cpu_cache_get_bs(new->cachep);

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

static int do_tune_cpucache_bs(struct kmem_cache_bs *cachep, int limit, 
				int batchcount, int shared)
{
	struct ccupdate_struct_bs new;
	int i, err;

	memset(&new.new, 0, sizeof(new.new));
	for_each_online_cpu(i) {
		new.new[i] = alloc_arraycache_bs(cpu_to_node_bs(i), 
							limit, batchcount);
		if (!new.new[i]) {
			for (i--; i >= 0; i--) kfree_bs(new.new[i]);
			return -ENOMEM;
		}
	}
	new.cachep = cachep;

	smp_call_function_all_cpus_bs(do_ccupdate_local_bs, (void *)&new);

	check_irq_on_bs();
	spin_lock(&cachep->spinlock);
	cachep->batchcount = batchcount;
	cachep->limit = limit;
	cachep->shared = shared;
	spin_unlock(&cachep->spinlock);

	for_each_online_cpu(i) {
		struct array_cache_bs *ccold = new.new[i];
		if (!ccold)
			continue;
		spin_lock_irq(&cachep->nodelists[cpu_to_node(i)]->list_lock);
		free_block_bs(cachep, ccold->entry, 
					ccold->avail, cpu_to_node_bs(i));
		spin_unlock_irq(&cachep->nodelists[cpu_to_node(i)]->list_lock);
		kfree_bs(ccold);
	}

	err = alloc_kmemlist_bs(cachep);
	if (err) {
		printk(KERN_ERR "alloc_kmemlist failed for %s, error %d.\n",
				cachep->name, -err);
		BUG_BS();
	}

	return 0;
}

static void enable_cpucache_bs(struct kmem_cache_bs *cachep)
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
	if (cachep->buffer_size > 131072)
		limit = 1;
	else if (cachep->buffer_size > PAGE_SIZE_BS)
		limit = 8;
	else if (cachep->buffer_size > 1024)
		limit = 24;
	else if (cachep->buffer_size > 256)
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
	if (cachep->buffer_size <= PAGE_SIZE_BS)
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
 * If @objp is NULL, no operation is performed.
 *
 * Don't free memory not originally allocated by kmalloc()
 * or you will run into trouble. 
 */
void kfree_bs(const void *objp)
{
	struct kmem_cache_bs *c;
	unsigned long flags;

	if (unlikely(!objp))
		return;
	local_irq_save(flags);
	kfree_debugcheck_bs(objp);
	c = virt_to_cache_bs(objp);
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
void inline *__do_kmalloc_bs(size_t size, gfp_t_bs flags,
				void *caller)
{
	struct kmem_cache_bs *cachep;

	/* If you want to save a few bytes .text space: replace
	 * __ with kmem_.
	 * Then kmalloc uses the uninlined functions instead of the
	 * inline functions.
	 */
	cachep = __find_general_cachep_bs(size, flags);
	if (unlikely(cachep == NULL))
		return NULL;
	return __cache_alloc_bs(cachep, flags, caller);
}

#ifndef CONFIG_DEBUG_SLAB

void *__kmalloc_bs(size_t size, gfp_t_bs flags)
{
	return __do_kmalloc_bs(size, flags, NULL);
}
EXPORT_SYMBOL(__kmalloc_bs);

#else

void *__kmalloc_track_caller_bs(size_t size, gfp_t_bs flags, void *caller)
{
	return __do_kmalloc_bs(size, flags, caller);
}
EXPORT_SYMBOL(__kmalloc_track_caller_bs);

#endif

/**
 * kmem_cache_alloc - Allocate an object
 * @cachep: The cache to allocate from.
 * @flags: See kmalloc().
 *
 * Allocate an object from this cache.  The flags are only relevant
 * if the cache has no available objects.
 */
void *kmem_cache_alloc_bs(struct kmem_cache_bs *cachep,
				gfp_t_bs flags)
{
	return __cache_alloc_bs(cachep, flags, __builtin_return_address(0));
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
void kmem_cache_free_bs(struct kmem_cache_bs *cachep, void *objp)
{
	unsigned long flags;

	local_irq_save(flags);
	__cache_free_bs(cachep, objp);
	local_irq_restore(flags);	
}
EXPORT_SYMBOL_GPL(kmem_cache_free_bs);

struct kmem_cache_bs *kmem_find_general_cachep_bs(size_t size, gfp_t_bs gfpflags)
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
int fastcall_bs kmem_ptr_validate_bs(struct kmem_cache_bs *cachep, void *ptr)
{
	unsigned long addr = (unsigned long)ptr;
	unsigned long min_addr = PAGE_OFFSET_BS;
	unsigned long align_mask = BYTES_PER_WORD_BS - 1;
	unsigned long size = cachep->buffer_size;
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
	if (unlikely(page_get_cache_bs(page) != cachep))
		goto out;
	return 1;
out:
	return 0;
}

/* For setting up all the kmem_list3s for cache whose buffer_size is same
   as size of kmem_list3. */
static inline void set_up_list3s_bs(struct kmem_cache_bs *cachep, int index)
{
	int node;

	for_each_online_node(node) {
		cachep->nodelists[node] = &initkmem_list3_bs[index+node];
		cachep->nodelists[node]->next_reap = jiffies +
			REAPTIMEOUT_LIST3_BS +
			((unsigned long)cachep)%REAPTIMEOUT_LIST3_BS;
	}
}

/**
 * calculate_slab_order - calculate size (page order) of slabs
 * @cachep: pointer to the cache that is being created
 * @size: size of objects to be created in this cache.
 * @align: required alignment for the objects.
 * @flags: slab allocation flags
 *
 * Also calculates the number of objects per slab.
 *
 * This could be made much more intelligent.  For now, try to avoid using
 * high order pages for slabs.  When the gfp() functions are more friendly
 * towards high-order requests, this should be changed.
 */
static inline size_t calculate_slab_order_bs(struct kmem_cache_bs *cachep,
                        size_t size, size_t align, unsigned long flags)
{
	size_t left_over = 0;
	int gfporder;

	for (gfporder = 0 ; gfporder <= MAX_GFP_ORDER_BS; gfporder++) {
		unsigned int num;
		size_t remainder;

		cache_estimate_bs(gfporder, size, align, flags, 
							&remainder, &num);
		if (!num)
			continue;

		/* More than offslab_limit objects will cause problems */
		if ((flags & CFLGS_OFF_SLAB_BS) && num > offslab_limit_bs)
			break;

		/* Found something acceptable - save it away */
		cachep->num = num; 
		cachep->gfporder = gfporder;
		left_over = remainder;

		/*
		 * A VFS-reclaimable slab tends to have most allocations
		 * as GFP_NOFS and we really don't want to have to be allocating
		 * higher-order pages when we are unable to shrink dcache.
		 */
		if (flags & SLAB_RECLAIM_ACCOUNT_BS)
			break;
		/*
		 * Large number of objects is good, but very large slabs are
		 * currently bad for the gfp()s.
		 */
		if (gfporder >= slab_break_gfp_order_bs)
			break;

		/*
		 * Acceptable internal fragmentation?
		 */
		if ((left_over * 8) <= (PAGE_SIZE_BS << gfporder))
			break;
	}
	return left_over;
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
struct kmem_cache_bs *
kmem_cache_create_bs(const char *name, size_t size, size_t align,
		unsigned long flags, 
		void (*ctor)(void *, struct kmem_cache_bs *, unsigned long),
		void (*dtor)(void *, struct kmem_cache_bs *, unsigned long))
{
	size_t left_over, slab_size, ralign;
	struct kmem_cache_bs *cachep = NULL;
	struct list_head *p;

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

	mutex_lock(&cache_chain_mutex_bs);

	list_for_each(p, &cache_chain_bs) {
		struct kmem_cache_bs *pc = 
				list_entry(p, struct kmem_cache_bs, next);
		mm_segment_t old_fs = get_fs();
		char tmp;
		int res;

		/*
		 * This happens when the module gets unloaded and doesn't
		 * destroy its slab cache and no-one else reuses the vmalloc
		 * area of the module.  Print a warning.
		 */
		set_fs(KERNEL_DS);
		res = __get_user(tmp, pc->name);
		set_fs(old_fs);
		if (res) {
			printk("SLAB: cache with size %d has lost its name\n",
					pc->buffer_size);
			continue;
		}

		if (!strcmp(pc->name,name)) {
			printk("kmem_cache_create: duplicate cache %s\n", name);
			dump_stack();
			goto oops;
		}
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
	cachep = (struct kmem_cache_bs *)kmem_cache_alloc_bs(&cache_cache_bs,
					SLAB_KERNEL_BS);
	if (!cachep)
		goto oops;
	memset(cachep, 0, sizeof(struct kmem_cache_bs));

#if DEBUG
	cachep->obj_size = size;

	if (flags & SLAB_RED_ZONE_BS) {
		/* redzone only works with word aligned caches */
		align = BYTES_PER_WORD_BS;

		/* add space for red zone words */
		cachep->obj_offset += BYTES_PER_WORD_BS;
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
	if (size >= malloc_sizes[INDEX_L3_BS+1].cs_size  && 
			cachep->reallen > cache_line_size_bs() && 
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

	left_over = calculate_slab_order_bs(cachep, size, align, flags);

	if (!cachep->num) {
		printk("kmem_cache_create: couldn't create cache %s.\n", name);
		kmem_cache_free_bs(&cache_cache_bs, cachep);
		cachep = NULL;
		goto oops;
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
	cachep->buffer_size = size;

	if (flags & CFLGS_OFF_SLAB_BS) {
		cachep->slabp_cache = 
				kmem_find_general_cachep_bs(slab_size, 0u);
	}
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

			/* If the cache that's used by
			 * kmalloc(sizeof(kmem_list3)) is the first cache,
			 * then we need to set up all its list3s, otherwise
			 * the creation of further caches will BUG().
			 */
			set_up_list3s_bs(cachep, SIZE_AC_BS);
			if (INDEX_AC_BS == INDEX_L3_BS)
				g_cpucache_up_bs = PARTIAL_L3_BS;
			else
				g_cpucache_up_bs = PARTIAL_AC_BS;
		} else {
			cachep->array[smp_processor_id()] =
				kmalloc_bs(sizeof(struct arraycache_init_bs),
						GFP_KERNEL_BS);

			if (g_cpucache_up_bs == PARTIAL_AC_BS) {
				set_up_list3s_bs(cachep, SIZE_L3_BS);
				g_cpucache_up_bs = PARTIAL_L3_BS;
			} else {
				int node;
				for_each_online_node(node) {

					cachep->nodelists[node] =
						kmalloc_node_bs(
						sizeof(struct kmem_list3_bs),
							GFP_KERNEL_BS, node);
					BUG_ON_BS(!cachep->nodelists[node]);
					kmem_list3_init_bs(
						cachep->nodelists[node]);
				}
			}
		}
		cachep->nodelists[numa_node_id_bs()]->next_reap =
			jiffies + REAPTIMEOUT_LIST3_BS +
			((unsigned long)cachep)%REAPTIMEOUT_LIST3_BS;

		BUG_ON_BS(!cpu_cache_get_bs(cachep));
		cpu_cache_get_bs(cachep)->avail = 0;
		cpu_cache_get_bs(cachep)->limit = BOOT_CPUCACHE_ENTRIES_BS;
		cpu_cache_get_bs(cachep)->batchcount = 1;
		cpu_cache_get_bs(cachep)->touched = 1;
		cachep->batchcount = 1;
		cachep->limit = BOOT_CPUCACHE_ENTRIES_BS;
	}

	/* cache setup completed, link it into the list */
	list_add(&cachep->next, &cache_chain_bs);
oops:
	if (!cachep && (flags & SLAB_PANIC_BS))
		panic("kmem_cache_create(): failed to create slab '%s'\n",
					name);
	mutex_unlock(&cache_chain_mutex_bs);
	return cachep;
}
EXPORT_SYMBOL_GPL(kmem_cache_create_bs);

static size_t slab_mgmt_size_bs(size_t nr_objs, size_t align)
{
	return ALIGN(sizeof(struct slab_bs)+nr_objs * 
				sizeof(kmem_bufctl_t_bs), align);
}

/* Calculate the number of objects and left-over bytes for a given
   buffer size. */
static void cache_estimate_bs(unsigned long gfporder, size_t buffer_size,
			   size_t align, int flags, size_t *left_over,
			   unsigned int *num)
{
	int nr_objs;
	size_t mgmt_size;
	size_t slab_size = PAGE_SIZE_BS << gfporder;

	/*
	 * The slab management structure can be either off the slab or
	 * on it. For the latter case, the memory allocated for a
	 * slab is used for:
	 *
	 * - The struct slab
	 * - One kmem_bufctl_t for each object
	 * - Padding to respect alignment of @align
	 * - @buffer_size bytes for each object
	 *
	 * If the slab management structure is off the slab, then the
	 * alignment will already be calculated into the size. Because
	 * the slabs are all pages aligned, the objects will be at the
	 * correct alignment when allocated.
	 */
	if (flags & CFLGS_OFF_SLAB_BS) {
		mgmt_size = 0;
		nr_objs = slab_size / buffer_size;

		if (nr_objs > SLAB_LIMIT_BS)
			nr_objs = SLAB_LIMIT_BS;
	} else {
		/*
		 * Ignore padding for the initial guess. The padding
		 * is at most @align-1 bytes, and @buffer_size is at
		 * least @align. In the worst case, this result will
		 * be one greater than the number of objects that fit
		 * into the memory allocation when taking the padding
		 * into account.
		 */
		nr_objs = (slab_size - sizeof(struct slab_bs)) /
			  (buffer_size + sizeof(kmem_bufctl_t_bs));

		/*
		 * This calculated number will be either the right
		 * amount, or one greater than what we want.
		 */
		if (slab_mgmt_size_bs(nr_objs, align) + nr_objs*buffer_size
		       > slab_size)
			nr_objs--;

		if (nr_objs > SLAB_LIMIT_BS)
			nr_objs = SLAB_LIMIT_BS;

		mgmt_size = slab_mgmt_size_bs(nr_objs, align);
	}
	*num = nr_objs;
	*left_over = slab_size - nr_objs*buffer_size - mgmt_size;
}

static void drain_array_locked_bs(struct kmem_cache_bs *cachep,
				struct array_cache_bs *ac, int force, int node)
{
	int tofree;

	check_spinlock_acquired_node_bs(cachep, node);
	if (ac->touched && !force) {
		ac->touched = 0;
	} else if (ac->avail) {
		tofree = force ? ac->avail : (ac->limit+4)/5;
		if (tofree > ac->avail) {
			tofree = (ac->avail+1)/2;
		}
		free_block_bs(cachep, ac->entry, tofree, node);
		ac->avail -= tofree;
		memmove(ac->entry, &(ac->entry[tofree]),
					sizeof(void*)*ac->avail);
	}
}

/**
 * cache_reap - Reclaim memory from caches.
 * @unused: unused parameter
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
	struct kmem_list3_bs *l3;

	if (!mutex_trylock(&cache_chain_mutex_bs)) {
		/* Give up. Setup the next iteration */
		schedule_delayed_work(
			(struct delayed_work *)&__get_cpu_var_bs(reap_work_bs),
			REAPTIMEOUT_CPUC_BS);
	}

	list_for_each(walk, &cache_chain_bs) {
		struct kmem_cache_bs *searchp;
		struct list_head *p;
		int tofree;
		struct slab_bs *slabp;

		searchp = list_entry(walk, struct kmem_cache_bs, next);

		if (searchp->flags & SLAB_NO_REAP_BS)
			goto next;

		check_irq_on_bs();

		l3 = searchp->nodelists[numa_node_id()];
		reap_alien_bs(searchp, l3);
		spin_lock_irq(&l3->list_lock);

		drain_array_locked_bs(searchp, cpu_cache_get_bs(searchp), 0,
				numa_node_id_bs());

		if (time_after(l3->next_reap, jiffies))
			goto next_unlock;

		l3->next_reap = jiffies + REAPTIMEOUT_LIST3_BS;

		if (l3->shared)
			drain_array_locked_bs(searchp, l3->shared, 0,
				numa_node_id_bs());

		if (l3->free_touched) {
			l3->free_touched = 0;
			goto next_unlock;
		}

		tofree = (l3->free_limit+5*searchp->num-1)/(5*searchp->num);
		do {
			p = l3->slabs_free.next;
			if (p == &(l3->slabs_free))
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
			l3->free_objects -= searchp->num;
			spin_unlock_irq(&l3->list_lock);
			slab_destroy_bs(searchp, slabp);
			spin_lock_irq(&l3->list_lock);
		} while (--tofree > 0);
next_unlock:
		spin_unlock_irq(&l3->list_lock);
next:
		cond_resched();
	}
	check_irq_on_bs();
	mutex_unlock(&cache_chain_mutex_bs);
	next_reap_node_bs();
	/* Setup the next iteration */
	schedule_delayed_work(
			(struct delayed_work *)&__get_cpu_var_bs(reap_work_bs), 
			REAPTIMEOUT_CPUC_BS);
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
		init_reap_node_bs(cpu);
		INIT_WORK(reap_work, cache_reap_bs);
		schedule_delayed_work_on(cpu, 
			(struct delayed_work *)reap_work, HZ + 3 * cpu);
	}
}



static int __devinit cpuup_callback_bs(struct notifier_block *nfb,
					unsigned long action, void *hcpu)
{
	long cpu = (long)hcpu;
	struct kmem_cache_bs *cachep;
	struct kmem_list3_bs *l3 = NULL;
	int node = cpu_to_node_bs(cpu);
	int memsize = sizeof(struct kmem_list3_bs);

	switch (action) {
	case CPU_UP_PREPARE_BS:
		mutex_lock(&cache_chain_mutex_bs);
		/* we need to do this right in the beginning since
		 * alloc_arraycache's are going to use this list.
		 * kmalloc_node allows us to add the slab to the right
		 * kmem_list3 and not this cpu's kmem_list3
		 */
		list_for_each_entry(cachep, &cache_chain_bs, next) {
			/* setup the size64 kmemlist for cpu before we can
			 * begin anything. Make sure some other cpu on this
			 * node has not already allocated this
			 */
			if (!cachep->nodelists[node]) {
				if (!(l3 = kmalloc_node_bs(memsize,
						GFP_KERNEL_BS, node)))
					goto bad;
				kmem_list3_init_bs(l3);
				l3->next_reap = jiffies + REAPTIMEOUT_LIST3_BS +
				  ((unsigned long)cachep)%REAPTIMEOUT_LIST3_BS;

				/*
				 * The l3s don't come and go as CPUs come and
				 * go.  cache_chain_mutex is sufficient
				 * protection here.
				 */
				cachep->nodelists[node] = l3;
			}

			spin_lock_irq(&cachep->nodelists[node]->list_lock);
			cachep->nodelists[node]->free_limit =
				(1 + nr_cpus_node_bs(node)) *
				cachep->batchcount + cachep->num;
			spin_unlock_irq(&cachep->nodelists[node]->list_lock);
		}

		/* Now we can go ahead with allocating the shared array's
		  & array cache's */
		list_for_each_entry(cachep, &cache_chain_bs, next) {
			struct array_cache_bs *nc;
			struct array_cache_bs *shared;
			struct array_cache_bs **alien;

			nc = alloc_arraycache_bs(node, cachep->limit,
					cachep->batchcount);

			if (!nc)
				goto bad;
			shared = alloc_arraycache_bs(node,
					cachep->shared * cachep->batchcount,
					0xbaadf00d);
			if (!shared)
				goto bad;

			alien = alloc_alien_cache_bs(node, cachep->limit);
			if (!alien)
				goto bad;
			cachep->array[cpu] = nc;

			l3 = cachep->nodelists[node];
			BUG_ON(!l3);

			spin_lock_irq(&l3->list_lock);
			if (!l3->shared) {
				/*
				 * We are serialised from CPU_DEAD or
				 * CPU_UP_CANCELLED by the cpucontrol lock
				 */
				l3->shared = shared;
				shared = NULL;
			}
			spin_unlock_irq(&l3->list_lock);

			kfree_bs(shared);
			free_alien_cache_bs(alien);
		}
		mutex_unlock(&cache_chain_mutex_bs);
		break;
	case CPU_ONLINE_BS:
		start_cpu_timer_bs(cpu);
		break;
#ifdef CONFIG_HOTPLUG_CPU_BS
	case CPU_DEAD_BS:
		/*
		 * Even if all the cpus of a node are down, we don't free the
		 * kmem_list3 of any cache. This to avoid a race between
		 * cpu_down, and a kmalloc allocation from another cpu for
		 * memory from the node of the cpu going down.  The list3
		 * structure is usually allocated from kmem_cache_create() and
		 * gets destroyed at kmem_cache_destroy().
		 */
		/* fall thru */
	case CPU_UP_CANCELED_BS:
		mutex_lock(&cache_chain_mutex_bs);

		list_for_each_entry(cachep, &cache_chain_bs, next) {
			struct array_cache_bs *nc;
			struct array_cache_bs *shared;
			struct array_cache_bs **alien;
			cpumask_t mask;

			mask = node_to_cpumask_bs(node);
			spin_lock_irq(&cachep->spinlock);
			/* cpu is dead; no one can alloc from it. */
			nc = cachep->array[cpu];
			cachep->array[cpu] = NULL;
			l3 = cachep->nodelists[node];

			if (!l3)
				goto free_array_cache;

			spin_lock_irq(&l3->list_lock);

			/* Free limit for this kmem_list3 */
			l3->free_limit -= cachep->batchcount;
			if (nc)
				free_block_bs(cachep, nc->entry, 
							nc->avail, node);

			if (!cpus_empty(mask)) {
                                spin_unlock_irq(&l3->list_lock);
                                goto free_array_cache;
                        }

			shared = l3->shared;
			if (shared) {
				free_block_bs(cachep, l3->shared->entry,
						l3->shared->avail, node);
				l3->shared = NULL;
			}

			alien = l3->alien;
			l3->alien = NULL;

			spin_unlock_irq(&l3->list_lock);

			kfree_bs(shared);
			if (align) {
				drain_alien_cache_bs(cachep, alien);
				free_alien_cache_bs(alien);
			}
free_array_cache:
			kfree_bs(nc);
		}
		/*
		 * In the previous loop, all the objects were freed to
		 * the respective cache's slabs,  now we can go ahead and
		 * shrink each nodelist to its limit.
		 */
		list_for_each_entry(cachep, &cache_chain_bs, next) {
			l3 = cachep->nodelists[node];
			if (!l3)
				continue;
			spin_lock_irq(&l3->list_lock);
			/* free slabs belonging to this node */
			__node_shrink_bs(cachep, node);
			spin_unlock_irq(&l3->list_lock);
		}
		mutex_unlock(&cache_chain_mutex_bs);
		break;
#endif
	}
	return NOTIFY_OK;
      bad:
	mutex_unlock(&cache_chain_mutex_bs);
	return NOTIFY_BAD;
}

static void do_drain_bs(void *arg)
{
	struct kmem_cache_bs *cachep = (struct kmem_cache_bs *)arg;
	struct array_cache_bs *ac;
	int node = numa_node_id_bs();

	check_irq_off_bs();
	ac = cpu_cache_get_bs(cachep);
	spin_lock(&cachep->nodelists[node]->list_lock);
	free_block_bs(cachep, ac->entry, ac->avail, node);
	spin_unlock(&cachep->nodelists[node]->list_lock);
	ac->avail = 0;
}

static void drain_cpu_caches_bs(struct kmem_cache_bs *cachep)
{
	struct kmem_list3_bs *l3;
	int node;

	smp_call_function_all_cpus_bs(do_drain_bs, cachep);
	check_irq_on_bs();
	for_each_online_node_bs(node)  {
		l3 = cachep->nodelists[node];
		if (l3) {
			spin_lock_irq(&l3->list_lock);
			drain_array_locked_bs(cachep, l3->shared, 1, node);
			spin_unlock_irq(&l3->list_lock);
			if (l3->alien)
				drain_alien_cache_bs(cachep, l3->alien);
		}
	}
}

static int __node_shrink_bs(struct kmem_cache_bs *cachep, int node)
{
	struct slab_bs *slabp;
	struct kmem_list3_bs *l3 = cachep->nodelists[node];
	int ret;

	for (;;) {
		struct list_head *p;

		p = l3->slabs_free.prev;
		if (p == &l3->slabs_free)
			break;

		slabp = list_entry(l3->slabs_free.prev, struct slab_bs, list);
#if DEBUG
		if (slabp->inuse)
			BUG();
#endif
		list_del(&slabp->list);

		l3->free_objects -= cachep->num;
		spin_unlock_irq(&l3->list_lock);
		slab_destroy_bs(cachep, slabp);
		spin_lock_irq(&l3->list_lock);
	}
	ret = !list_empty(&l3->slabs_full) ||
		!list_empty(&l3->slabs_partial);
	return ret;
}

static int __cache_shrink_bs(struct kmem_cache_bs *cachep)
{
	int ret = 0, i = 0;
	struct kmem_list3_bs *l3;

	drain_cpu_caches_bs(cachep);

	check_irq_on_bs();
	for_each_online_node_bs(i) {
		l3 = cachep->nodelists[i];
		if (l3) {
			spin_lock_irq(&l3->list_lock);
			ret += __node_shrink_bs(cachep, i);
			spin_unlock_irq(&l3->list_lock);
		}
	}
	return (ret ? 1 : 0);
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
int kmem_cache_destroy_bs(struct kmem_cache_bs *cachep)
{
	int i;
	struct kmem_list3_bs *l3;

	if (!cachep || in_interrupt())
		BUG_BS();

	/* Don't let CPUs to come and go */
	//lock_cpu_hotplug();

	/* Find the cache in the chain of caches. */
	mutex_lock(&cache_chain_mutex_bs);

	/*
	 * the chain is never empty, cache_cache is never destroyed
	 */
	list_del(&cachep->next);
	mutex_unlock(&cache_chain_mutex_bs);

	if (__cache_shrink_bs(cachep)) {
		slab_error_bs(cachep, "Can't free all objects");
		mutex_lock(&cache_chain_mutex_bs);
		list_add(&cachep->next, &cache_chain_bs);
		mutex_unlock(&cache_chain_mutex_bs);
		//unlock_cpu_hotplug();
		return 1;
	}

	if (unlikely(cachep->flags & SLAB_DESTROY_BY_RCU_BS))
		synchronize_rcu();

	for_each_online_cpu(i)
		kfree_bs(cachep->array[i]);

	/* NUMA: free the list3 structures */
	for_each_online_node_bs(i) {
		if ((l3 = cachep->nodelists[i])) {
			kfree_bs(l3->shared);
			free_alien_cache_bs(l3->alien);
			kfree_bs(l3);
		}
	}

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
int kmem_cache_shrink_bs(struct kmem_cache_bs *cachep)
{
	if (!cachep || in_interrupt())
		BUG_BS();

	return __cache_shrink_bs(cachep);
}
EXPORT_SYMBOL_GPL(kmem_cache_shrink_bs);

static __unused struct notifier_block cpucache_notifier_bs = 
				{ &cpuup_callback_bs, NULL, 0};

/*
 * swap the static kmem_list3 with kmalloced memory
 */
static void init_list_bs(struct kmem_cache_bs *cachep, 
				struct kmem_list3_bs *list, int nodeid)
{
	struct kmem_list3_bs *ptr;

	BUG_ON_BS(cachep->nodelists[nodeid] != list);
	ptr = kmalloc_node_bs(sizeof(struct kmem_list3_bs), 
						GFP_KERNEL_BS, nodeid);
	BUG_ON_BS(!ptr);

	local_irq_disable();
	memcpy(ptr, list, sizeof(struct kmem_list3_bs));
	MAKE_ALL_LISTS_BS(cachep, ptr, nodeid);
	cachep->nodelists[nodeid] = ptr;
	local_irq_enable();
}

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
	int i;
	int order;

	for (i = 0; i < NUM_INIT_LISTS_BS; i++) {
		kmem_list3_init_bs(&initkmem_list3_bs[i]);
		if (i < MAX_NUMNODES_BS)
			cache_cache_bs.nodelists[i] = NULL;
	}

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
	 *    Initially an __init data area is used for the head array and the
	 *    kmem_list3 structures, it's replaced with a kmalloc allocated
	 *    array at the end of the bootstrap.
	 * 2) Create the first kmalloc cache.
	 *    The kmem_cache_t for the new cache is allocated normally.
	 *    An __init data area is used for the head array.
	 * 3) Create the remaining kmalloc caches, with minimally sized
	 *    head arrays.
	 * 4) Replace the __init data head arrays for cache_cache and the first
	 *    kmalloc cache with kmalloc allocated arrays.
	 * 5) Replace the __init data for kmem_list3 for cache_cache and
	 *    the other cache's with kmalloc allocated memory.
	 * 6) Resize the head arrays of the kmalloc caches to their final sizes.
	 */

	/* 1) create the cache_cache */
	spin_lock_init(&cache_cache_bs.spinlock);
	INIT_LIST_HEAD(&cache_chain_bs);
	list_add(&cache_cache_bs.next, &cache_chain_bs);
	cache_cache_bs.colour_off = cache_line_size_bs();
	cache_cache_bs.array[smp_processor_id()] = &initarray_cache_bs.cache;
	cache_cache_bs.nodelists[numa_node_id_bs()] = 
					&initkmem_list3_bs[CACHE_CACHE_BS];
	cache_cache_bs.buffer_size = ALIGN(cache_cache_bs.buffer_size, 
						cache_line_size_bs());

	for (order = 0; order < MAX_ORDER_BS; order++) {
		cache_estimate_bs(order, cache_cache_bs.buffer_size,
			cache_line_size_bs(), 0, &left_over, 
			&cache_cache_bs.num);
		if (cache_cache_bs.num)
			break;
	}
	if (!cache_cache_bs.num)
		BUG_BS();

	cache_cache_bs.gfporder = order;
	cache_cache_bs.colour = left_over / cache_cache_bs.colour_off;
	cache_cache_bs.slab_size = 
		ALIGN(cache_cache_bs.num * sizeof(kmem_bufctl_t_bs) + 
		sizeof(struct slab_bs), cache_line_size_bs());

	/* 2+3) Create the kmalloc caches */
	sizes = malloc_sizes_bs;
	names = cache_names_bs;

	/* Initialize the caches that provide memory for the array cache
	 * and the kmem_list3 structures first.
	 * Without this, further allocations will bug
	 */

	sizes[INDEX_AC_BS].cs_cachep = kmem_cache_create_bs(
			names[INDEX_AC_BS].name,
			sizes[INDEX_AC_BS].cs_size, 
			ARCH_KMALLOC_MINALIGN_BS,
			(ARCH_KMALLOC_FLAGS_BS | SLAB_PANIC_BS),
			NULL,
			NULL);

	if (INDEX_AC_BS != INDEX_L3_BS)
		sizes[INDEX_L3_BS].cs_cachep = kmem_cache_create_bs(
			names[INDEX_L3_BS].name,
			sizes[INDEX_L3_BS].cs_size,
			ARCH_KMALLOC_MINALIGN_BS,
			(ARCH_KMALLOC_FLAGS_BS | SLAB_PANIC_BS),
			NULL,
			NULL);


	while (sizes->cs_size != ULONG_MAX) {
		/*
		 * For performance, all the general caches are L1 aligned.
		 * This should be particularly beneficial on SMP boxes, as it
		 * eliminates "false sharing"
		 * Note for sytems short on memory removing the alignment will
		 * allow tighter packing of the smaller caches.
		 **/
		if(!sizes->cs_cachep)
			sizes->cs_cachep = kmem_cache_create_bs(
						names->name,
						sizes->cs_size, 
						ARCH_KMALLOC_MINALIGN_BS,
						(ARCH_KMALLOC_FLAGS_BS | 
						SLAB_PANIC_BS),
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
		BUG_ON_BS(cpu_cache_get_bs(&cache_cache_bs) != 
						&initarray_cache_bs.cache);
		memcpy(ptr, cpu_cache_get_bs(&cache_cache_bs),
					sizeof(struct arraycache_init_bs));
		cache_cache_bs.array[smp_processor_id()] = ptr;
		local_irq_enable();

		ptr = kmalloc_bs(sizeof(struct arraycache_init_bs),
							GFP_KERNEL_BS);
		local_irq_disable();
		BUG_ON_BS(cpu_cache_get_bs(
				malloc_sizes_bs[INDEX_AC_BS].cs_cachep) !=
						&initarray_generic_bs.cache);
		memcpy(ptr, cpu_cache_get_bs(
				malloc_sizes_bs[INDEX_AC_BS].cs_cachep),
					sizeof(struct arraycache_init_bs));
		malloc_sizes_bs[INDEX_AC_BS].cs_cachep->array[
					smp_processor_id()] = ptr;
		local_irq_enable();
	}

	/* 5) Replace the bootstrap kmem_list3's */
	{
		int node;
		/* Replace the static kmem_list3 structures for the boot cpu */
		init_list_bs(&cache_cache_bs, 
			&initkmem_list3_bs[CACHE_CACHE_BS], numa_node_id_bs());

		for_each_online_node_bs(node) {
			init_list_bs(malloc_sizes_bs[INDEX_AC_BS].cs_cachep,
				&initkmem_list3_bs[SIZE_AC_BS+node], node);

			if (INDEX_AC_BS != INDEX_L3_BS) {
				init_list_bs(
					malloc_sizes_bs[INDEX_L3_BS].cs_cachep,
					&initkmem_list3_bs[SIZE_L3_BS+node],
					node);
			}
		}
	}

	/* 6) resize the head arrays to their final sizes */
	{
		struct kmem_cache_bs *cachep;
		mutex_lock(&cache_chain_mutex_bs);
		list_for_each_entry(cachep, &cache_chain_bs, next)
			enable_cpucache_bs(cachep);
		mutex_unlock(&cache_chain_mutex_bs);
	}

	/* Done! */
	g_cpucache_up_bs = FULL_BS;

	/* Register a cpu startup notifier callback
	 * that initializes cpu_cache_get for all new cpus
	 */
	register_cpu_notifier(&cpucache_notifier_bs);

	/* The reap timers are started later, with a module init call:
	 * That part of the kernel is not yet operational.
	 */

	/* FIXME: slab_initcall entry, used to debug slab,
	 * This code isn't default code */
	DEBUG_CALL(slab);
}

static int __init __unused cpucache_init_bs(void)
{
	int cpu;

	/* 
	 * Register the timers that return unneeded
	 * pages to gfp.
	 */
	for_each_online_cpu(cpu)
		start_cpu_timer_bs(cpu);

	return 0;
}
__initcall_bs(cpucache_init_bs);

#ifdef CONFIG_SMP
/**
 * __alloc_percpu - allocate one copy of the object for every present
 * cpu in the system, zeroing them.
 * Objects should be dereferenced using the per_cpu_ptr macro only.
 *
 * @size: how many bytes of memory are required.
 * @align: the alignment, which can't be greater than SMP_CACHE_BYTES.
 */
void *__alloc_percpu_bs(size_t size)
{
	int i;
	struct percpu_data_bs *pdata = 
			kmalloc_bs(sizeof (*pdata), GFP_KERNEL_BS);

	if (!pdata)
		return NULL;

	/*
	 * Cannot use for_each_online_cpu since a cpu may come online
	 * and we have no way of figuring out how to fix the array
	 * that we have allocated then....
	 */
	for_each_possible_cpu(i) {
		int node = cpu_to_node(i);

		if (node_online(node))
			pdata->ptrs[i] = kmalloc_node_bs(size, 
							GFP_KERNEL_BS, node);
		else
			pdata->ptrs[i] = kmalloc_bs(size, GFP_KERNEL_BS);

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
EXPORT_SYMBOL_GPL(__alloc_percpu_bs);

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

/**
 * ksize - get the actual amount of memory allocated for a given object
 * @objp: Pointer to the object
 *
 * kmalloc may internally round up allocations and return more memory
 * than requested. ksize() can be used to determine the actual amount of
 * memory allocated. The caller may use this additional memory, even though
 * a smaller amount of memory was initially specified with the kmalloc call.
 * The caller must guarantee that objp points to a valid object previously
 * allocated with either kmalloc() or kmem_cache_alloc(). The object
 * must not be freed during the duration of the call.
 */
unsigned int ksize_bs(const void *objp)
{
	if (unlikely(objp == NULL))
		return 0;

	return obj_size_bs(virt_to_cache_bs(objp));
}


static void print_slabinfo_header_bs(struct seq_file *m)
{
	/*
	 * Output format version, so at least we can change it
	 * without _too_ many complaints.
	 */
#if STATS
	seq_puts(m, "slabinfo - version: 2.1 (statistics)\n");
#else
	seq_puts(m, "slabinfo - version: 2.1\n");
#endif
	seq_puts(m, "# name            <active_objs> <num_objs> <objsize> "
		 "<objperslab> <pagesperslab>");
	seq_puts(m, " : tunables <limit> <batchcount> <sharedfactor>");
	seq_puts(m, " : slabdata <active_slabs> <num_slabs> <sharedavail>");
#if STATS
	seq_puts(m, " : globalstat <listallocs> <maxobjs> <grown> <reaped> "
		 "<error> <maxfreeable> <nodeallocs> <remotefrees>");
	seq_puts(m, " : cpustat <allochit> <allocmiss> <freehit> <freemiss>");
#endif
	seq_putc(m, '\n');
}

static void *s_start_bs(struct seq_file *m, loff_t *pos)
{
	loff_t n = *pos;
	struct list_head *p;

	mutex_lock(&cache_chain_mutex_bs);
	if (!n)
		print_slabinfo_header_bs(m);
	p = cache_chain_bs.next;
	while (n--) {
		p = p->next;
		if (p == &cache_chain_bs)
			return NULL;
	}
	return list_entry(p, struct kmem_cache_bs, next);
}

static void *s_next_bs(struct seq_file *m, void *p, loff_t *pos)
{
	struct kmem_cache_bs *cachep = p;
	++*pos;
	return cachep->next.next == &cache_chain_bs ? NULL :
		list_entry(cachep->next.next, struct kmem_cache_bs, next);
}

static void s_stop_bs(struct seq_file *m, void *p)
{
	mutex_unlock(&cache_chain_mutex_bs);
}

static int s_show_bs(struct seq_file *m, void *p)
{
	struct kmem_cache_bs *cachep = p;
	struct list_head *q;
	struct slab_bs *slabp;
	unsigned long active_objs;
	unsigned long num_objs;
	unsigned long active_slabs = 0;
	unsigned long num_slabs, free_objects = 0, shared_avail = 0;
	const char *name;
	char *error = NULL;
	int node;
	struct kmem_list3_bs *l3;

	spin_lock(&cachep->spinlock);
	active_objs = 0;
	num_slabs = 0;
	for_each_online_node_bs(node) {
		l3 = cachep->nodelists[node];
		if (!l3)
			continue;

		check_irq_on_bs();
		spin_lock_irq(&l3->list_lock);

		list_for_each(q,&l3->slabs_full) {
			slabp = list_entry(q, struct slab_bs, list);
			if (slabp->inuse != cachep->num && !error)
				error = "slabs_full accounting error";
			active_objs += cachep->num;
			active_slabs++;
		}
		list_for_each(q,&l3->slabs_partial) {
			slabp = list_entry(q, struct slab_bs, list);
			if (slabp->inuse == cachep->num && !error)
				error = "slabs_partial inuse accounting error";
			if (!slabp->inuse && !error)
				error = "slabs_partial/inuse accounting error";
			active_objs += slabp->inuse;
			active_slabs++;
		}
		list_for_each(q,&l3->slabs_free) {
			slabp = list_entry(q, struct slab_bs, list);
			if (slabp->inuse && !error)
				error = "slabs_free/inuse accounting error";
			num_slabs++;
		}
		free_objects += l3->free_objects;
		if (l3->shared)
			shared_avail += l3->shared->avail;

		spin_unlock_irq(&l3->list_lock);
	}
	num_slabs += active_slabs;
	num_objs = num_slabs * cachep->num;
	if (num_objs - active_objs != free_objects && !error)
		error = "free_objects accounting error";

	name = cachep->name;
	if (error)
		printk(KERN_ERR "slab: cache %s error: %s\n", name, error);

	seq_printf(m, "%-17s %6lu %6lu %6u %4u %4d",
		name, active_objs, num_objs, cachep->buffer_size,
		cachep->num, (1<<cachep->gfporder));
	seq_printf(m, " : tunables %4u %4u %4u",
		cachep->limit, cachep->batchcount,
		cachep->shared);
	seq_printf(m, " : slabdata %6lu %6lu %6lu",
		active_slabs, num_slabs, shared_avail);

#if STATS
	{	/* list3 stats */
		unsigned long high = cachep->high_mark;
		unsigned long allocs = cachep->num_allocations;
		unsigned long grown = cachep->grown;
		unsigned long reaped = cachep->reaped;
		unsigned long errors = cachep->errors;
		unsigned long max_freeable = cachep->max_freeable;
		unsigned long node_allocs = cachep->node_allocs;
		unsigned long node_frees = cachep->node_frees;

		seq_printf(m, " : globalstat %7lu %6lu %5lu %4lu %4lu "
				"%4lu %4lu %4lu",
			allocs, high, grown, reaped, errors,
			max_freeable, node_allocs, node_frees);
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
	spin_unlock(&cachep->spinlock);
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

/*
 * This initializes kmem_list3 for all nodes.
 */
static int alloc_kmemlist_bs(struct kmem_cache_bs *cachep)
{
	int node;
	struct kmem_list3_bs *l3;
	int err = 0;

	for_each_online_node_bs(node) {
		struct array_cache_bs *nc = NULL, *new;
		struct array_cache_bs **new_alien = NULL;
#ifdef CONFIG_NUMA
		if (!(new_alien = alloc_alien_cache_bs(node, cachep->limit)))
			goto fail;
#endif
		if (!(new = alloc_arraycache_bs(node, (cachep->shared*
				cachep->batchcount), 0xbaadf00d)))
			goto fail;
		if ((l3 = cachep->nodelists[node])) {

			spin_lock_irq(&l3->list_lock);

			if ((nc = cachep->nodelists[node]->shared))
				free_block_bs(cachep, nc->entry,
							nc->avail, node);

			l3->shared = new;
			if (!cachep->nodelists[node]->alien) {
				l3->alien = new_alien;
				new_alien = NULL;
			}
			l3->free_limit = (1 + nr_cpus_node_bs(node))*
				cachep->batchcount + cachep->num;
			spin_unlock_irq(&l3->list_lock);
			kfree_bs(nc);
			free_alien_cache_bs(new_alien);
			continue;
		}
		if (!(l3 = kmalloc_node_bs(sizeof(struct kmem_list3_bs),
						GFP_KERNEL_BS, node)))
			goto fail;

		kmem_list3_init_bs(l3);
		l3->next_reap = jiffies + REAPTIMEOUT_LIST3_BS +
			((unsigned long)cachep)%REAPTIMEOUT_LIST3_BS;
		l3->shared = new;
		l3->alien = new_alien;
		l3->free_limit = (1 + nr_cpus_node_bs(node))*
			cachep->batchcount + cachep->num;
		cachep->nodelists[node] = l3;
	}
	return err;
fail:
	err = -ENOMEM;
	return err;
}
#endif
