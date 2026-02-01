 

#include "display/intel_de.h"
#include "display/intel_display.h"
#include "display/intel_display_trace.h"
#include "display/skl_watermark.h"

#include "gt/intel_engine_regs.h"
#include "gt/intel_gt.h"
#include "gt/intel_gt_mcr.h"
#include "gt/intel_gt_regs.h"

#include "i915_drv.h"
#include "i915_reg.h"
#include "intel_clock_gating.h"
#include "intel_mchbar_regs.h"
#include "vlv_sideband.h"

struct drm_i915_clock_gating_funcs {
	void (*init_clock_gating)(struct drm_i915_private *i915);
};

static void gen9_init_clock_gating(struct drm_i915_private *i915)
{
	if (HAS_LLC(i915)) {
		 
		intel_uncore_rmw(&i915->uncore, CHICKEN_PAR1_1, 0, SKL_DE_COMPRESSED_HASH_MODE);
	}

	 
	intel_uncore_rmw(&i915->uncore, CHICKEN_PAR1_1, 0, SKL_EDP_PSR_FIX_RDWRAP);

	 
	intel_uncore_rmw(&i915->uncore, GEN8_CHICKEN_DCPR_1, 0, MASK_WAKEMEM);

	 
	intel_uncore_rmw(&i915->uncore, DISP_ARB_CTL, 0, DISP_FBC_MEMORY_WAKE);
}

static void bxt_init_clock_gating(struct drm_i915_private *i915)
{
	gen9_init_clock_gating(i915);

	 
	intel_uncore_rmw(&i915->uncore, GEN8_UCGCTL6, 0, GEN8_SDEUNIT_CLOCK_GATE_DISABLE);

	 
	intel_uncore_rmw(&i915->uncore, GEN8_UCGCTL6, 0, GEN8_HDCUNIT_CLOCK_GATE_DISABLE_HDCREQ);

	 
	intel_uncore_write(&i915->uncore, GEN9_CLKGATE_DIS_0,
			   intel_uncore_read(&i915->uncore, GEN9_CLKGATE_DIS_0) |
			   PWM1_GATING_DIS | PWM2_GATING_DIS);

	 
	intel_uncore_write(&i915->uncore, RM_TIMEOUT, MMIO_TIMEOUT_US(950));

	 
	intel_uncore_rmw(&i915->uncore, DISP_ARB_CTL, 0, DISP_FBC_WM_DIS);

	 
	intel_uncore_rmw(&i915->uncore, ILK_DPFC_CHICKEN(INTEL_FBC_A), 0, DPFC_DISABLE_DUMMY0);
}

static void glk_init_clock_gating(struct drm_i915_private *i915)
{
	gen9_init_clock_gating(i915);

	 
	intel_uncore_write(&i915->uncore, GEN9_CLKGATE_DIS_0,
			   intel_uncore_read(&i915->uncore, GEN9_CLKGATE_DIS_0) |
			   PWM1_GATING_DIS | PWM2_GATING_DIS);
}

static void ibx_init_clock_gating(struct drm_i915_private *i915)
{
	 
	intel_uncore_write(&i915->uncore, SOUTH_DSPCLK_GATE_D, PCH_DPLSUNIT_CLOCK_GATE_DISABLE);
}

static void g4x_disable_trickle_feed(struct drm_i915_private *dev_priv)
{
	enum pipe pipe;

	for_each_pipe(dev_priv, pipe) {
		intel_uncore_rmw(&dev_priv->uncore, DSPCNTR(pipe), 0, DISP_TRICKLE_FEED_DISABLE);

		intel_uncore_rmw(&dev_priv->uncore, DSPSURF(pipe), 0, 0);
		intel_uncore_posting_read(&dev_priv->uncore, DSPSURF(pipe));
	}
}

