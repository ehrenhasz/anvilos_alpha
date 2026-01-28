#ifndef __ASM_SMP_H
#define __ASM_SMP_H
#include <linux/bitops.h>
#include <linux/linkage.h>
#include <linux/smp.h>
#include <linux/threads.h>
#include <linux/cpumask.h>
#include <linux/atomic.h>
#include <asm/smp-ops.h>
extern int smp_num_siblings;
extern cpumask_t cpu_sibling_map[];
extern cpumask_t cpu_core_map[];
extern cpumask_t cpu_foreign_map[];
static inline int raw_smp_processor_id(void)
{
#if defined(__VDSO__)
	extern int vdso_smp_processor_id(void)
		__compiletime_error("VDSO should not call smp_processor_id()");
	return vdso_smp_processor_id();
#else
	return current_thread_info()->cpu;
#endif
}
#define raw_smp_processor_id raw_smp_processor_id
extern int __cpu_number_map[CONFIG_MIPS_NR_CPU_NR_MAP];
#define cpu_number_map(cpu)  __cpu_number_map[cpu]
extern int __cpu_logical_map[NR_CPUS];
#define cpu_logical_map(cpu)  __cpu_logical_map[cpu]
#define NO_PROC_ID	(-1)
#define SMP_RESCHEDULE_YOURSELF 0x1	 
#define SMP_CALL_FUNCTION	0x2
#define SMP_ICACHE_FLUSH	0x4
#define SMP_ASK_C0COUNT		0x8
extern cpumask_t cpu_coherent_mask;
extern unsigned int smp_max_threads __initdata;
extern asmlinkage void smp_bootstrap(void);
extern void calculate_cpu_foreign_map(void);
static inline void arch_smp_send_reschedule(int cpu)
{
	extern const struct plat_smp_ops *mp_ops;	 
	mp_ops->send_ipi_single(cpu, SMP_RESCHEDULE_YOURSELF);
}
#ifdef CONFIG_HOTPLUG_CPU
static inline int __cpu_disable(void)
{
	extern const struct plat_smp_ops *mp_ops;	 
	return mp_ops->cpu_disable();
}
static inline void __cpu_die(unsigned int cpu)
{
	extern const struct plat_smp_ops *mp_ops;	 
	mp_ops->cpu_die(cpu);
}
extern void __noreturn play_dead(void);
#endif
#ifdef CONFIG_KEXEC
static inline void kexec_nonboot_cpu(void)
{
	extern const struct plat_smp_ops *mp_ops;	 
	return mp_ops->kexec_nonboot_cpu();
}
static inline void *kexec_nonboot_cpu_func(void)
{
	extern const struct plat_smp_ops *mp_ops;	 
	return mp_ops->kexec_nonboot_cpu;
}
#endif
int mips_smp_ipi_allocate(const struct cpumask *mask);
int mips_smp_ipi_free(const struct cpumask *mask);
static inline void arch_send_call_function_single_ipi(int cpu)
{
	extern const struct plat_smp_ops *mp_ops;	 
	mp_ops->send_ipi_single(cpu, SMP_CALL_FUNCTION);
}
static inline void arch_send_call_function_ipi_mask(const struct cpumask *mask)
{
	extern const struct plat_smp_ops *mp_ops;	 
	mp_ops->send_ipi_mask(mask, SMP_CALL_FUNCTION);
}
#endif  
