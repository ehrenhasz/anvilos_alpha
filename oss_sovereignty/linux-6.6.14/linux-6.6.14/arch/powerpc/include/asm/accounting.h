#ifndef __POWERPC_ACCOUNTING_H
#define __POWERPC_ACCOUNTING_H
struct cpu_accounting_data {
	unsigned long utime;
	unsigned long stime;
#ifdef CONFIG_ARCH_HAS_SCALED_CPUTIME
	unsigned long utime_scaled;
	unsigned long stime_scaled;
#endif
	unsigned long gtime;
	unsigned long hardirq_time;
	unsigned long softirq_time;
	unsigned long steal_time;
	unsigned long idle_time;
	unsigned long starttime;	 
	unsigned long starttime_user;	 
#ifdef CONFIG_ARCH_HAS_SCALED_CPUTIME
	unsigned long startspurr;	 
	unsigned long utime_sspurr;	 
#endif
};
#endif