static void ilk_init_clock_gating(struct drm_i915_private *i915)
{
	u32 dspclk_gate = ILK_VRHUNIT_CLOCK_GATE_DISABLE;

	 
	dspclk_gate |= ILK_DPFCRUNIT_CLOCK_GATE_DISABLE |
		   ILK_DPFCUNIT_CLOCK_GATE_DISABLE |
		   ILK_DPFDUNIT_CLOCK_GATE_ENABLE;

	intel_uncore_write(&i915->uncore, PCH_3DCGDIS0,
			   MARIUNIT_CLOCK_GATE_DISABLE |
			   SVSMUNIT_CLOCK_GATE_DISABLE);
	intel_uncore_write(&i915->uncore, PCH_3DCGDIS1,
			   VFMUNIT_CLOCK_GATE_DISABLE);

	 
	intel_uncore_write(&i915->uncore, ILK_DISPLAY_CHICKEN2,
			   (intel_uncore_read(&i915->uncore, ILK_DISPLAY_CHICKEN2) |
			    ILK_DPARB_GATE | ILK_VSDPFD_FULL));
	dspclk_gate |= ILK_DPARBUNIT_CLOCK_GATE_ENABLE;
	intel_uncore_write(&i915->uncore, DISP_ARB_CTL,
			   (intel_uncore_read(&i915->uncore, DISP_ARB_CTL) |
			    DISP_FBC_WM_DIS));

	 
	if (IS_IRONLAKE_M(i915)) {
		 
		intel_uncore_rmw(&i915->uncore, ILK_DISPLAY_CHICKEN1, 0, ILK_FBCQ_DIS);
		intel_uncore_rmw(&i915->uncore, ILK_DISPLAY_CHICKEN2, 0, ILK_DPARB_GATE);
	}

	intel_uncore_write(&i915->uncore, ILK_DSPCLK_GATE_D, dspclk_gate);

	intel_uncore_rmw(&i915->uncore, ILK_DISPLAY_CHICKEN2, 0, ILK_ELPIN_409_SELECT);

	g4x_disable_trickle_feed(i915);

	ibx_init_clock_gating(i915);
}

static void cpt_init_clock_gating(struct drm_i915_private *i915)
{
	enum pipe pipe;
	u32 val;

	 
	intel_uncore_write(&i915->uncore, SOUTH_DSPCLK_GATE_D, PCH_DPLSUNIT_CLOCK_GATE_DISABLE |
			   PCH_DPLUNIT_CLOCK_GATE_DISABLE |
			   PCH_CPUNIT_CLOCK_GATE_DISABLE);
	intel_uncore_rmw(&i915->uncore, SOUTH_CHICKEN2, 0, DPLS_EDP_PPS_FIX_DIS);
	 
	for_each_pipe(i915, pipe) {
		val = intel_uncore_read(&i915->uncore, TRANS_CHICKEN2(pipe));
		val |= TRANS_CHICKEN2_TIMING_OVERRIDE;
		val &= ~TRANS_CHICKEN2_FDI_POLARITY_REVERSED;
		if (i915->display.vbt.fdi_rx_polarity_inverted)
			val |= TRANS_CHICKEN2_FDI_POLARITY_REVERSED;
		val &= ~TRANS_CHICKEN2_DISABLE_DEEP_COLOR_COUNTER;
		val &= ~TRANS_CHICKEN2_DISABLE_DEEP_COLOR_MODESWITCH;
		intel_uncore_write(&i915->uncore, TRANS_CHICKEN2(pipe), val);
	}
	 
	for_each_pipe(i915, pipe) {
		intel_uncore_write(&i915->uncore, TRANS_CHICKEN1(pipe),
				   TRANS_CHICKEN1_DP0UNIT_GC_DISABLE);
	}
}

static void gen6_check_mch_setup(struct drm_i915_private *i915)
{
	u32 tmp;

	tmp = intel_uncore_read(&i915->uncore, MCH_SSKPD);
	if (REG_FIELD_GET(SSKPD_WM0_MASK_SNB, tmp) != 12)
		drm_dbg_kms(&i915->drm,
			    "Wrong MCH_SSKPD value: 0x%08x This can cause underruns.\n",
			    tmp);
}

