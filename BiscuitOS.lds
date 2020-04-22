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
	}
	. = ALIGN(16);
	/* percpu */
	.percpu_bs : {
		__per_cpu_start_bs = .;
			*(.data.percpu_bs)
		__per_cpu_end_bs = .;
	}
	. = ALIGN(16);
}
