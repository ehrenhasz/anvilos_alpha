#ifndef __ASM_CPU_OPS_H
#define __ASM_CPU_OPS_H
#include <linux/init.h>
#include <linux/threads.h>
struct cpu_operations {
	const char	*name;
	int		(*cpu_init)(unsigned int);
	int		(*cpu_prepare)(unsigned int);
	int		(*cpu_boot)(unsigned int);
	void		(*cpu_postboot)(void);
#ifdef CONFIG_HOTPLUG_CPU
	bool		(*cpu_can_disable)(unsigned int cpu);
	int		(*cpu_disable)(unsigned int cpu);
	void		(*cpu_die)(unsigned int cpu);
	int		(*cpu_kill)(unsigned int cpu);
#endif
};
int __init init_cpu_ops(int cpu);
extern const struct cpu_operations *get_cpu_ops(int cpu);
static inline void __init init_bootcpu_ops(void)
{
	init_cpu_ops(0);
}
#endif  
