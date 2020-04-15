#ifndef _BISCUITOS_KERNEL_H
#define _BISCUITOS_KERNEL_H

#define BS_DUP()  printk("Expand..[%s-%s-%d]\n", __FILE__, __func__, __LINE__)
#define BS_DONE() printk("Done..[%s-%s-%d]\n", __FILE__, __func__, __LINE__)

#endif
