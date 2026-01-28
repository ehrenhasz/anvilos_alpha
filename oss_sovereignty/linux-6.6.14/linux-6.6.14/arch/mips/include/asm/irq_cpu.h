#ifndef _ASM_IRQ_CPU_H
#define _ASM_IRQ_CPU_H
extern void mips_cpu_irq_init(void);
#ifdef CONFIG_IRQ_DOMAIN
struct device_node;
extern int mips_cpu_irq_of_init(struct device_node *of_node,
				struct device_node *parent);
#endif
#endif  
