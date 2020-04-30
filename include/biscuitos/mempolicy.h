#ifndef _BISCUITOS_MEMPOLICY_H
#define _BISCUITOS_MEMPOLICY_H
/*
 * NUMA memory policies for Linux.
 * Copyright 2003,2004 Andi Kleen SuSE Labs
 */

struct shared_policy_bs {};

static inline void mpol_free_shared_policy_bs(struct shared_policy_bs *p)
{
}

static inline void mpol_shared_policy_init_bs(struct shared_policy_bs *p)
{
}

#endif
