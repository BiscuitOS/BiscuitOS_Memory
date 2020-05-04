/*
 *  linux/mm/oom_kill.c
 * 
 *  Copyright (C)  1998,2000  Rik van Riel
 *      Thanks go out to Claus Fischer for some serious inspiration and
 *      for goading me into coding this file...
 *
 *  The routines in this file are used to kill a process when
 *  we're seriously out of memory. This gets called from kswapd()
 *  in linux/mm/vmscan.c when we really run out of memory.
 *
 *  Since we won't call these routines often (on a well-configured
 *  machine) this file will double as a 'coding guide' and a signpost
 *  for newbie kernel hackers. It features several pointers to major
 *  kernel subsystems and hints as to where to find out what things do.
 */

/**
 * oom_kill - kill the "best" process when we run out of memory
 *
 * If we run out of memory, we have the choice between either
 * killing a random task (bad), letting the system crash (worse)
 * OR try to be smart about which process to kill. Note that we
 * don't have to be perfect here, we just have to be good.
 */
void out_of_memory_bs(unsigned int __nocast gfp_mask, int order)
{
}
