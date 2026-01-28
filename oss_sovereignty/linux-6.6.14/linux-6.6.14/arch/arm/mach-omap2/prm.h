#ifndef __ARCH_ARM_MACH_OMAP2_PRM_H
#define __ARCH_ARM_MACH_OMAP2_PRM_H
#include "prcm-common.h"
# ifndef __ASSEMBLER__
extern struct omap_domain_base prm_base;
extern u16 prm_features;
int omap_prcm_init(void);
int omap2_prcm_base_init(void);
# endif
#define PRM_HAS_IO_WAKEUP	BIT(0)
#define PRM_HAS_VOLTAGE		BIT(1)
#define MAX_MODULE_SOFTRESET_WAIT		10000
#define MAX_MODULE_HARDRESET_WAIT		10000
#define OMAP_INTRANSITION_MASK				(1 << 20)
#define OMAP_POWERSTATEST_SHIFT				0
#define OMAP_POWERSTATEST_MASK				(0x3 << 0)
#define OMAP_POWERSTATE_SHIFT				0
#define OMAP_POWERSTATE_MASK				(0x3 << 0)
#define OMAP_GLOBAL_COLD_RST_SRC_ID_SHIFT			0
#define OMAP_GLOBAL_WARM_RST_SRC_ID_SHIFT			1
#define OMAP_SECU_VIOL_RST_SRC_ID_SHIFT				2
#define OMAP_MPU_WD_RST_SRC_ID_SHIFT				3
#define OMAP_SECU_WD_RST_SRC_ID_SHIFT				4
#define OMAP_EXTWARM_RST_SRC_ID_SHIFT				5
#define OMAP_VDD_MPU_VM_RST_SRC_ID_SHIFT			6
#define OMAP_VDD_IVA_VM_RST_SRC_ID_SHIFT			7
#define OMAP_VDD_CORE_VM_RST_SRC_ID_SHIFT			8
#define OMAP_ICEPICK_RST_SRC_ID_SHIFT				9
#define OMAP_ICECRUSHER_RST_SRC_ID_SHIFT			10
#define OMAP_C2C_RST_SRC_ID_SHIFT				11
#define OMAP_UNKNOWN_RST_SRC_ID_SHIFT				12
#ifndef __ASSEMBLER__
struct prm_reset_src_map {
	s8 reg_shift;
	s8 std_shift;
};
struct prm_ll_data {
	u32 (*read_reset_sources)(void);
	bool (*was_any_context_lost_old)(u8 part, s16 inst, u16 idx);
	void (*clear_context_loss_flags_old)(u8 part, s16 inst, u16 idx);
	int (*late_init)(void);
	int (*assert_hardreset)(u8 shift, u8 part, s16 prm_mod, u16 offset);
	int (*deassert_hardreset)(u8 shift, u8 st_shift, u8 part, s16 prm_mod,
				  u16 offset, u16 st_offset);
	int (*is_hardreset_asserted)(u8 shift, u8 part, s16 prm_mod,
				     u16 offset);
	void (*reset_system)(void);
	int (*clear_mod_irqs)(s16 module, u8 regs, u32 wkst_mask);
	u32 (*vp_check_txdone)(u8 vp_id);
	void (*vp_clear_txdone)(u8 vp_id);
};
extern int prm_register(struct prm_ll_data *pld);
extern int prm_unregister(struct prm_ll_data *pld);
int omap_prm_assert_hardreset(u8 shift, u8 part, s16 prm_mod, u16 offset);
int omap_prm_deassert_hardreset(u8 shift, u8 st_shift, u8 part, s16 prm_mod,
				u16 offset, u16 st_offset);
int omap_prm_is_hardreset_asserted(u8 shift, u8 part, s16 prm_mod, u16 offset);
extern bool prm_was_any_context_lost_old(u8 part, s16 inst, u16 idx);
extern void prm_clear_context_loss_flags_old(u8 part, s16 inst, u16 idx);
void omap_prm_reset_system(void);
int omap_prm_clear_mod_irqs(s16 module, u8 regs, u32 wkst_mask);
#define OMAP3_VP_VDD_MPU_ID	0
#define OMAP3_VP_VDD_CORE_ID	1
#define OMAP4_VP_VDD_CORE_ID	0
#define OMAP4_VP_VDD_IVA_ID	1
#define OMAP4_VP_VDD_MPU_ID	2
u32 omap_prm_vp_check_txdone(u8 vp_id);
void omap_prm_vp_clear_txdone(u8 vp_id);
#endif
#endif
