/*
 * mm/thrash.c
 *
 * Copyright (C) 2004, Red Hat, Inc.
 * Copyright (C) 2004, Rik van Riel <riel@redhat.com>
 * Released under the GPL, see the file COPYING for details.
 *
 * Simple token based thrashing protection, using the algorithm
 * described in:  http://www.cs.wm.edu/~sjiang/token.pdf
 */
#include <linux/kernel.h>

#define SWAP_TOKEN_TIMEOUT_BS		0
/*
 * Currently disabled; Needs further code to work at HZ * 300.
 */     
unsigned long swap_token_default_timeout_bs = SWAP_TOKEN_TIMEOUT_BS;
