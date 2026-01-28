#ifndef __ASM_TXX9IRQ_H
#define __ASM_TXX9IRQ_H
#include <irq.h>
#ifdef CONFIG_IRQ_MIPS_CPU
#define TXX9_IRQ_BASE	(MIPS_CPU_IRQ_BASE + 8)
#else
#ifdef CONFIG_I8259
#define TXX9_IRQ_BASE	(I8259A_IRQ_BASE + 16)
#else
#define TXX9_IRQ_BASE	0
#endif
#endif
#define TXx9_MAX_IR 32
void txx9_irq_init(unsigned long baseaddr);
int txx9_irq(void);
int txx9_irq_set_pri(int irc_irq, int new_pri);
#endif  
