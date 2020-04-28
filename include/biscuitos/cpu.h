#ifndef _BISCUITOS_CPU_H
#define _BISCUITOS_CPU_H
/*
 * include/linux/cpu.h - generic cpu definition
 *
 * This is mainly for topological representation. We define the 
 * basic 'struct cpu' here, which can be embedded in per-arch 
 * definitions of processors.
 *
 * Basic handling of the devices is done in drivers/base/cpu.c
 * and system devices are handled in drivers/base/sys.c. 
 *       
 * CPUs are exported via driverfs in the class/cpu/devices/
 * directory. 
 *
 * Per-cpu interfaces can be implemented using a struct device_interface. 
 * See the following for how to do this: 
 * - drivers/base/intf.c 
 * - Documentation/driver-model/interface.txt
 */

#define CPU_ONLINE_BS		0x0002 /* CPU (unsigned)v is up */
#define CPU_UP_PREPARE_BS	0x0003 /* CPU (unsigned)v coming up */
#define CPU_UP_CANCELED_BS	0x0004 /* CPU (unsigned)v NOT coming up */
#define CPU_DOWN_PREPARE_BS	0x0005 /* CPU (unsigned)v going down */
#define CPU_DOWN_FAILED_BS	0x0006 /* CPU (unsigned)v NOT going down */
#define CPU_DEAD_BS		0x0007 /* CPU (unsigned)v dead */

#define register_cpu_notifier(x)	do { } while (0)

#endif
