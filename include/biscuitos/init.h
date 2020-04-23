#ifndef _BISCUITOS_INIT_H
#define _BISCUITOS_INIT_H

/* Defined in init/main.c */
extern char saved_command_line_bs[];

/*
 * Used for initiailzation calls...
 */
typedef int (*bootmem_entry_t_bs)(void);

/* FIXME: BiscuitOS initall */

#define bootmem_initcall_bs(fn)						\
	static bootmem_entry_t_bs __bootmem_entry_##fn			\
	__attribute__((__used__))					\
	__attribute__((__section__(".bootmem_data_bs"))) = fn

#endif
