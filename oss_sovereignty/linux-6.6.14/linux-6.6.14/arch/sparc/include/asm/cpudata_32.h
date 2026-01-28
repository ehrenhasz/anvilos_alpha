#ifndef _SPARC_CPUDATA_H
#define _SPARC_CPUDATA_H
#include <linux/percpu.h>
typedef struct {
	unsigned long udelay_val;
	unsigned long clock_tick;
	unsigned int counter;
#ifdef CONFIG_SMP
	unsigned int irq_resched_count;
	unsigned int irq_call_count;
#endif
	int prom_node;
	int mid;
	int next;
} cpuinfo_sparc;
DECLARE_PER_CPU(cpuinfo_sparc, __cpu_data);
#define cpu_data(__cpu) per_cpu(__cpu_data, (__cpu))
#define local_cpu_data() (*this_cpu_ptr(&__cpu_data))
#endif  
