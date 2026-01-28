#ifndef ARCH_ARM_MACH_OMAP2_HDQ1W_H
#define ARCH_ARM_MACH_OMAP2_HDQ1W_H
#include "omap_hwmod.h"
#define HDQ_CTRL_STATUS_OFFSET			0x0c
#define HDQ_CTRL_STATUS_CLOCKENABLE_SHIFT	5
extern int omap_hdq1w_reset(struct omap_hwmod *oh);
#endif
