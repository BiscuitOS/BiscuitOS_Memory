#ifndef _BISCUITOS_MM_H
#define _BISCUITOS_MM_H

#include "asm-generated/page.h"
#include "asm-generated/memory.h"
#include "asm-generated/pgtable.h"

extern void *high_memory_bs;
extern pgprot_t protection_map_bs[16];

#endif
