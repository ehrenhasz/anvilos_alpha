#ifndef __ARCH_ARM_MACH_OMAP2_PRCM_MPU_44XX_54XX_H
#define __ARCH_ARM_MACH_OMAP2_PRCM_MPU_44XX_54XX_H
#ifndef __ASSEMBLER__
#include "prcm-common.h"
extern struct omap_domain_base prcm_mpu_base;
extern u32 omap4_prcm_mpu_read_inst_reg(s16 inst, u16 idx);
extern void omap4_prcm_mpu_write_inst_reg(u32 val, s16 inst, u16 idx);
extern void __init omap2_set_globals_prcm_mpu(void __iomem *prcm_mpu);
#endif
#endif
