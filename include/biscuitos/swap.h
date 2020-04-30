#ifndef _BISCUITOS_SWAP_H
#define _BISCUITOS_SWAP_H

#define SWAP_CLUSTER_MAX_BS	32

/*
 * Per process flags
 */
#define PF_ALIGNWARN_BS		0x00000001 /* Print alignment warning msgs */
					/* Not implemented yet, only for 486*/
#define PF_STARTING_BS		0x00000002 /* being created */
#define PF_EXITING_BS		0x00000004 /* getting shut down */
#define PF_DEAD_BS		0x00000008 /* Dead */
#define PF_FORKNOEXEC_BS	0x00000040 /* forked but didn't exec */
#define PF_SUPERPRIV_BS		0x00000100 /* used super-user privileges */
#define PF_DUMPCORE_BS		0x00000200 /* dumped core */
#define PF_SIGNALED_BS		0x00000400 /* killed by a signal */
#define PF_MEMALLOC_BS		0x00000800 /* Allocating memory */
#define PF_FLUSHER_BS		0x00001000 /* responsible for disk writeback */
#define PF_USED_MATH_BS		0x00002000 /* if unset the fpu must be */
					   /* initialized before use */
#define PF_FREEZE_BS		0x00004000 /* this task is being frozen for */
					   /* suspend now */
#define PF_NOFREEZE_BS		0x00008000 /* this thread should not be */
					   /* frozen */
#define PF_FROZEN_BS		0x00010000 /* frozen for system suspend */
#define PF_FSTRANS_BS		0x00020000 /* inside a filesystem transaction */
#define PF_KSWAPD_BS		0x00040000 /* I am kswapd */
#define PF_SWAPOFF_BS		0x00080000 /* I am in swapoff */
#define PF_LESS_THROTTLE_BS	0x00100000 /* Throttle me less: I clean */
					   /* memory */
#define PF_SYNCWRITE_BS		0x00200000 /* I am doing a sync write */
#define PF_BORROWED_MM_BS	0x00400000 /* I am a kthread doing use_mm */
#define PF_RANDOMIZE_BS		0x00800000 /* randomize virtual address space */

/*      
 * current->reclaim_state points to one of these when a task is running
 * memory reclaim
 */ 
struct reclaim_state_bs {
	unsigned long reclaimed_slab;
};

/* A swap entry has to fit into a "unsigned long", as
 * the entry is hidden in the "index" field of the
 * swapper address space.
 */
typedef struct {
	unsigned long val;
} swp_entry_t_bs;

#define swap_token_default_timeout_bs		0
#define has_swap_token_bs(x)			0

extern unsigned long totalram_pages_bs;
extern unsigned long totalhigh_pages_bs;

extern unsigned int nr_free_pages_bs(void);
extern void out_of_memory_bs(unsigned int __nocast gfp_mask);
extern void __init swap_setup_bs(void);
struct zone_bs;
extern int try_to_free_pages_bs(struct zone_bs **zones, 
			unsigned int gfp_mask, unsigned int order);

extern void lru_add_drain_bs(void);

#endif