static void gen6_init_clock_gating(struct drm_i915_private *i915)
{
	u32 dspclk_gate = ILK_VRHUNIT_CLOCK_GATE_DISABLE;

	intel_uncore_write(&i915->uncore, ILK_DSPCLK_GATE_D, dspclk_gate);

	intel_uncore_rmw(&i915->uncore, ILK_DISPLAY_CHICKEN2, 0, ILK_ELPIN_409_SELECT);

	intel_uncore_write(&i915->uncore, GEN6_UCGCTL1,
			   intel_uncore_read(&i915->uncore, GEN6_UCGCTL1) |
			   GEN6_BLBUNIT_CLOCK_GATE_DISABLE |
			   GEN6_CSUNIT_CLOCK_GATE_DISABLE);

	 
	intel_uncore_write(&i915->uncore, GEN6_UCGCTL2,
			   GEN6_RCPBUNIT_CLOCK_GATE_DISABLE |
			   GEN6_RCCUNIT_CLOCK_GATE_DISABLE);

	 
	intel_uncore_write(&i915->uncore, ILK_DISPLAY_CHICKEN1,
			   intel_uncore_read(&i915->uncore, ILK_DISPLAY_CHICKEN1) |
			   ILK_FBCQ_DIS | ILK_PABSTRETCH_DIS);
	intel_uncore_write(&i915->uncore, ILK_DISPLAY_CHICKEN2,
			   intel_uncore_read(&i915->uncore, ILK_DISPLAY_CHICKEN2) |
			   ILK_DPARB_GATE | ILK_VSDPFD_FULL);
	intel_uncore_write(&i915->uncore, ILK_DSPCLK_GATE_D,
			   intel_uncore_read(&i915->uncore, ILK_DSPCLK_GATE_D) |
			   ILK_DPARBUNIT_CLOCK_GATE_ENABLE  |
			   ILK_DPFDUNIT_CLOCK_GATE_ENABLE);

	g4x_disable_trickle_feed(i915);

	cpt_init_clock_gating(i915);

	gen6_check_mch_setup(i915);
}

static void lpt_init_clock_gating(struct drm_i915_private *i915)
{
	 
	if (HAS_PCH_LPT_LP(i915))
		intel_uncore_rmw(&i915->uncore, SOUTH_DSPCLK_GATE_D,
				 0, PCH_LP_PARTITION_LEVEL_DISABLE);

	 
	intel_uncore_rmw(&i915->uncore, TRANS_CHICKEN1(PIPE_A),
			 0, TRANS_CHICKEN1_DP0UNIT_GC_DISABLE);
}

static void gen8_set_l3sqc_credits(struct drm_i915_private *i915,
				   int general_prio_credits,
				   int high_prio_credits)
{
	u32 misccpctl;
	u32 val;

	 
	misccpctl = intel_uncore_rmw(&i915->uncore, GEN7_MISCCPCTL,
				     GEN7_DOP_CLOCK_GATE_ENABLE, 0);

	val = intel_gt_mcr_read_any(to_gt(i915), GEN8_L3SQCREG1);
	val &= ~L3_PRIO_CREDITS_MASK;
	val |= L3_GENERAL_PRIO_CREDITS(general_prio_credits);
	val |= L3_HIGH_PRIO_CREDITS(high_prio_credits);
	intel_gt_mcr_multicast_write(to_gt(i915), GEN8_L3SQCREG1, val);

	 
	intel_gt_mcr_read_any(to_gt(i915), GEN8_L3SQCREG1);
	udelay(1);
	intel_uncore_write(&i915->uncore, GEN7_MISCCPCTL, misccpctl);
}

static void icl_init_clock_gating(struct drm_i915_private *i915)
{
	 
	intel_uncore_write(&i915->uncore, ILK_DPFC_CHICKEN(INTEL_FBC_A),
			   DPFC_CHICKEN_COMP_DUMMY_PIXEL);

	 
	intel_uncore_rmw(&i915->uncore, GEN8_CHICKEN_DCPR_1,
			 0, ICL_DELAY_PMRSP);
}

static void gen12lp_init_clock_gating(struct drm_i915_private *i915)
{
	 
	if (DISPLAY_VER(i915) == 12)
		intel_uncore_write(&i915->uncore, ILK_DPFC_CHICKEN(INTEL_FBC_A),
				   DPFC_CHICKEN_COMP_DUMMY_PIXEL);

	 
	if (DISPLAY_VER(i915) == 12)
		intel_uncore_rmw(&i915->uncore, CLKREQ_POLICY,
				 CLKREQ_POLICY_MEM_UP_OVRD, 0);
}

static void adlp_init_clock_gating(struct drm_i915_private *i915)
{
	gen12lp_init_clock_gating(i915);

	 
	intel_de_rmw(i915, GEN9_CLKGATE_DIS_5, 0, DPCE_GATING_DIS);

	 
	intel_de_rmw(i915, GEN8_CHICKEN_DCPR_1, DDI_CLOCK_REG_ACCESS, 0);
}

