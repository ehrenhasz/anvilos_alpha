#ifdef __KERNEL__
#ifndef _ASM_POWERPC_IRQ_H
#define _ASM_POWERPC_IRQ_H
#include <linux/threads.h>
#include <linux/list.h>
#include <linux/radix-tree.h>
#include <asm/types.h>
#include <linux/atomic.h>
extern atomic_t ppc_n_lost_interrupts;
#define NR_IRQS		CONFIG_NR_IRQS
#define NR_IRQS_LEGACY		16
extern irq_hw_number_t virq_to_hw(unsigned int virq);
static __inline__ int irq_canonicalize(int irq)
{
	return irq;
}
extern int distribute_irqs;
struct pt_regs;
#ifdef CONFIG_BOOKE_OR_40x
extern void *critirq_ctx[NR_CPUS];
extern void *dbgirq_ctx[NR_CPUS];
extern void *mcheckirq_ctx[NR_CPUS];
#endif
extern void *hardirq_ctx[NR_CPUS];
extern void *softirq_ctx[NR_CPUS];
void __do_IRQ(struct pt_regs *regs);
int irq_choose_cpu(const struct cpumask *mask);
#if defined(CONFIG_PPC_BOOK3S_64) && defined(CONFIG_NMI_IPI)
extern void arch_trigger_cpumask_backtrace(const cpumask_t *mask,
					   int exclude_cpu);
#define arch_trigger_cpumask_backtrace arch_trigger_cpumask_backtrace
#endif
#endif  
#endif  
