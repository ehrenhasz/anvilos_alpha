#ifndef __ASM_MACH_IP27_IRQ_H
#define __ASM_MACH_IP27_IRQ_H
#define NR_IRQS 256
#include <asm/mach-generic/irq.h>
#define IP27_HUB_PEND0_IRQ	(MIPS_CPU_IRQ_BASE + 2)
#define IP27_HUB_PEND1_IRQ	(MIPS_CPU_IRQ_BASE + 3)
#define IP27_RT_TIMER_IRQ	(MIPS_CPU_IRQ_BASE + 4)
#define IP27_HUB_IRQ_BASE	(MIPS_CPU_IRQ_BASE + 8)
#define IP27_HUB_IRQ_COUNT	128
#endif  