static void xehpsdv_init_clock_gating(struct drm_i915_private *i915)
{
	 
	if (IS_XEHPSDV_GRAPHICS_STEP(i915, STEP_A0, STEP_B0))
		intel_uncore_rmw(&i915->uncore, XEHP_CLOCK_GATE_DIS, 0, SGR_DIS);
}

static void dg2_init_clock_gating(struct drm_i915_private *i915)
{
	 
	intel_uncore_rmw(&i915->uncore, XEHP_CLOCK_GATE_DIS, 0,
			 SGSI_SIDECLK_DIS);

	 
	if (IS_DG2_GRAPHICS_STEP(i915, G10, STEP_A0, STEP_B0))
		intel_uncore_rmw(&i915->uncore, XEHP_CLOCK_GATE_DIS, 0,
				 SGR_DIS | SGGI_DIS);
}

static void pvc_init_clock_gating(struct drm_i915_private *i915)
{
	 
	if (IS_PVC_BD_STEP(i915, STEP_A0, STEP_B0))
		intel_uncore_rmw(&i915->uncore, XEHP_CLOCK_GATE_DIS, 0, SGR_DIS);

	 
	if (IS_PVC_BD_STEP(i915, STEP_A0, STEP_B0))
		intel_uncore_rmw(&i915->uncore, XEHP_CLOCK_GATE_DIS, 0, SGSI_SIDECLK_DIS);
}

static void cnp_init_clock_gating(struct drm_i915_private *i915)
{
	if (!HAS_PCH_CNP(i915))
		return;

	 
	intel_uncore_rmw(&i915->uncore, SOUTH_DSPCLK_GATE_D, 0, CNP_PWM_CGE_GATING_DISABLE);
}

static void cfl_init_clock_gating(struct drm_i915_private *i915)
{
	cnp_init_clock_gating(i915);
	gen9_init_clock_gating(i915);

	 
	intel_uncore_rmw(&i915->uncore, FBC_LLC_READ_CTRL, 0, FBC_LLC_FULLY_OPEN);

	 
	intel_uncore_rmw(&i915->uncore, DISP_ARB_CTL, 0, DISP_FBC_WM_DIS);

	 
	intel_uncore_rmw(&i915->uncore, ILK_DPFC_CHICKEN(INTEL_FBC_A),
			 0, DPFC_NUKE_ON_ANY_MODIFICATION);
}

static void kbl_init_clock_gating(struct drm_i915_private *i915)
{
	gen9_init_clock_gating(i915);

	 
	intel_uncore_rmw(&i915->uncore, FBC_LLC_READ_CTRL, 0, FBC_LLC_FULLY_OPEN);

	 
	if (IS_KABYLAKE(i915) && IS_GRAPHICS_STEP(i915, 0, STEP_C0))
		intel_uncore_rmw(&i915->uncore, GEN8_UCGCTL6,
				 0, GEN8_SDEUNIT_CLOCK_GATE_DISABLE);

	 
	if (IS_KABYLAKE(i915) && IS_GRAPHICS_STEP(i915, 0, STEP_C0))
		intel_uncore_rmw(&i915->uncore, GEN6_UCGCTL1,
				 0, GEN6_GAMUNIT_CLOCK_GATE_DISABLE);

	 
	intel_uncore_rmw(&i915->uncore, DISP_ARB_CTL, 0, DISP_FBC_WM_DIS);

	 
	intel_uncore_rmw(&i915->uncore, ILK_DPFC_CHICKEN(INTEL_FBC_A),
			 0, DPFC_NUKE_ON_ANY_MODIFICATION);
}

static void skl_init_clock_gating(struct drm_i915_private *i915)
{
	gen9_init_clock_gating(i915);

	 
	intel_uncore_rmw(&i915->uncore, GEN7_MISCCPCTL,
			 GEN7_DOP_CLOCK_GATE_ENABLE, 0);

	 
	intel_uncore_rmw(&i915->uncore, FBC_LLC_READ_CTRL, 0, FBC_LLC_FULLY_OPEN);

	 
	intel_uncore_rmw(&i915->uncore, DISP_ARB_CTL, 0, DISP_FBC_WM_DIS);

	 
	intel_uncore_rmw(&i915->uncore, ILK_DPFC_CHICKEN(INTEL_FBC_A),
			 0, DPFC_NUKE_ON_ANY_MODIFICATION);

	 
	intel_uncore_rmw(&i915->uncore, ILK_DPFC_CHICKEN(INTEL_FBC_A), 0, DPFC_DISABLE_DUMMY0);
}

