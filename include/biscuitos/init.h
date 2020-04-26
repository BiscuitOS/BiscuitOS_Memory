#ifndef _BISCUITOS_INIT_H
#define _BISCUITOS_INIT_H

/* Defined in init/main.c */
extern char saved_command_line_bs[];

#define DEBUG_DATA_T(name)						\
	typedef int (*name##_entry_t_bs)(void)

#define DEBUG_FUNC_T(name)						\
static void inline name##_debug_entry(void)				\
{									\
	name##_entry_t_bs *call;					\
	extern name##_entry_t_bs name##_section_start_bs[];		\
	extern name##_entry_t_bs name##_section_end_bs[];		\
									\
	for (call = name##_section_start_bs; 				\
			call < name##_section_end_bs; call++)		\
		(*call)();						\
}

#define DEBUG_CALL(name)	name##_debug_entry()

/*
 * Used for initiailzation calls...
 */
DEBUG_DATA_T(bootmem);
DEBUG_DATA_T(buddy);
DEBUG_DATA_T(slab);
DEBUG_DATA_T(vmalloc);

/* FIXME: BiscuitOS initall */

#define bootmem_initcall_bs(fn)						\
	static bootmem_entry_t_bs __bootmem_entry_##fn			\
	__attribute__((__used__))					\
	__attribute__((__section__(".bootmem_data_bs"))) = fn

#define buddy_initcall_bs(fn)						\
	static buddy_entry_t_bs __buddy_entry_##fn			\
	__attribute__((__used__))					\
	__attribute__((__section__(".buddy_data_bs"))) = fn

#define slab_initcall_bs(fn)						\
	static slab_entry_t_bs __slab_entry_##fn			\
	__attribute__((__used__))					\
	__attribute__((__section__(".slab_data_bs"))) = fn

#define vmalloc_initcall_bs(fn)						\
	static vmalloc_entry_t_bs __vmalloc_entry_##fn			\
	__attribute__((__used__))					\
	__attribute__((__section__(".vmalloc_data_bs"))) = fn

#endif
