#ifndef __ASM_VDSO_PROCESSOR_H
#define __ASM_VDSO_PROCESSOR_H
#ifndef __ASSEMBLY__
#ifdef CONFIG_CPU_LOONGSON64
#define cpu_relax()	smp_mb()
#else
#define cpu_relax()	barrier()
#endif
#endif  
#endif  
