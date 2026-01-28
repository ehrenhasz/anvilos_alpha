#ifndef __MACH_STI_SMP_H
#define __MACH_STI_SMP_H
extern const struct smp_operations sti_smp_ops;
void sti_secondary_startup(void);
#endif
