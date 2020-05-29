

#include <linux/kernel.h>
#include <linux/proc_fs.h>
#include "biscuitos/kernel.h"
#include "biscuitos/init.h"

extern struct seq_operations vmstat_op_bs;
extern struct seq_operations fragmentation_op_bs;
extern struct seq_operations slabinfo_op_bs;
extern struct seq_operations zoneinfo_op_bs;

static int __init init_mm_internals_bs(void)
{
#ifdef CONFIG_PROC_FS
	proc_create_seq("vmstat_bs", 0444, NULL, &vmstat_op_bs);
	proc_create_seq("buddyinfo_bs", 0444, NULL, &fragmentation_op_bs);
#ifndef CONFIG_SLOB_BS
	proc_create_seq("slabinfo_bs", 0444, NULL, &slabinfo_op_bs);
#endif
	proc_create_seq("zoneinfo_bs", 0444, NULL, &zoneinfo_op_bs);
#endif
	return 0;
}
module_initcall_bs(init_mm_internals_bs);
