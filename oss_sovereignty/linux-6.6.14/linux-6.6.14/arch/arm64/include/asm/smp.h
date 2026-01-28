#ifndef __ASM_SMP_H
#define __ASM_SMP_H
#include <linux/const.h>
#define CPU_STUCK_REASON_SHIFT		(8)
#define CPU_BOOT_STATUS_MASK		((UL(1) << CPU_STUCK_REASON_SHIFT) - 1)
#define CPU_MMU_OFF			(-1)
#define CPU_BOOT_SUCCESS		(0)
#define CPU_KILL_ME			(1)
#define CPU_STUCK_IN_KERNEL		(2)
#define CPU_PANIC_KERNEL		(3)
#define CPU_STUCK_REASON_52_BIT_VA	(UL(1) << CPU_STUCK_REASON_SHIFT)
#define CPU_STUCK_REASON_NO_GRAN	(UL(2) << CPU_STUCK_REASON_SHIFT)
#ifndef __ASSEMBLY__
#include <asm/percpu.h>
#include <linux/threads.h>
#include <linux/cpumask.h>
#include <linux/thread_info.h>
DECLARE_PER_CPU_READ_MOSTLY(int, cpu_number);
#define raw_smp_processor_id() (*raw_cpu_ptr(&cpu_number))
extern u64 __cpu_logical_map[NR_CPUS];
extern u64 cpu_logical_map(unsigned int cpu);
static inline void set_cpu_logical_map(unsigned int cpu, u64 hwid)
{
	__cpu_logical_map[cpu] = hwid;
}
struct seq_file;
extern void smp_init_cpus(void);
extern void set_smp_ipi_range(int ipi_base, int nr_ipi);
asmlinkage void secondary_start_kernel(void);
struct secondary_data {
	struct task_struct *task;
	long status;
};
extern struct secondary_data secondary_data;
extern long __early_cpu_boot_status;
extern void secondary_entry(void);
extern void arch_send_call_function_single_ipi(int cpu);
extern void arch_send_call_function_ipi_mask(const struct cpumask *mask);
#ifdef CONFIG_ARM64_ACPI_PARKING_PROTOCOL
extern void arch_send_wakeup_ipi_mask(const struct cpumask *mask);
#else
static inline void arch_send_wakeup_ipi_mask(const struct cpumask *mask)
{
	BUILD_BUG();
}
#endif
extern int __cpu_disable(void);
static inline void __cpu_die(unsigned int cpu) { }
extern void __noreturn cpu_die(void);
extern void __noreturn cpu_die_early(void);
static inline void __noreturn cpu_park_loop(void)
{
	for (;;) {
		wfe();
		wfi();
	}
}
static inline void update_cpu_boot_status(int val)
{
	WRITE_ONCE(secondary_data.status, val);
	dsb(ishst);
}
static inline void __noreturn cpu_panic_kernel(void)
{
	update_cpu_boot_status(CPU_PANIC_KERNEL);
	cpu_park_loop();
}
bool cpus_are_stuck_in_kernel(void);
extern void crash_smp_send_stop(void);
extern bool smp_crash_stop_failed(void);
#endif  
#endif  
