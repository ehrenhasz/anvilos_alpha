#include "pm-common.h"
struct device;
#ifdef CONFIG_SAMSUNG_PM
extern __init int s3c_pm_init(void);
extern __init int s3c64xx_pm_init(void);
#else
static inline int s3c_pm_init(void)
{
	return 0;
}
static inline int s3c64xx_pm_init(void)
{
	return 0;
}
#endif
extern unsigned long s3c_irqwake_intmask;
extern unsigned long s3c_irqwake_eintmask;
extern void (*pm_cpu_prep)(void);
extern int (*pm_cpu_sleep)(unsigned long);
extern unsigned long s3c_pm_flags;
extern int s3c2410_cpu_suspend(unsigned long);
#ifdef CONFIG_PM_SLEEP
extern int s3c_irq_wake(struct irq_data *data, unsigned int state);
extern void s3c_cpu_resume(void);
#else
#define s3c_irq_wake NULL
#define s3c_cpu_resume NULL
#endif
#ifdef CONFIG_SAMSUNG_PM
extern int s3c_irqext_wake(struct irq_data *data, unsigned int state);
#else
#define s3c_irqext_wake NULL
#endif
extern void s3c_pm_configure_extint(void);
#ifdef CONFIG_GPIO_SAMSUNG
extern void samsung_pm_restore_gpios(void);
extern void samsung_pm_save_gpios(void);
#else
static inline void samsung_pm_restore_gpios(void) {}
static inline void samsung_pm_save_gpios(void) {}
#endif
extern void s3c_pm_save_core(void);
extern void s3c_pm_restore_core(void);
