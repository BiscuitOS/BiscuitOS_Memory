#ifndef _BISCUITOS_MM_H
#define _BISCUITOS_MM_H

#include "asm-generated/pgtable.h"
#include "asm-generated/page.h"
#include "asm-generated/memory.h"

extern void *high_memory_bs;
extern pgprot_t_bs protection_map_bs[16];

#endif