static void bdw_init_clock_gating(struct drm_i915_private *i915)
{
	enum pipe pipe;

	 
	intel_uncore_rmw(&i915->uncore, CHICKEN_PIPESL_1(PIPE_A), 0, HSW_FBCQ_DIS);

	 
	intel_uncore_rmw(&i915->uncore, GAM_ECOCHK, 0, HSW_ECOCHK_ARB_PRIO_SOL);

	 
	intel_uncore_rmw(&i915->uncore, CHICKEN_PAR1_1, 0, HSW_MASK_VBL_TO_PIPE_IN_SRD);

	for_each_pipe(i915, pipe) {
		 
		intel_uncore_rmw(&i915->uncore, CHICKEN_PIPESL_1(pipe),
				 0, BDW_UNMASK_VBL_TO_REGS_IN_SRD);
	}

	 
	 
	intel_uncore_rmw(&i915->uncore, GEN7_FF_THREAD_MODE,
			 GEN8_FF_DS_REF_CNT_FFME | GEN7_FF_VS_REF_CNT_FFME, 0);

	intel_uncore_write(&i915->uncore, RING_PSMI_CTL(RENDER_RING_BASE),
			   _MASKED_BIT_ENABLE(GEN8_RC_SEMA_IDLE_MSG_DISABLE));

	 
	intel_uncore_rmw(&i915->uncore, GEN8_UCGCTL6, 0, GEN8_SDEUNIT_CLOCK_GATE_DISABLE);

	 
	gen8_set_l3sqc_credits(i915, 30, 2);

	 
	intel_uncore_rmw(&i915->uncore, CHICKEN_PAR2_1,
			 0, KVM_CONFIG_CHANGE_NOTIFICATION_SELECT);

	lpt_init_clock_gating(i915);

	 
	intel_uncore_rmw(&i915->uncore, GEN6_UCGCTL1, 0, GEN6_EU_TCUNIT_CLOCK_GATE_DISABLE);
}

static void hsw_init_clock_gating(struct drm_i915_private *i915)
{
	enum pipe pipe;

	 
	intel_uncore_rmw(&i915->uncore, CHICKEN_PIPESL_1(PIPE_A), 0, HSW_FBCQ_DIS);

	 
	intel_uncore_rmw(&i915->uncore, CHICKEN_PAR1_1, 0, HSW_MASK_VBL_TO_PIPE_IN_SRD);

	for_each_pipe(i915, pipe) {
		 
		intel_uncore_rmw(&i915->uncore, CHICKEN_PIPESL_1(pipe),
				 0, HSW_UNMASK_VBL_TO_REGS_IN_SRD);
	}

	 
	intel_uncore_rmw(&i915->uncore, GEN7_SQ_CHICKEN_MBCUNIT_CONFIG,
			 0, GEN7_SQ_CHICKEN_MBCUNIT_SQINTMOB);

	 
	intel_uncore_rmw(&i915->uncore, GAM_ECOCHK, 0, HSW_ECOCHK_ARB_PRIO_SOL);

	lpt_init_clock_gating(i915);
}

static void ivb_init_clock_gating(struct drm_i915_private *i915)
{
	intel_uncore_write(&i915->uncore, ILK_DSPCLK_GATE_D, ILK_VRHUNIT_CLOCK_GATE_DISABLE);

	 
	intel_uncore_rmw(&i915->uncore, ILK_DISPLAY_CHICKEN1, 0, ILK_FBCQ_DIS);

	 
	intel_uncore_write(&i915->uncore, IVB_CHICKEN3,
			   CHICKEN3_DGMG_REQ_OUT_FIX_DISABLE |
			   CHICKEN3_DGMG_DONE_FIX_DISABLE);

	if (IS_IVB_GT1(i915))
		intel_uncore_write(&i915->uncore, GEN7_ROW_CHICKEN2,
				   _MASKED_BIT_ENABLE(DOP_CLOCK_GATING_DISABLE));
	else {
		 
		intel_uncore_write(&i915->uncore, GEN7_ROW_CHICKEN2,
				   _MASKED_BIT_ENABLE(DOP_CLOCK_GATING_DISABLE));
		intel_uncore_write(&i915->uncore, GEN7_ROW_CHICKEN2_GT2,
				   _MASKED_BIT_ENABLE(DOP_CLOCK_GATING_DISABLE));
	}

	 
	intel_uncore_write(&i915->uncore, GEN6_UCGCTL2,
			   GEN6_RCZUNIT_CLOCK_GATE_DISABLE);

	 
	intel_uncore_rmw(&i915->uncore, GEN7_SQ_CHICKEN_MBCUNIT_CONFIG,
			 0, GEN7_SQ_CHICKEN_MBCUNIT_SQINTMOB);

	g4x_disable_trickle_feed(i915);

	intel_uncore_rmw(&i915->uncore, GEN6_MBCUNIT_SNPCR, GEN6_MBC_SNPCR_MASK,
			 GEN6_MBC_SNPCR_MED);

	if (!HAS_PCH_NOP(i915))
		cpt_init_clock_gating(i915);

	gen6_check_mch_setup(i915);
}

