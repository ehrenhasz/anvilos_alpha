#ifndef __ASM_MACH_GENERIC_IRQ_H
#define __ASM_MACH_GENERIC_IRQ_H
#ifndef NR_IRQS
#define NR_IRQS 256
#endif
#ifdef CONFIG_I8259
#ifndef I8259A_IRQ_BASE
#define I8259A_IRQ_BASE 0
#endif
#endif
#ifdef CONFIG_IRQ_MIPS_CPU
#ifndef MIPS_CPU_IRQ_BASE
#ifdef CONFIG_I8259
#define MIPS_CPU_IRQ_BASE 16
#else
#define MIPS_CPU_IRQ_BASE 0
#endif  
#endif
#endif  
#endif  
