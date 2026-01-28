#ifndef __ARCH_ARM_PLAT_OMAP_INCLUDE_MACH_OMAP_DEVICE_H
#define __ARCH_ARM_PLAT_OMAP_INCLUDE_MACH_OMAP_DEVICE_H
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include "omap_hwmod.h"
#define OMAP_DEVICE_STATE_UNKNOWN	0
#define OMAP_DEVICE_STATE_ENABLED	1
#define OMAP_DEVICE_STATE_IDLE		2
#define OMAP_DEVICE_STATE_SHUTDOWN	3
#define OMAP_DEVICE_SUSPENDED		BIT(0)
struct omap_device {
	struct platform_device		*pdev;
	struct omap_hwmod		**hwmods;
	unsigned long			_driver_status;
	u8				hwmods_cnt;
	u8				_state;
	u8                              flags;
};
int omap_device_enable(struct platform_device *pdev);
int omap_device_idle(struct platform_device *pdev);
int omap_device_assert_hardreset(struct platform_device *pdev,
				 const char *name);
int omap_device_deassert_hardreset(struct platform_device *pdev,
				 const char *name);
static inline struct omap_device *to_omap_device(struct platform_device *pdev)
{
	return pdev ? pdev->archdata.od : NULL;
}
#endif
