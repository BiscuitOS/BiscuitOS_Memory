/*
 *  linux/mm/mempool.c
 *
 *  memory buffer pool support. Such pools are mostly used
 *  for guaranteed, deadlock-free memory allocations during
 *  extreme VM load.
 *
 *  started by Ingo Molnar, Copyright (C) 2001
 */
#include <linux/kernel.h>
#include <linux/sched.h>
#include "biscuitos/kernel.h"
#include "biscuitos/mm.h"
#include "biscuitos/slab.h"
#include "biscuitos/mempool.h"

static void add_element_bs(mempool_t_bs *pool, void *element)
{
	BUG_ON_BS(pool->curr_nr >= pool->min_nr);
	pool->elements[pool->curr_nr++] = element;
}

static void *remove_element_bs(mempool_t_bs *pool)
{
	BUG_ON_BS(pool->curr_nr <= 0);
	return pool->elements[--pool->curr_nr];
}

static void free_pool_bs(mempool_t_bs *pool)
{
	while (pool->curr_nr) {
		void *element = remove_element_bs(pool);

		pool->free(element, pool->pool_data);
	}
	kfree_bs(pool->elements);
	kfree_bs(pool);
}

/**
 * mempool_create - create a memory pool
 * @min_nr:    the minimum number of elements guaranteed to be
 *             allocated for this pool.
 * @alloc_fn:  user-defined element-allocation function.
 * @free_fn:   user-defined element-freeing function.
 * @pool_data: optional private data available to the user-defined functions.
 *
 * this function creates and allocates a guaranteed size, preallocated
 * memory pool. The pool can be used from the mempool_alloc and mempool_free
 * functions. This function might sleep. Both the alloc_fn() and the free_fn()
 * functions might sleep - as long as the mempool_alloc function is not called
 * from IRQ contexts.
 */
mempool_t_bs *mempool_create_bs(int min_nr, mempool_alloc_t_bs *alloc_fn,
		mempool_free_t_bs *free_fn, void *pool_data)
{
	mempool_t_bs *pool;

	pool = kmalloc_bs(sizeof(*pool), GFP_KERNEL_BS);
	if (!pool)
		return NULL;
	memset(pool, 0, sizeof(*pool));
	pool->elements = kmalloc_bs(min_nr * sizeof(void *), GFP_KERNEL_BS);
	if (!pool->elements) {
		kfree_bs(pool);
		return NULL;
	}
	spin_lock_init(&pool->lock);
	pool->min_nr = min_nr;
	pool->pool_data = pool_data;
	init_waitqueue_head(&pool->wait);
	pool->alloc = alloc_fn;
	pool->free = free_fn;

	/*
	 * First pre-allocate the guaranteed number of buffers.
	 */
	while (pool->curr_nr < pool->min_nr) {
		void *element;

		element = pool->alloc(GFP_KERNEL_BS, pool->pool_data);
		if (unlikely(!element)) {
			free_pool_bs(pool);
			return NULL;
		}
		add_element_bs(pool, element);
	}
	return pool;
}
EXPORT_SYMBOL_GPL(mempool_create_bs);
/**
 * mempool_resize - resize an existing memory pool
 * @pool:       pointer to the memory pool which was allocated via
 *              mempool_create().
 * @new_min_nr: the new minimum number of elements guaranteed to be
 *              allocated for this pool.
 * @gfp_mask:   the usual allocation bitmask.
 *
 * This function shrinks/grows the pool. In the case of growing,
 * it cannot be guaranteed that the pool will be grown to the new
 * size immediately, but new mempool_free() calls will refill it.
 *
 * Note, the caller must guarantee that no mempool_destroy is called
 * while this function is running. mempool_alloc() & mempool_free()
 * might be called (eg. from IRQ contexts) while this function executes.
 */
int mempool_resize_bs(mempool_t_bs *pool, int new_min_nr, gfp_t_bs gfp_mask)
{
	void *element;
	void **new_elements;
	unsigned long flags;

	BUG_ON_BS(new_min_nr <= 0);

	spin_lock_irqsave(&pool->lock, flags);
	if (new_min_nr <= pool->min_nr) {
		while (new_min_nr < pool->curr_nr) {
			element = remove_element_bs(pool);
			spin_unlock_irqrestore(&pool->lock, flags);
			pool->free(element, pool->pool_data);
			spin_lock_irqsave(&pool->lock, flags);
		}
		pool->min_nr = new_min_nr;
		goto out_unlock;
	}
	spin_unlock_irqrestore(&pool->lock, flags);

	/* Grow the pool */
	new_elements = kmalloc_bs(new_min_nr * sizeof(*new_elements), 
								gfp_mask);
	if (!new_elements)
		return -ENOMEM;

	spin_lock_irqsave(&pool->lock, flags);
	if (unlikely(new_min_nr <= pool->min_nr)) {
		/* Raced, other resize will do our work */
		spin_unlock_irqrestore(&pool->lock, flags);
		kfree_bs(new_elements);
		goto out;
	}
	memcpy(new_elements, pool->elements,
			pool->curr_nr * sizeof(*new_elements));
	kfree_bs(pool->elements);
	pool->elements = new_elements;
	pool->min_nr = new_min_nr;

	while (pool->curr_nr < pool->min_nr) {
		spin_unlock_irqrestore(&pool->lock, flags);
		element = pool->alloc(gfp_mask, pool->pool_data);
		if (!element)
			goto out;
		spin_lock_irqsave(&pool->lock, flags);
		if (pool->curr_nr < pool->min_nr) {
			add_element_bs(pool, element);
		} else {
			spin_unlock_irqrestore(&pool->lock, flags);
			pool->free(element, pool->pool_data); /* Raced */
			goto out;
		}
	}
out_unlock:
	spin_unlock_irqrestore(&pool->lock, flags);
out:
	return 0;
}
EXPORT_SYMBOL_GPL(mempool_resize_bs);

