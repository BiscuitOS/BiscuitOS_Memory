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
DEBUG_DATA_T(percpu);
DEBUG_DATA_T(buddy);
DEBUG_DATA_T(pcp);
DEBUG_DATA_T(slab);
DEBUG_DATA_T(vmalloc);
DEBUG_DATA_T(module);
DEBUG_DATA_T(kmap);
DEBUG_DATA_T(fixmap);
DEBUG_DATA_T(login);

#define __used		__attribute__((__used__))
#define __unused	__attribute__((__unused__))

/* FIXME: BiscuitOS initall */

#define bootmem_initcall_bs(fn)						\
	static bootmem_entry_t_bs __bootmem_entry_##fn			\
	__attribute__((__used__))					\
	__attribute__((__section__(".bootmem_data_bs"))) = fn

#define percpu_initcall_bs(fn)						\
	static percpu_entry_t_bs __percpu_entry_##fn			\
	__attribute__((__used__))					\
	__attribute__((__section__(".percpu_data_bs"))) = fn

#define buddy_initcall_bs(fn)						\
	static buddy_entry_t_bs __buddy_entry_##fn			\
	__attribute__((__used__))					\
	__attribute__((__section__(".buddy_data_bs"))) = fn

#define pcp_initcall_bs(fn)						\
	static pcp_entry_t_bs __pcp_entry_##fn				\
	__attribute__((__used__))					\
	__attribute__((__section__(".pcp_data_bs"))) = fn

#define slab_initcall_bs(fn)						\
	static slab_entry_t_bs __slab_entry_##fn			\
	__attribute__((__used__))					\
	__attribute__((__section__(".slab_data_bs"))) = fn

#define vmalloc_initcall_bs(fn)						\
	static vmalloc_entry_t_bs __vmalloc_entry_##fn			\
	__attribute__((__used__))					\
	__attribute__((__section__(".vmalloc_data_bs"))) = fn

#define module_initcall_bs(fn)						\
	static module_entry_t_bs __module_entry_##fn			\
	__attribute__((__used__))					\
	__attribute__((__section__(".module_data_bs"))) = fn

#define kmap_initcall_bs(fn)						\
	static kmap_entry_t_bs __kmap_entry_##fn			\
	__attribute__((__used__))					\
	__attribute__((__section__(".kmap_data_bs"))) = fn

#define fixmap_initcall_bs(fn)						\
	static fixmap_entry_t_bs __fixmap_entry_##fn			\
	__attribute__((__used__))					\
	__attribute__((__section__(".fixmap_data_bs"))) = fn

#define login_initcall_bs(fn)						\
	static login_entry_t_bs __login_entry_##fn			\
	__attribute__((__used__))					\
	__attribute__((__section__(".login_data_bs"))) = fn

struct obs_kernel_param_bs {
	const char *str;
	int (*setup_func)(char *);
	int early;
};

/*      
 * Only for really core code.  See moduleparam.h for the normal way.
 *      
 * Force the alignment so the compiler doesn't space elements of the
 * obs_kernel_param "array" too far apart in .init.setup.
 */
#define __setup_param_bs(str, unique_id, fn, early)			\
	static char __setup_str_##unique_id_bs[] __initdata = str;	\
	static struct obs_kernel_param_bs __setup_##unique_id_bs	\
		__attribute__((__used__))				\
		__attribute__((__section__(".init.setup_bs")))		\
		__attribute__((aligned((sizeof(long)))))		\
		= { __setup_str_##unique_id_bs, fn, early }

#define __setup_bs(str, fn)						\
	__setup_param_bs(str, fn, fn, 0)				\

#define __devinit		__init
#define __initcall_bs(x)	

#endif
