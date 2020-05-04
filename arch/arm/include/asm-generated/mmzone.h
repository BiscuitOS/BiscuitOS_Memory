#ifndef _BISCUITOS_ARM_MMZONE_H
#define _BISCUITOS_ARM_MMZONE_H

/*
 * NODE_MEM_MAP gives the kaddr for the mem_map of the node.
 */
#define NODE_MEM_MAP_BS(nid)       (NODE_DATA_BS(nid)->node_mem_map)

#endif
