#ifndef __ARCH_MXS_PM_H
#define __ARCH_MXS_PM_H
#ifdef CONFIG_PM
void mxs_pm_init(void);
#else
#define mxs_pm_init NULL
#endif
#endif
