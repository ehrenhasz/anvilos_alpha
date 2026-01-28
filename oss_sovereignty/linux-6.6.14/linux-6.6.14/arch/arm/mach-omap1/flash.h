#ifndef __OMAP_FLASH_H
#define __OMAP_FLASH_H
#include <linux/mtd/map.h>
struct platform_device;
extern void omap1_set_vpp(struct platform_device *pdev, int enable);
#endif
