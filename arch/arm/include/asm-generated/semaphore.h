#ifndef _BISCUITOS_ARM_SEMAPHORE_H
#define _BISCUITOS_ARM_SEMAPHORE_H

#include <linux/wait.h>

struct semaphore_bs {
	atomic_t count;
	int sleepers;
	wait_queue_head_t wait;
};

static inline void sema_init_bs(struct semaphore_bs *sem, int val)
{
	atomic_set(&sem->count, val);
	sem->sleepers = 0;
	init_waitqueue_head(&sem->wait);
}

static inline void init_MUTEX_bs(struct semaphore_bs *sem)
{
	sema_init_bs(sem, 1);
}

#endif