static void vlv_init_clock_gating(struct drm_i915_private *i915)
{
	 
	intel_uncore_write(&i915->uncore, IVB_CHICKEN3,
			   CHICKEN3_DGMG_REQ_OUT_FIX_DISABLE |
			   CHICKEN3_DGMG_DONE_FIX_DISABLE);

	 
	intel_uncore_write(&i915->uncore, GEN7_ROW_CHICKEN2,
			   _MASKED_BIT_ENABLE(DOP_CLOCK_GATING_DISABLE));

	 
	intel_uncore_rmw(&i915->uncore, GEN7_SQ_CHICKEN_MBCUNIT_CONFIG,
			 0, GEN7_SQ_CHICKEN_MBCUNIT_SQINTMOB);

	 
	intel_uncore_write(&i915->uncore, GEN6_UCGCTL2,
			   GEN6_RCZUNIT_CLOCK_GATE_DISABLE);

	 
	intel_uncore_rmw(&i915->uncore, GEN7_UCGCTL4, 0, GEN7_L3BANK2X_CLOCK_GATE_DISABLE);

	 
	intel_uncore_write(&i915->uncore, VLV_GUNIT_CLOCK_GATE, GCFG_DIS);
}

static void chv_init_clock_gating(struct drm_i915_private *i915)
{
	 
	 
	intel_uncore_rmw(&i915->uncore, GEN7_FF_THREAD_MODE,
			 GEN8_FF_DS_REF_CNT_FFME | GEN7_FF_VS_REF_CNT_FFME, 0);

	 
	intel_uncore_write(&i915->uncore, RING_PSMI_CTL(RENDER_RING_BASE),
			   _MASKED_BIT_ENABLE(GEN8_RC_SEMA_IDLE_MSG_DISABLE));

	 
	intel_uncore_rmw(&i915->uncore, GEN6_UCGCTL1, 0, GEN6_CSUNIT_CLOCK_GATE_DISABLE);

	 
	intel_uncore_rmw(&i915->uncore, GEN8_UCGCTL6, 0, GEN8_SDEUNIT_CLOCK_GATE_DISABLE);

	 
	gen8_set_l3sqc_credits(i915, 38, 2);
}

static void g4x_init_clock_gating(struct drm_i915_private *i915)
{
	u32 dspclk_gate;

	intel_uncore_write(&i915->uncore, RENCLK_GATE_D1, 0);
	intel_uncore_write(&i915->uncore, RENCLK_GATE_D2, VF_UNIT_CLOCK_GATE_DISABLE |
			   GS_UNIT_CLOCK_GATE_DISABLE |
			   CL_UNIT_CLOCK_GATE_DISABLE);
	intel_uncore_write(&i915->uncore, RAMCLK_GATE_D, 0);
	dspclk_gate = VRHUNIT_CLOCK_GATE_DISABLE |
		OVRUNIT_CLOCK_GATE_DISABLE |
		OVCUNIT_CLOCK_GATE_DISABLE;
	if (IS_GM45(i915))
		dspclk_gate |= DSSUNIT_CLOCK_GATE_DISABLE;
	intel_uncore_write(&i915->uncore, DSPCLK_GATE_D(i915), dspclk_gate);

	g4x_disable_trickle_feed(i915);
}

