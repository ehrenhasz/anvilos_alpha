#ifndef __ASM_OPENRISC_SMP_H
#define __ASM_OPENRISC_SMP_H
#include <asm/spr.h>
#include <asm/spr_defs.h>
#define raw_smp_processor_id()	(current_thread_info()->cpu)
#define hard_smp_processor_id()	mfspr(SPR_COREID)
extern void smp_init_cpus(void);
extern void arch_send_call_function_single_ipi(int cpu);
extern void arch_send_call_function_ipi_mask(const struct cpumask *mask);
extern void set_smp_cross_call(void (*)(const struct cpumask *, unsigned int));
extern void handle_IPI(unsigned int ipi_msg);
#endif  
