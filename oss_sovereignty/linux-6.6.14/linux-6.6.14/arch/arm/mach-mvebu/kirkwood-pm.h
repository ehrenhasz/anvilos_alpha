#ifndef __ARCH_KIRKWOOD_PM_H
#define __ARCH_KIRKWOOD_PM_H
#ifdef CONFIG_PM
void kirkwood_pm_init(void);
#else
static inline void kirkwood_pm_init(void) {};
#endif
#endif