/**
 * mempool_destroy - deallocate a memory pool
 * @pool:      pointer to the memory pool which was allocated via
 *             mempool_create().
 *
 * this function only sleeps if the free_fn() function sleeps. The caller
 * has to guarantee that all elements have been returned to the pool (ie:
 * freed) prior to calling mempool_destroy().
 */
void mempool_destroy_bs(mempool_t_bs *pool)
{
	if (pool->curr_nr != pool->min_nr)
		BUG_BS();	/* There were outstanding elements */
	free_pool_bs(pool);
}
EXPORT_SYMBOL_GPL(mempool_destroy_bs);

/**
 * mempool_alloc - allocate an element from a specific memory pool
 * @pool:      pointer to the memory pool which was allocated via
 *             mempool_create().
 * @gfp_mask:  the usual allocation bitmask.
 *
 * this function only sleeps if the alloc_fn function sleeps or
 * returns NULL. Note that due to preallocation, this function
 * *never* fails when called from process contexts. (it might
 * fail if called from an IRQ context.)
 */
void *mempool_alloc_bs(mempool_t_bs *pool, gfp_t_bs gfp_mask)
{
	void *element;
	unsigned long flags;
	DEFINE_WAIT(wait);
	gfp_t_bs gfp_temp;

	might_sleep_if(gfp_mask & __GFP_WAIT_BS);

	gfp_mask |= __GFP_NOMEMALLOC_BS; /* don't allocate emergency reserves */
	gfp_mask |= __GFP_NORETRY_BS; /* don't loop in __alloc_pages */
	gfp_mask |= __GFP_NOWARN_BS; /* failures are OK */

	gfp_temp = gfp_mask & ~(__GFP_WAIT_BS | __GFP_IO_BS);

repeat_alloc:

	element = pool->alloc(gfp_temp, pool->pool_data);
	if (likely(element != NULL))
		return element;

	spin_lock_irqsave(&pool->lock, flags);
	if (likely(pool->curr_nr)) {
		element = remove_element_bs(pool);
		spin_unlock_irqrestore(&pool->lock, flags);
		return element;
	}
	spin_unlock_irqrestore(&pool->lock, flags);

	/* We must not sleep in the GFP_ATOMIC case */
	if (!(gfp_mask & __GFP_WAIT_BS))
		return NULL;

	/* Now start performing page reclaim */
	gfp_temp = gfp_mask;
	prepare_to_wait(&pool->wait, &wait, TASK_UNINTERRUPTIBLE);
	smp_mb();
	if (!pool->curr_nr)
		io_schedule();
	finish_wait(&pool->wait, &wait);

	goto repeat_alloc;
}
EXPORT_SYMBOL_GPL(mempool_alloc_bs);

/**
 * mempool_free - return an element to the pool.
 * @element:   pool element pointer.
 * @pool:      pointer to the memory pool which was allocated via
 *             mempool_create().
 *
 * this function only sleeps if the free_fn() function sleeps.
 */
void mempool_free_bs(void *element, mempool_t_bs *pool)
{
	unsigned long flags;

	smp_mb();
	if (pool->curr_nr < pool->min_nr) {
		spin_lock_irqsave(&pool->lock, flags);
		if (pool->curr_nr < pool->min_nr) {
			add_element_bs(pool, element);
			spin_unlock_irqrestore(&pool->lock, flags);
			wake_up(&pool->wait);
			return;
		}
		spin_unlock_irqrestore(&pool->lock, flags);
	}
	pool->free(element, pool->pool_data);
}
EXPORT_SYMBOL_GPL(mempool_free_bs);

/*
 * A commonly used alloc and free fn
 */
void *mempool_alloc_slab_bs(gfp_t_bs gfp_mask, void *pool_data)
{
	kmem_cache_t_bs *mem = (kmem_cache_t_bs *)pool_data;
	return kmem_cache_alloc_bs(mem, gfp_mask);
}
EXPORT_SYMBOL_GPL(mempool_alloc_slab_bs);

void mempool_free_slab_bs(void *element, void *pool_data)
{
	kmem_cache_t_bs *mem = (kmem_cache_t_bs *)pool_data;
	kmem_cache_free_bs(mem, element);
}
EXPORT_SYMBOL_GPL(mempool_free_slab_bs);
