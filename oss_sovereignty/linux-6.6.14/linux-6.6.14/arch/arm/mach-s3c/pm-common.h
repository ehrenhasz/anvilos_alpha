#ifndef __PLAT_SAMSUNG_PM_COMMON_H
#define __PLAT_SAMSUNG_PM_COMMON_H __FILE__
#include <linux/irq.h>
#include <linux/soc/samsung/s3c-pm.h>
struct sleep_save {
	void __iomem	*reg;
	unsigned long	val;
};
#define SAVE_ITEM(x) \
	{ .reg = (x) }
extern void s3c_pm_do_save(struct sleep_save *ptr, int count);
extern void s3c_pm_do_restore(const struct sleep_save *ptr, int count);
extern void s3c_pm_do_restore_core(const struct sleep_save *ptr, int count);
#endif
