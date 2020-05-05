#ifndef _BISCUITOS_SLAB_H
#define _BISCUITOS_SLAB_H

typedef struct kmem_cache_s_bs kmem_cache_t_bs;

#include <linux/compiler.h>
#include "biscuitos/gfp.h"
#include "biscuitos/types.h"

/* flags for kmem_cache_alloc() */
#define SLAB_NOFS_BS		GFP_NOFS_BS
#define SLAB_NOIO_BS		GFP_NOIO_BS
#define SLAB_ATOMIC_BS		GFP_ATOMIC_BS
#define SLAB_USER_BS		GFP_USER_BS
#define SLAB_KERNEL_BS		GFP_KERNEL_BS
#define SLAB_DMA_BS		GFP_DMA_BS

#define SLAB_LEVEL_MASK_BS	GFP_LEVEL_MASK_BS
#define SLAB_NO_GROW_BS		__GFP_NO_GROW_BS  /* don't grow a cache */

/* flags to pass to kmem_cache_create().
 * The first 3 are only valid when the allocator as been build
 * SLAB_DEBUG_SUPPORT.
 */
#define SLAB_DEBUG_INITIAL_BS	0x00000200UL	/* Call constructor (as verifier) */
#define SLAB_RED_ZONE_BS	0x00000400UL	/* Red zone objs in a cache */

#define SLAB_NO_REAP_BS		0x00001000UL	/* never reap from the cache */
#define SLAB_POISON_BS		0x00000800UL	/* Poison objects */
#define SLAB_NO_REEAP_BS	0x00001000UL	/* never reap from the cache */
#define SLAB_HWCACHE_ALIGN_BS	0x00002000UL	/* align objs on a h/w cache lines */
#define SLAB_CACHE_DMA_BS	0x00004000UL	/* use GFP_DMA memory */
#define SLAB_MUST_HWCACHE_ALIGN_BS \
				0x00008000UL	/* force alignment */
#define SLAB_STORE_USER_BS	0x00010000UL	/* store the last owner for bug hunting */
#define SLAB_RECLAIM_ACCOUNT_BS	0x00020000UL	/* track pages allocated to indicate what is reclaimable later */
#define SLAB_PANIC_BS		0x00040000UL	/* panic if kmem_cache_create() fails */
#define SLAB_DESTROY_BY_RCU_BS	0x00080000UL	/* defer freeing pages to RCU */

/* flags passed to a constructor func */
#define SLAB_CTOR_CONSTRUCTOR_BS	0x001UL	/* if not set, then deconstructor */
#define SLAB_CTOR_ATOMIC_BS		0x002UL	/* tell constructor it can't sleep */
#define SLAB_CTOR_VERIFY_BS		0x004UL	/* tell constructor it's a verify call */

/* Size description struct for general caches. */
struct cache_sizes_bs {
	size_t		cs_size;
	kmem_cache_t_bs	*cs_cachep;
	kmem_cache_t_bs	*cs_dmacachep;
};

extern void __init kmem_cache_init_bs(void);

void *kmem_cache_alloc_bs(kmem_cache_t_bs *, gfp_t_bs);
void kmem_cache_free_bs(kmem_cache_t_bs *cachep, void *objp);
kmem_cache_t_bs *kmem_find_general_cachep_bs(size_t size, gfp_t_bs gfpflags);
extern struct cache_sizes_bs malloc_sizes_bs[];
extern void *__kmalloc_bs(size_t, gfp_t_bs);
void kfree_bs(const void *objp);
kmem_cache_t_bs *
kmem_cache_create_bs(const char *name, size_t size, size_t align,
		unsigned long flags,
		void (*ctor)(void *, kmem_cache_t_bs *, unsigned long),
		void (*dtor)(void *, kmem_cache_t_bs *, unsigned long));
extern int kmem_cache_destroy_bs(kmem_cache_t_bs *cachep);
extern int kmem_cache_shrink_bs(kmem_cache_t_bs *cachep);
extern void *kzalloc_bs(size_t, gfp_t);

/**
 * kcalloc - allocate memory for an array. The memory is set to zero.
 * @n: number of elements.
 * @size: element size.
 * @flags: the type of memory to allocate.
 */
static inline void *kcalloc_bs(size_t n, size_t size, gfp_t flags)
{
	if (n != 0 && size > INT_MAX / n)
		return NULL;
	return kzalloc_bs(n * size, flags);
}

static inline void *kmalloc_bs(size_t size, gfp_t_bs flags)
{
	if (__builtin_constant_p(size)) {
		int i = 0;
#define CACHE_BS(x)						\
		if (size <= x)					\
			goto found;				\
		else						\
			i++;
#include "biscuitos/kmalloc_sizes.h"
#undef CACHE_BS
found:
		return kmem_cache_alloc_bs((flags & GFP_DMA_BS) ?
			malloc_sizes_bs[i].cs_dmacachep :
			malloc_sizes_bs[i].cs_cachep, flags);
	}
	return __kmalloc_bs(size, flags);
}

static inline void *kmalloc_node_bs(size_t size, gfp_t_bs flags, int node)
{
	return kmalloc_bs(size, flags);
}

static inline void *kmem_cache_alloc_node_bs(kmem_cache_t_bs *cachep,
				int flags, int node)
{
	return kmem_cache_alloc_bs(cachep, flags);
}

extern int FASTCALL_BS(kmem_ptr_validate_bs(kmem_cache_t_bs *, void *));
#endif
