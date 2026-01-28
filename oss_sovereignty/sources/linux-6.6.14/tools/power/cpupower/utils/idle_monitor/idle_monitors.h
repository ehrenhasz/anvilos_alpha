


#ifndef _CPUIDLE_IDLE_MONITORS_H_
#define _CPUIDLE_IDLE_MONITORS_H_

#define DEF(x) extern struct cpuidle_monitor x ##_monitor;
#include "idle_monitors.def"
#undef DEF
extern struct cpuidle_monitor *all_monitors[];

#endif 
