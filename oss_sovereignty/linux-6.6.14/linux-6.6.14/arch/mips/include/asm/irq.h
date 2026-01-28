#ifndef _ASM_IRQ_H
#define _ASM_IRQ_H
#include <linux/linkage.h>
#include <linux/smp.h>
#include <asm/mipsmtregs.h>
#include <irq.h>
#define IRQ_STACK_SIZE			THREAD_SIZE
#define IRQ_STACK_START			(IRQ_STACK_SIZE - 16)
extern void *irq_stack[NR_CPUS];
static inline bool on_irq_stack(int cpu, unsigned long sp)
{
	unsigned long low = (unsigned long)irq_stack[cpu];
	unsigned long high = low + IRQ_STACK_SIZE;
	return (low <= sp && sp <= high);
}
#ifdef CONFIG_I8259
static inline int irq_canonicalize(int irq)
{
	return ((irq == I8259A_IRQ_BASE + 2) ? I8259A_IRQ_BASE + 9 : irq);
}
#else
#define irq_canonicalize(irq) (irq)	 
#endif
asmlinkage void plat_irq_dispatch(void);
extern void do_IRQ(unsigned int irq);
struct irq_domain;
extern void do_domain_IRQ(struct irq_domain *domain, unsigned int irq);
extern void arch_init_irq(void);
extern void spurious_interrupt(void);
#define CP0_LEGACY_COMPARE_IRQ 7
#define CP0_LEGACY_PERFCNT_IRQ 7
extern int cp0_compare_irq;
extern int cp0_compare_irq_shift;
extern int cp0_perfcount_irq;
extern int cp0_fdc_irq;
extern int get_c0_fdc_int(void);
void arch_trigger_cpumask_backtrace(const struct cpumask *mask,
				    int exclude_cpu);
#define arch_trigger_cpumask_backtrace arch_trigger_cpumask_backtrace
#endif  
