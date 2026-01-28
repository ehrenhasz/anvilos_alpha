#ifndef __ASM_ARC_SMP_H
#define __ASM_ARC_SMP_H
#ifdef CONFIG_SMP
#include <linux/types.h>
#include <linux/init.h>
#include <linux/threads.h>
#define raw_smp_processor_id() (current_thread_info()->cpu)
struct cpumask;
extern void arch_send_call_function_single_ipi(int cpu);
extern void arch_send_call_function_ipi_mask(const struct cpumask *mask);
extern void __init smp_init_cpus(void);
extern void first_lines_of_secondary(void);
extern const char *arc_platform_smp_cpuinfo(void);
extern void arc_platform_smp_wait_to_boot(int);
extern void start_kernel_secondary(void);
extern int smp_ipi_irq_setup(int cpu, irq_hw_number_t hwirq);
struct plat_smp_ops {
	const char 	*info;
	void		(*init_early_smp)(void);
	void		(*init_per_cpu)(int cpu);
	void		(*cpu_kick)(int cpu, unsigned long pc);
	void		(*ipi_send)(int cpu);
	void		(*ipi_clear)(int irq);
};
extern struct plat_smp_ops  plat_smp_ops;
#else  
static inline void smp_init_cpus(void) {}
static inline const char *arc_platform_smp_cpuinfo(void)
{
	return "";
}
#endif   
#ifndef CONFIG_ARC_HAS_LLSC
#include <linux/irqflags.h>
#ifdef CONFIG_SMP
#include <asm/spinlock.h>
extern arch_spinlock_t smp_atomic_ops_lock;
#define atomic_ops_lock(flags)	do {		\
	local_irq_save(flags);			\
	arch_spin_lock(&smp_atomic_ops_lock);	\
} while (0)
#define atomic_ops_unlock(flags) do {		\
	arch_spin_unlock(&smp_atomic_ops_lock);	\
	local_irq_restore(flags);		\
} while (0)
#else  
#define atomic_ops_lock(flags)		local_irq_save(flags)
#define atomic_ops_unlock(flags)	local_irq_restore(flags)
#endif  
#endif	 
#endif
