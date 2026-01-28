#ifndef __MACH_370_XP_COHERENCY_H
#define __MACH_370_XP_COHERENCY_H
extern void __iomem *coherency_base;	 
extern unsigned long coherency_phys_base;
int set_cpu_coherent(void);
int coherency_init(void);
int coherency_available(void);
#endif	 
