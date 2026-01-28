#ifndef __ASM_ARC_IRQ_H
#define __ASM_ARC_IRQ_H
#define NR_IRQS		512
#ifdef CONFIG_ISA_ARCV2
#define IPI_IRQ		19
#define SOFTIRQ_IRQ	21
#define FIRST_EXT_IRQ	24
#endif
#include <linux/interrupt.h>
#include <asm-generic/irq.h>
extern void arc_init_IRQ(void);
extern void arch_do_IRQ(unsigned int, struct pt_regs *);
#endif
