/*
 * BiscuitOS Memory-Manager LD Scripts
 *
 * (C) 2020.04.20 BuddyZhang1 <buddy.zhang@aliyun.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
OUTPUT_ARCH(arm)
SECTIONS {
	/* Early param section */
	.early_param_bs 0 : {
		__early_begin_bs = .;
			*(__early_param_bs)
		__early_end_bs = .;
		__start___param_bs = .;
		__setup_start_bs = .;
			*(.init.setup_bs)
		__setup_end_bs = .;
		__stop___param_bs = .;
	}

	. = ALIGN(16);
	/* bootmem */
	.bootmem_bs 0 : {
		bootmem_section_start_bs = .;
			*(.bootmem_data_bs)
		bootmem_section_end_bs = .;
	}

	. = ALIGN(16);
	/* percpu */
	.percpu_bs : {
		__per_cpu_start_bs = .;
			*(.data.percpu_bs)
		__per_cpu_end_bs = .;
	}

	. = ALIGN(16);
	/* Buddy */
	.buddy_bs : {
		buddy_section_start_bs = .;
			*(.buddy_data_bs)
		buddy_section_end_bs = .;
	}

	. = ALIGN(16);
	/* Slab */
	.slab_bs : {
		slab_section_start_bs = .;
			*(.slab_data_bs)
		slab_section_end_bs = .;
	}

	. = ALIGN(16);
	/* Vmalloc */
	.vmalloc_bs : {
		vmalloc_section_start_bs = .;
			*(.vmalloc_data_bs)
		vmalloc_section_end_bs = .;
	}

	. = ALIGN(16);
	/* Init call */
	.init_bs 0 : {
		module_section_start_bs = .;
			*(.module_data_bs)
		module_section_end_bs = .;
		login_section_start_bs = .;
			*(.login_data_bs)
		login_section_end_bs = .;
	}
}
