#ifndef _BISCUITOS_ARM_SEMAPHORE_H
#define _BISCUITOS_ARM_SEMAPHORE_H
/*
 * Copyright (c) 2008 Intel Corporation
 * Author: Matthew Wilcox <willy@linux.intel.com>
 *
 * Distributed under the terms of the GNU GPL, version 2
 *
 * Please see kernel/locking/semaphore.c for documentation of these functions
 */
#include <linux/semaphore.h>

static inline void init_MUTEX_bs(struct semaphore *sem)
{
	sema_init(sem, 1);
}

#endif
