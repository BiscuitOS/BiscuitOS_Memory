#ifndef _BISCUITOS_NODEMASK_H
#define _BISCUITOS_NODEMASK_H

#include <linux/bitmap.h>
#include "asm-generated/arch.h"
#include "asm-generated/types.h"

typedef struct { DECLARE_BITMAP(bits, MAX_NUMNODES_BS); } nodemask_t_bs;

extern nodemask_t_bs node_online_map_bs;
extern nodemask_t_bs node_possible_map_bs;

/* FIXME: better would be to fix all architectures to never return
	> MAX_NUMNODES_BS, then the silly min_ts could be dropped. */
#define first_node_bs(src)	__first_node_bs(&(src))
static inline int __first_node_bs(const nodemask_t_bs *srcp)
{
	return min_t(int, MAX_NUMNODES_BS, 
			find_first_bit(srcp->bits, MAX_NUMNODES_BS));
}

#define next_node_bs(n, src)	__next_node_bs((n), &(src))
static inline int __next_node_bs(int n, const nodemask_t_bs *srcp)
{
	return min_t(int, MAX_NUMNODES_BS,
			find_next_bit(srcp->bits, MAX_NUMNODES_BS, n+1));
}

#define nodes_empty_bs(src)	__nodes_empty_bs(&(src), MAX_NUMNODES_BS)
static inline int __nodes_empty_bs(const nodemask_t_bs *srcp, int nbits)
{
	return bitmap_empty(srcp->bits, nbits);
}

#define nodes_weight_bs(nodemask)				\
			__nodes_weight_bs(&(nodemask), MAX_NUMNODES_BS)
static inline int __nodes_weight_bs(const nodemask_t_bs *srcp, int nbits)
{
	return bitmap_weight(srcp->bits, nbits);
}

#define nodes_clear_bs(dst) __nodes_clear_bs(&(dst), MAX_NUMNODES_BS)
static inline void __nodes_clear_bs(nodemask_t_bs *dstp, int nbits)
{
	bitmap_zero(dstp->bits, nbits);
}

/* No static inline type checking - see Subtlety (1) above */
#define node_isset_bs(node, nodemask)	test_bit((node), (nodemask).bits)

#if MAX_NUMNODES_BS > 1
#define for_each_node_mask_bs(node, mask)			\
	for ((node) = first_node_bs(mask);			\
		(node) < MAX_NUMNODES_BS;			\
		(node) = next_node_bs((node), (mask)))

#define num_online_nodes_bs()	nodes_weight_bs(node_online_map_bs)
#define node_online_bs(node)	node_isset_bs((node), node_online_map_bs)

#else /* MAX_NUMNODES_BS == 1 */
#define for_each_node_mask_bs(node, mask)			\
	if (!nodes_empty_bs(mask))				\
		for ((node) = 0; (node) < 1; (node)++)

#define num_online_nodes_bs()	1
#define node_online_bs(node)	1

#endif /* MAX_NUMNODES_BS */


#define node_set_online_bs(node)				\
		set_bit((node), node_online_map_bs.bits)
#define for_each_online_node_bs(node)				\
		for_each_node_mask_bs((node), node_online_map_bs)
#define for_each_node_bs(node)					\
		for_each_node_mask_bs((node), node_possible_map_bs)

#define NODE_MASK_LAST_WORD_BS BITMAP_LAST_WORD_MASK(MAX_NUMNODES_BS)

#if MAX_NUMNODES_BS <= BITS_PER_LONG_BS

#define NODE_MASK_ALL_BS						\
((nodemask_t_bs) { {							\
	[BITS_TO_LONGS(MAX_NUMNODES_BS)-1] = NODE_MASK_LAST_WORD_BS	\
}})

#else

#define NODE_MASK_ALL_BS						\
((nodemask_t_bs) { {							\
        [0 ... BITS_TO_LONGS(MAX_NUMNODES_BS)-2] = ~0UL,\
        [BITS_TO_LONGS(MAX_NUMNODES_BS)-1] = NODE_MASK_LAST_WORD_BS	\
} })

#endif

#endif
