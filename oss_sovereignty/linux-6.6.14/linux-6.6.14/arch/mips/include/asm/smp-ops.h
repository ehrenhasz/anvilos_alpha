#ifndef __ASM_SMP_OPS_H
#define __ASM_SMP_OPS_H
#include <linux/errno.h>
#include <asm/mips-cps.h>
#ifdef CONFIG_SMP
#include <linux/cpumask.h>
struct task_struct;
struct plat_smp_ops {
	void (*send_ipi_single)(int cpu, unsigned int action);
	void (*send_ipi_mask)(const struct cpumask *mask, unsigned int action);
	void (*init_secondary)(void);
	void (*smp_finish)(void);
	int (*boot_secondary)(int cpu, struct task_struct *idle);
	void (*smp_setup)(void);
	void (*prepare_cpus)(unsigned int max_cpus);
	void (*prepare_boot_cpu)(void);
#ifdef CONFIG_HOTPLUG_CPU
	int (*cpu_disable)(void);
	void (*cpu_die)(unsigned int cpu);
	void (*cleanup_dead_cpu)(unsigned cpu);
#endif
#ifdef CONFIG_KEXEC
	void (*kexec_nonboot_cpu)(void);
#endif
};
extern void register_smp_ops(const struct plat_smp_ops *ops);
static inline void plat_smp_setup(void)
{
	extern const struct plat_smp_ops *mp_ops;	 
	mp_ops->smp_setup();
}
extern void mips_smp_send_ipi_single(int cpu, unsigned int action);
extern void mips_smp_send_ipi_mask(const struct cpumask *mask,
				      unsigned int action);
#else  
struct plat_smp_ops;
static inline void plat_smp_setup(void)
{
}
static inline void register_smp_ops(const struct plat_smp_ops *ops)
{
}
#endif  
static inline int register_up_smp_ops(void)
{
#ifdef CONFIG_SMP_UP
	extern const struct plat_smp_ops up_smp_ops;
	register_smp_ops(&up_smp_ops);
	return 0;
#else
	return -ENODEV;
#endif
}
static inline int register_vsmp_smp_ops(void)
{
#ifdef CONFIG_MIPS_MT_SMP
	extern const struct plat_smp_ops vsmp_smp_ops;
	if (!cpu_has_mipsmt)
		return -ENODEV;
	register_smp_ops(&vsmp_smp_ops);
	return 0;
#else
	return -ENODEV;
#endif
}
#ifdef CONFIG_MIPS_CPS
extern int register_cps_smp_ops(void);
#else
static inline int register_cps_smp_ops(void)
{
	return -ENODEV;
}
#endif
#endif  
