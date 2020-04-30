#ifndef _BISCUITOS_SCHED_H
#define _BISCUITOS_SCHED_H

#define MAX_USER_RT_PRIO_BS	100
#define MAX_RT_PRIO_BS		MAX_USER_RT_PRIO_BS
#define MAX_PRIO_BS		(MAX_RT_PROT_BS + 40)
#define rt_task_bs(p)		(unlikely((p)->prio < MAX_RT_PRIO_BS))

static inline void refrigerator_bs(unsigned long flag) {}

#endif