static void i965gm_init_clock_gating(struct drm_i915_private *i915)
{
	struct intel_uncore *uncore = &i915->uncore;

	intel_uncore_write(uncore, RENCLK_GATE_D1, I965_RCC_CLOCK_GATE_DISABLE);
	intel_uncore_write(uncore, RENCLK_GATE_D2, 0);
	intel_uncore_write(uncore, DSPCLK_GATE_D(i915), 0);
	intel_uncore_write(uncore, RAMCLK_GATE_D, 0);
	intel_uncore_write16(uncore, DEUC, 0);
	intel_uncore_write(uncore,
			   MI_ARB_STATE,
			   _MASKED_BIT_ENABLE(MI_ARB_DISPLAY_TRICKLE_FEED_DISABLE));
}

static void i965g_init_clock_gating(struct drm_i915_private *i915)
{
	intel_uncore_write(&i915->uncore, RENCLK_GATE_D1, I965_RCZ_CLOCK_GATE_DISABLE |
			   I965_RCC_CLOCK_GATE_DISABLE |
			   I965_RCPB_CLOCK_GATE_DISABLE |
			   I965_ISC_CLOCK_GATE_DISABLE |
			   I965_FBC_CLOCK_GATE_DISABLE);
	intel_uncore_write(&i915->uncore, RENCLK_GATE_D2, 0);
	intel_uncore_write(&i915->uncore, MI_ARB_STATE,
			   _MASKED_BIT_ENABLE(MI_ARB_DISPLAY_TRICKLE_FEED_DISABLE));
}

static void gen3_init_clock_gating(struct drm_i915_private *i915)
{
	u32 dstate = intel_uncore_read(&i915->uncore, D_STATE);

	dstate |= DSTATE_PLL_D3_OFF | DSTATE_GFX_CLOCK_GATING |
		DSTATE_DOT_CLOCK_GATING;
	intel_uncore_write(&i915->uncore, D_STATE, dstate);

	if (IS_PINEVIEW(i915))
		intel_uncore_write(&i915->uncore, ECOSKPD(RENDER_RING_BASE),
				   _MASKED_BIT_ENABLE(ECO_GATING_CX_ONLY));

	 
	intel_uncore_write(&i915->uncore, ECOSKPD(RENDER_RING_BASE),
			   _MASKED_BIT_DISABLE(ECO_FLIP_DONE));

	 
	intel_uncore_write(&i915->uncore, INSTPM, _MASKED_BIT_ENABLE(INSTPM_AGPBUSY_INT_EN));

	 
	intel_uncore_write(&i915->uncore, MI_ARB_STATE,
			   _MASKED_BIT_ENABLE(MI_ARB_C3_LP_WRITE_ENABLE));

	intel_uncore_write(&i915->uncore, MI_ARB_STATE,
			   _MASKED_BIT_ENABLE(MI_ARB_DISPLAY_TRICKLE_FEED_DISABLE));
}

static void i85x_init_clock_gating(struct drm_i915_private *i915)
{
	intel_uncore_write(&i915->uncore, RENCLK_GATE_D1, SV_CLOCK_GATE_DISABLE);

	 
	intel_uncore_write(&i915->uncore, MI_STATE, _MASKED_BIT_ENABLE(MI_AGPBUSY_INT_EN) |
			   _MASKED_BIT_DISABLE(MI_AGPBUSY_830_MODE));

	intel_uncore_write(&i915->uncore, MEM_MODE,
			   _MASKED_BIT_ENABLE(MEM_DISPLAY_TRICKLE_FEED_DISABLE));

	 
	intel_uncore_write(&i915->uncore, SCPD0,
			   _MASKED_BIT_ENABLE(SCPD_FBC_IGNORE_3D));
}

static void i830_init_clock_gating(struct drm_i915_private *i915)
{
	intel_uncore_write(&i915->uncore, MEM_MODE,
			   _MASKED_BIT_ENABLE(MEM_DISPLAY_A_TRICKLE_FEED_DISABLE) |
			   _MASKED_BIT_ENABLE(MEM_DISPLAY_B_TRICKLE_FEED_DISABLE));
}

void intel_clock_gating_init(struct drm_i915_private *i915)
{
	i915->clock_gating_funcs->init_clock_gating(i915);
}

static void nop_init_clock_gating(struct drm_i915_private *i915)
{
	drm_dbg_kms(&i915->drm,
		    "No clock gating settings or workarounds applied.\n");
}

#define CG_FUNCS(platform)						\
static const struct drm_i915_clock_gating_funcs platform##_clock_gating_funcs = { \
	.init_clock_gating = platform##_init_clock_gating,		\
}

