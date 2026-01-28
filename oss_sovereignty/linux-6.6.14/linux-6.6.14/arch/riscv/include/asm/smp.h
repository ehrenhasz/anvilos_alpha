#ifndef _ASM_RISCV_SMP_H
#define _ASM_RISCV_SMP_H
#include <linux/cpumask.h>
#include <linux/irqreturn.h>
#include <linux/thread_info.h>
#define INVALID_HARTID ULONG_MAX
struct seq_file;
extern unsigned long boot_cpu_hartid;
#ifdef CONFIG_SMP
#include <linux/jump_label.h>
extern unsigned long __cpuid_to_hartid_map[NR_CPUS];
#define cpuid_to_hartid_map(cpu)    __cpuid_to_hartid_map[cpu]
void show_ipi_stats(struct seq_file *p, int prec);
void __init setup_smp(void);
void arch_send_call_function_ipi_mask(struct cpumask *mask);
void arch_send_call_function_single_ipi(int cpu);
int riscv_hartid_to_cpuid(unsigned long hartid);
void riscv_ipi_enable(void);
void riscv_ipi_disable(void);
bool riscv_ipi_have_virq_range(void);
void riscv_ipi_set_virq_range(int virq, int nr, bool use_for_rfence);
DECLARE_STATIC_KEY_FALSE(riscv_ipi_for_rfence);
#define riscv_use_ipi_for_rfence() \
	static_branch_unlikely(&riscv_ipi_for_rfence)
bool smp_crash_stop_failed(void);
asmlinkage void smp_callin(void);
#define raw_smp_processor_id() (current_thread_info()->cpu)
#if defined CONFIG_HOTPLUG_CPU
int __cpu_disable(void);
static inline void __cpu_die(unsigned int cpu) { }
#endif  
#else
static inline void show_ipi_stats(struct seq_file *p, int prec)
{
}
static inline int riscv_hartid_to_cpuid(unsigned long hartid)
{
	if (hartid == boot_cpu_hartid)
		return 0;
	return -1;
}
static inline unsigned long cpuid_to_hartid_map(int cpu)
{
	return boot_cpu_hartid;
}
static inline void riscv_ipi_enable(void)
{
}
static inline void riscv_ipi_disable(void)
{
}
static inline bool riscv_ipi_have_virq_range(void)
{
	return false;
}
static inline void riscv_ipi_set_virq_range(int virq, int nr,
					    bool use_for_rfence)
{
}
static inline bool riscv_use_ipi_for_rfence(void)
{
	return false;
}
#endif  
#if defined(CONFIG_HOTPLUG_CPU) && (CONFIG_SMP)
bool cpu_has_hotplug(unsigned int cpu);
#else
static inline bool cpu_has_hotplug(unsigned int cpu)
{
	return false;
}
#endif
#endif  
