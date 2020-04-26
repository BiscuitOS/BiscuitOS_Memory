

#include <linux/kernel.h>
#include <linux/proc_fs.h>
#include "biscuitos/kernel.h"
#include "biscuitos/init.h"

extern struct seq_operations vmstat_op_bs;

static int __init init_mm_internals_bs(void)
{
#ifdef CONFIG_PROC_FS
	proc_create_seq("vmstat_bs", 0444, NULL, &vmstat_op_bs);
#endif
}
module_initcall_bs(init_mm_internals_bs);
