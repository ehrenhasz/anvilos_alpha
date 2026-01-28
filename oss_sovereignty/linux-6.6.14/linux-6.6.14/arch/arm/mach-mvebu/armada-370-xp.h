#ifndef __MACH_ARMADA_370_XP_H
#define __MACH_ARMADA_370_XP_H
#ifdef CONFIG_SMP
void armada_xp_secondary_startup(void);
extern const struct smp_operations armada_xp_smp_ops;
#endif
#endif  
