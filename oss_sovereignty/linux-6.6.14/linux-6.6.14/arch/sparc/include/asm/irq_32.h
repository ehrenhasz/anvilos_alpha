#ifndef _SPARC_IRQ_H
#define _SPARC_IRQ_H
#define NR_IRQS    64
#include <linux/interrupt.h>
#define irq_canonicalize(irq)	(irq)
void __init sun4d_init_sbi_irq(void);
#define NO_IRQ		0xffffffff
#endif
