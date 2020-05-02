#ifndef _BISCUITOS_PRIO_TREE_H
#define _BISCUITOS_PRIO_TREE_H

struct prio_tree_node_bs {
	struct prio_tree_node_bs *left;
	struct prio_tree_node_bs *right;
	struct prio_tree_node_bs *parent;
	unsigned long start;
	unsigned long last; /* last location _in_ interval */
};

struct prio_tree_root_bs {
	struct prio_tree_node_bs *prio_tree_node;
	unsigned short index_bits;
	unsigned short raw;
		/*
		 * 0: nodes are of type struct prio_tree_node
		 * 1: nodes are of type raw_prio_tree_node
		 */
};

#endif
