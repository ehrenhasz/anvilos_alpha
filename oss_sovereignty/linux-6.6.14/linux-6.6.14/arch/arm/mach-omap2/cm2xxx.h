#ifndef __ARCH_ASM_MACH_OMAP2_CM2XXX_H
#define __ARCH_ASM_MACH_OMAP2_CM2XXX_H
#include "prcm-common.h"
#include "cm2xxx_3xxx.h"
#define OMAP2420_CM_REGADDR(module, reg)				\
			OMAP2_L4_IO_ADDRESS(OMAP2420_CM_BASE + (module) + (reg))
#define OMAP2430_CM_REGADDR(module, reg)				\
			OMAP2_L4_IO_ADDRESS(OMAP2430_CM_BASE + (module) + (reg))
#define OMAP24XX_CM_FCLKEN2				0x0004
#define OMAP24XX_CM_ICLKEN4				0x001c
#define OMAP24XX_CM_AUTOIDLE4				0x003c
#define OMAP24XX_CM_IDLEST4				0x002c
#define OMAP24XX_CM_IDLEST_VAL				0
#ifndef __ASSEMBLER__
extern void omap2xxx_cm_set_dpll_disable_autoidle(void);
extern void omap2xxx_cm_set_dpll_auto_low_power_stop(void);
extern int omap2xxx_cm_fclks_active(void);
extern int omap2xxx_cm_mpu_retention_allowed(void);
extern u32 omap2xxx_cm_get_core_clk_src(void);
extern u32 omap2xxx_cm_get_core_pll_config(void);
extern void omap2xxx_cm_set_mod_dividers(u32 mpu, u32 dsp, u32 gfx, u32 core,
					 u32 mdm);
int __init omap2xxx_cm_init(const struct omap_prcm_init_data *data);
#endif
#endif