CG_FUNCS(pvc);
CG_FUNCS(dg2);
CG_FUNCS(xehpsdv);
CG_FUNCS(adlp);
CG_FUNCS(gen12lp);
CG_FUNCS(icl);
CG_FUNCS(cfl);
CG_FUNCS(skl);
CG_FUNCS(kbl);
CG_FUNCS(bxt);
CG_FUNCS(glk);
CG_FUNCS(bdw);
CG_FUNCS(chv);
CG_FUNCS(hsw);
CG_FUNCS(ivb);
CG_FUNCS(vlv);
CG_FUNCS(gen6);
CG_FUNCS(ilk);
CG_FUNCS(g4x);
CG_FUNCS(i965gm);
CG_FUNCS(i965g);
CG_FUNCS(gen3);
CG_FUNCS(i85x);
CG_FUNCS(i830);
CG_FUNCS(nop);
#undef CG_FUNCS

 
void intel_clock_gating_hooks_init(struct drm_i915_private *i915)
{
	if (IS_METEORLAKE(i915))
		i915->clock_gating_funcs = &nop_clock_gating_funcs;
	else if (IS_PONTEVECCHIO(i915))
		i915->clock_gating_funcs = &pvc_clock_gating_funcs;
	else if (IS_DG2(i915))
		i915->clock_gating_funcs = &dg2_clock_gating_funcs;
	else if (IS_XEHPSDV(i915))
		i915->clock_gating_funcs = &xehpsdv_clock_gating_funcs;
	else if (IS_ALDERLAKE_P(i915))
		i915->clock_gating_funcs = &adlp_clock_gating_funcs;
	else if (GRAPHICS_VER(i915) == 12)
		i915->clock_gating_funcs = &gen12lp_clock_gating_funcs;
	else if (GRAPHICS_VER(i915) == 11)
		i915->clock_gating_funcs = &icl_clock_gating_funcs;
	else if (IS_COFFEELAKE(i915) || IS_COMETLAKE(i915))
		i915->clock_gating_funcs = &cfl_clock_gating_funcs;
	else if (IS_SKYLAKE(i915))
		i915->clock_gating_funcs = &skl_clock_gating_funcs;
	else if (IS_KABYLAKE(i915))
		i915->clock_gating_funcs = &kbl_clock_gating_funcs;
	else if (IS_BROXTON(i915))
		i915->clock_gating_funcs = &bxt_clock_gating_funcs;
	else if (IS_GEMINILAKE(i915))
		i915->clock_gating_funcs = &glk_clock_gating_funcs;
	else if (IS_BROADWELL(i915))
		i915->clock_gating_funcs = &bdw_clock_gating_funcs;
	else if (IS_CHERRYVIEW(i915))
		i915->clock_gating_funcs = &chv_clock_gating_funcs;
	else if (IS_HASWELL(i915))
		i915->clock_gating_funcs = &hsw_clock_gating_funcs;
	else if (IS_IVYBRIDGE(i915))
		i915->clock_gating_funcs = &ivb_clock_gating_funcs;
	else if (IS_VALLEYVIEW(i915))
		i915->clock_gating_funcs = &vlv_clock_gating_funcs;
	else if (GRAPHICS_VER(i915) == 6)
		i915->clock_gating_funcs = &gen6_clock_gating_funcs;
	else if (GRAPHICS_VER(i915) == 5)
		i915->clock_gating_funcs = &ilk_clock_gating_funcs;
	else if (IS_G4X(i915))
		i915->clock_gating_funcs = &g4x_clock_gating_funcs;
	else if (IS_I965GM(i915))
		i915->clock_gating_funcs = &i965gm_clock_gating_funcs;
	else if (IS_I965G(i915))
		i915->clock_gating_funcs = &i965g_clock_gating_funcs;
	else if (GRAPHICS_VER(i915) == 3)
		i915->clock_gating_funcs = &gen3_clock_gating_funcs;
	else if (IS_I85X(i915) || IS_I865G(i915))
		i915->clock_gating_funcs = &i85x_clock_gating_funcs;
	else if (GRAPHICS_VER(i915) == 2)
		i915->clock_gating_funcs = &i830_clock_gating_funcs;
	else {
		MISSING_CASE(INTEL_DEVID(i915));
		i915->clock_gating_funcs = &nop_clock_gating_funcs;
	}
}
