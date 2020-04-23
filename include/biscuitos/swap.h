#ifndef _BISCUITOS_SWAP_H
#define _BISCUITOS_SWAP_H

/*      
 * current->reclaim_state points to one of these when a task is running
 * memory reclaim
 */ 
struct reclaim_state_bs {
	unsigned long reclaimed_slab;
};

extern unsigned long totalram_pages_bs;
extern unsigned long totalhigh_pages_bs;

extern unsigned int nr_free_pages_bs(void);

#endif
