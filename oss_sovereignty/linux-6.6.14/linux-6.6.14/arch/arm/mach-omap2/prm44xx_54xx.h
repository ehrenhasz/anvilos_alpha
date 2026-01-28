#ifndef __ARCH_ARM_MACH_OMAP2_PRM44XX_54XX_H
#define __ARCH_ARM_MACH_OMAP2_PRM44XX_54XX_H
#include "prcm-common.h"
#ifndef __ASSEMBLER__
extern u32 omap4_prm_vcvp_read(u8 offset);
extern void omap4_prm_vcvp_write(u32 val, u8 offset);
extern u32 omap4_prm_vcvp_rmw(u32 mask, u32 bits, u8 offset);
int __init omap44xx_prm_init(const struct omap_prcm_init_data *data);
#endif
#endif
