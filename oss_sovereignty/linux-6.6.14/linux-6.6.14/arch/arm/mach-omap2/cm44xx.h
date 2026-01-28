#ifndef __ARCH_ASM_MACH_OMAP2_CM44XX_H
#define __ARCH_ASM_MACH_OMAP2_CM44XX_H
#include "prcm-common.h"
#include "cm.h"
#define OMAP4_CM_CLKSTCTRL				0x0000
#define OMAP4_CM_STATICDEP				0x0004
int omap4_cm_init(const struct omap_prcm_init_data *data);
#endif
