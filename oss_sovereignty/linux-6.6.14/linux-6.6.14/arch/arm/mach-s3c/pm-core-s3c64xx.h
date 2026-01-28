#ifndef __MACH_S3C64XX_PM_CORE_H
#define __MACH_S3C64XX_PM_CORE_H __FILE__
#include <linux/serial_s3c.h>
#include <linux/delay.h>
#include "regs-gpio.h"
#include "regs-clock.h"
#include "map.h"
static inline void s3c_pm_debug_init_uart(void)
{
}
static inline void s3c_pm_arch_prepare_irqs(void)
{
	__raw_writel(__raw_readl(S3C64XX_EINT0PEND), S3C64XX_EINT0PEND);
}
static inline void s3c_pm_arch_stop_clocks(void)
{
}
static inline void s3c_pm_arch_show_resume_irqs(void)
{
}
#ifdef CONFIG_PM_SLEEP
#define s3c_irqwake_eintallow	((1 << 28) - 1)
#define s3c_irqwake_intallow	(~0)
#else
#define s3c_irqwake_eintallow 0
#define s3c_irqwake_intallow  0
#endif
static inline void s3c_pm_restored_gpios(void)
{
	__raw_writel(0, S3C64XX_SLPEN);
}
static inline void samsung_pm_saved_gpios(void)
{
	__raw_writel(S3C64XX_SLPEN_USE_xSLP, S3C64XX_SLPEN);
}
#endif  
