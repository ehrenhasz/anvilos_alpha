#ifndef __ASM_SMP_H
#define __ASM_SMP_H
extern int init_per_cpu(int cpuid);
#if defined(CONFIG_SMP)
#define PDC_OS_BOOT_RENDEZVOUS     0x10
#define PDC_OS_BOOT_RENDEZVOUS_HI  0x28
#ifndef ASSEMBLY
#include <linux/bitops.h>
#include <linux/threads.h>	 
#include <linux/cpumask.h>
typedef unsigned long address_t;
#define cpu_number_map(cpu)	(cpu)
#define cpu_logical_map(cpu)	(cpu)
extern void smp_send_all_nop(void);
extern void arch_send_call_function_single_ipi(int cpu);
extern void arch_send_call_function_ipi_mask(const struct cpumask *mask);
#define raw_smp_processor_id()		(current_thread_info()->cpu)
#endif  
#else  
static inline void smp_send_all_nop(void) { return; }
#endif
#define NO_PROC_ID		0xFF		 
#define ANY_PROC_ID		0xFF		 
int __cpu_disable(void);
void __cpu_die(unsigned int cpu);
#endif  
