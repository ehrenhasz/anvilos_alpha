#ifndef _ASM_POWERPC_I8259_H
#define _ASM_POWERPC_I8259_H
#ifdef __KERNEL__
#include <linux/irq.h>
extern void i8259_init(struct device_node *node, unsigned long intack_addr);
extern unsigned int i8259_irq(void);
struct irq_domain *__init i8259_get_host(void);
#endif  
#endif  
