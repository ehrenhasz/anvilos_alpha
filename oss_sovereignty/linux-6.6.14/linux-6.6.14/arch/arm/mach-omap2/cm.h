#ifndef __ARCH_ASM_MACH_OMAP2_CM_H
#define __ARCH_ASM_MACH_OMAP2_CM_H
#define MAX_MODULE_READY_TIME		2000
# ifndef __ASSEMBLER__
#include <linux/clk/ti.h>
#include "prcm-common.h"
extern struct omap_domain_base cm_base;
extern struct omap_domain_base cm2_base;
# endif
#define MAX_MODULE_DISABLE_TIME		5000
# ifndef __ASSEMBLER__
struct cm_ll_data {
	int (*split_idlest_reg)(struct clk_omap_reg *idlest_reg, s16 *prcm_inst,
				u8 *idlest_reg_id);
	int (*wait_module_ready)(u8 part, s16 prcm_mod, u16 idlest_reg,
				 u8 idlest_shift);
	int (*wait_module_idle)(u8 part, s16 prcm_mod, u16 idlest_reg,
				u8 idlest_shift);
	void (*module_enable)(u8 mode, u8 part, u16 inst, u16 clkctrl_offs);
	void (*module_disable)(u8 part, u16 inst, u16 clkctrl_offs);
	u32 (*xlate_clkctrl)(u8 part, u16 inst, u16 clkctrl_offs);
};
extern int cm_split_idlest_reg(struct clk_omap_reg *idlest_reg, s16 *prcm_inst,
			       u8 *idlest_reg_id);
int omap_cm_wait_module_ready(u8 part, s16 prcm_mod, u16 idlest_reg,
			      u8 idlest_shift);
int omap_cm_wait_module_idle(u8 part, s16 prcm_mod, u16 idlest_reg,
			     u8 idlest_shift);
int omap_cm_module_enable(u8 mode, u8 part, u16 inst, u16 clkctrl_offs);
int omap_cm_module_disable(u8 part, u16 inst, u16 clkctrl_offs);
u32 omap_cm_xlate_clkctrl(u8 part, u16 inst, u16 clkctrl_offs);
extern int cm_register(const struct cm_ll_data *cld);
extern int cm_unregister(const struct cm_ll_data *cld);
int omap_cm_init(void);
int omap2_cm_base_init(void);
# endif
#endif
