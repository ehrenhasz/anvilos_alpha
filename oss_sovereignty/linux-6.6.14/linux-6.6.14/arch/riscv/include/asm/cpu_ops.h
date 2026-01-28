#ifndef __ASM_CPU_OPS_H
#define __ASM_CPU_OPS_H
#include <linux/init.h>
#include <linux/sched.h>
#include <linux/threads.h>
struct cpu_operations {
	const char	*name;
	int		(*cpu_prepare)(unsigned int cpu);
	int		(*cpu_start)(unsigned int cpu,
				     struct task_struct *tidle);
#ifdef CONFIG_HOTPLUG_CPU
	int		(*cpu_disable)(unsigned int cpu);
	void		(*cpu_stop)(void);
	int		(*cpu_is_stopped)(unsigned int cpu);
#endif
};
extern const struct cpu_operations cpu_ops_spinwait;
extern const struct cpu_operations *cpu_ops[NR_CPUS];
void __init cpu_set_ops(int cpu);
#endif  
