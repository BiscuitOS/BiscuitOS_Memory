#ifndef _BISCUITOS_MEMPOOL_H
#define _BISCUITOS_MEMPOOL_H

#include <linux/wait.h>

typedef void * (mempool_alloc_t_bs)(unsigned int __nocast gfp_mask, void *pool_data);
typedef void (mempool_free_t_bs)(void *element, void *pool_data);

typedef struct mempool_s_bs {
	spinlock_t lock;
	int min_nr;		/* nr of elements at *elements */
	int curr_nr;		/* Current nr of elements at *elements */
	void **elements;

	void *pool_data;
	mempool_alloc_t_bs *alloc;
	mempool_free_t_bs *free;
	wait_queue_head_t wait;
} mempool_t_bs;

/*
 * A mempool_alloc_t and mempool_free_t that get the memory from
 * a slab that is passed in through pool_data.
 */
void *mempool_alloc_slab_bs(unsigned int __nocast gfp_mask, void *pool_data);
void mempool_free_slab_bs(void *element, void *pool_data);
void mempool_free_bs(void *element, mempool_t_bs *pool);
void *mempool_alloc_bs(mempool_t_bs *pool, unsigned int __nocast gfp_mask);
void mempool_destroy_bs(mempool_t_bs *pool);
mempool_t_bs *mempool_create_bs(int min_nr, mempool_alloc_t_bs *alloc_fn,
			mempool_free_t_bs *free_fn, void *pool_data);
mempool_t_bs *mempool_create_node_bs(int min_nr, mempool_alloc_t_bs *alloc_fn,
			mempool_free_t_bs *free_fn, void *pool_data, int nid);
int mempool_resize_bs(mempool_t_bs *, int, unsigned int __nocast);

#endif
