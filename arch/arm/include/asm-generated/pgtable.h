#ifndef _BISCUITOS_ARM_PGTABLE_H
#define _BISCUITOS_ARM_PGTABLE_H

#define PTRS_PER_PGD		2048

/*
 * These are used to make use of C type-checking
 */
typedef struct { unsigned long pte; } pte_t;
typedef struct { unsigned long pmd; } pmd_t;
typedef struct { unsigned long pgd[2]; } pgd_t;
typedef struct { unsigned long pgprot; } pgprot_t;

extern phys_addr_t swapper_pg_dir_bs;

#endif
