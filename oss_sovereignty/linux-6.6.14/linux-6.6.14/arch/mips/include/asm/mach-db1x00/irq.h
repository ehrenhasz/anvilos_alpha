#ifndef __ASM_MACH_GENERIC_IRQ_H
#define __ASM_MACH_GENERIC_IRQ_H
#ifdef NR_IRQS
#undef NR_IRQS
#endif
#ifndef MIPS_CPU_IRQ_BASE
#define MIPS_CPU_IRQ_BASE 0
#endif
#define NR_IRQS 152
#endif  
