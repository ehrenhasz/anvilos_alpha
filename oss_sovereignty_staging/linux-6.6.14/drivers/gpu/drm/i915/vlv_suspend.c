
 

#include <linux/string_helpers.h>
#include <linux/kernel.h>

#include <drm/drm_print.h>

#include "i915_drv.h"
#include "i915_reg.h"
#include "i915_trace.h"
#include "i915_utils.h"
#include "intel_clock_gating.h"
#include "vlv_suspend.h"

#include "gt/intel_gt_regs.h"

struct vlv_s0ix_state {
	 
	u32 wr_watermark;
	u32 gfx_prio_ctrl;
	u32 arb_mode;
	u32 gfx_pend_tlb0;
	u32 gfx_pend_tlb1;
	u32 lra_limits[GEN7_LRA_LIMITS_REG_NUM];
	u32 media_max_req_count;
	u32 gfx_max_req_count;
	u32 render_hwsp;
	u32 ecochk;
	u32 bsd_hwsp;
	u32 blt_hwsp;
	u32 tlb_rd_addr;

	 
	u32 g3dctl;
	u32 gsckgctl;
	u32 mbctl;

	 
	u32 ucgctl1;
	u32 ucgctl3;
	u32 rcgctl1;
	u32 rcgctl2;
	u32 rstctl;
	u32 misccpctl;

	 
	u32 gfxpause;
	u32 rpdeuhwtc;
	u32 rpdeuc;
	u32 ecobus;
	u32 pwrdwnupctl;
	u32 rp_down_timeout;
	u32 rp_deucsw;
	u32 rcubmabdtmr;
	u32 rcedata;
	u32 spare2gh;

	 
	u32 gt_imr;
	u32 gt_ier;
	u32 pm_imr;
	u32 pm_ier;
	u32 gt_scratch[GEN7_GT_SCRATCH_REG_NUM];

	 
	u32 tilectl;
	u32 gt_fifoctl;
	u32 gtlc_wake_ctrl;
	u32 gtlc_survive;
	u32 pmwgicz;

	 
	u32 gu_ctl0;
	u32 gu_ctl1;
	u32 pcbr;
	u32 clock_gate_dis2;
};

 
static void vlv_save_gunit_s0ix_state(struct drm_i915_private *i915)
{
	struct vlv_s0ix_state *s = i915->vlv_s0ix_state;
	struct intel_uncore *uncore = &i915->uncore;
	int i;

	if (!s)
		return;

	 
	s->wr_watermark = intel_uncore_read(uncore, GEN7_WR_WATERMARK);
	s->gfx_prio_ctrl = intel_uncore_read(uncore, GEN7_GFX_PRIO_CTRL);
	s->arb_mode = intel_uncore_read(uncore, ARB_MODE);
	s->gfx_pend_tlb0 = intel_uncore_read(uncore, GEN7_GFX_PEND_TLB0);
	s->gfx_pend_tlb1 = intel_uncore_read(uncore, GEN7_GFX_PEND_TLB1);

	for (i = 0; i < ARRAY_SIZE(s->lra_limits); i++)
		s->lra_limits[i] = intel_uncore_read(uncore, GEN7_LRA_LIMITS(i));

	s->media_max_req_count = intel_uncore_read(uncore, GEN7_MEDIA_MAX_REQ_COUNT);
	s->gfx_max_req_count = intel_uncore_read(uncore, GEN7_GFX_MAX_REQ_COUNT);

	s->render_hwsp = intel_uncore_read(uncore, RENDER_HWS_PGA_GEN7);
	s->ecochk = intel_uncore_read(uncore, GAM_ECOCHK);
	s->bsd_hwsp = intel_uncore_read(uncore, BSD_HWS_PGA_GEN7);
	s->blt_hwsp = intel_uncore_read(uncore, BLT_HWS_PGA_GEN7);

	s->tlb_rd_addr = intel_uncore_read(uncore, GEN7_TLB_RD_ADDR);

	 
	s->g3dctl = intel_uncore_read(uncore, VLV_G3DCTL);
	s->gsckgctl = intel_uncore_read(uncore, VLV_GSCKGCTL);
	s->mbctl = intel_uncore_read(uncore, GEN6_MBCTL);

	 
	s->ucgctl1 = intel_uncore_read(uncore, GEN6_UCGCTL1);
	s->ucgctl3 = intel_uncore_read(uncore, GEN6_UCGCTL3);
	s->rcgctl1 = intel_uncore_read(uncore, GEN6_RCGCTL1);
	s->rcgctl2 = intel_uncore_read(uncore, GEN6_RCGCTL2);
	s->rstctl = intel_uncore_read(uncore, GEN6_RSTCTL);
	s->misccpctl = intel_uncore_read(uncore, GEN7_MISCCPCTL);

	 
	s->gfxpause = intel_uncore_read(uncore, GEN6_GFXPAUSE);
	s->rpdeuhwtc = intel_uncore_read(uncore, GEN6_RPDEUHWTC);
	s->rpdeuc = intel_uncore_read(uncore, GEN6_RPDEUC);
	s->ecobus = intel_uncore_read(uncore, ECOBUS);
	s->pwrdwnupctl = intel_uncore_read(uncore, VLV_PWRDWNUPCTL);
	s->rp_down_timeout = intel_uncore_read(uncore, GEN6_RP_DOWN_TIMEOUT);
	s->rp_deucsw = intel_uncore_read(uncore, GEN6_RPDEUCSW);
	s->rcubmabdtmr = intel_uncore_read(uncore, GEN6_RCUBMABDTMR);
	s->rcedata = intel_uncore_read(uncore, VLV_RCEDATA);
	s->spare2gh = intel_uncore_read(uncore, VLV_SPAREG2H);

	 
	s->gt_imr = intel_uncore_read(uncore, GTIMR);
	s->gt_ier = intel_uncore_read(uncore, GTIER);
	s->pm_imr = intel_uncore_read(uncore, GEN6_PMIMR);
	s->pm_ier = intel_uncore_read(uncore, GEN6_PMIER);

	for (i = 0; i < ARRAY_SIZE(s->gt_scratch); i++)
		s->gt_scratch[i] = intel_uncore_read(uncore, GEN7_GT_SCRATCH(i));

	 
	s->tilectl = intel_uncore_read(uncore, TILECTL);
	s->gt_fifoctl = intel_uncore_read(uncore, GTFIFOCTL);
	s->gtlc_wake_ctrl = intel_uncore_read(uncore, VLV_GTLC_WAKE_CTRL);
	s->gtlc_survive = intel_uncore_read(uncore, VLV_GTLC_SURVIVABILITY_REG);
	s->pmwgicz = intel_uncore_read(uncore, VLV_PMWGICZ);

	 
	s->gu_ctl0 = intel_uncore_read(uncore, VLV_GU_CTL0);
	s->gu_ctl1 = intel_uncore_read(uncore, VLV_GU_CTL1);
	s->pcbr = intel_uncore_read(uncore, VLV_PCBR);
	s->clock_gate_dis2 = intel_uncore_read(uncore, VLV_GUNIT_CLOCK_GATE2);

	 
}

static void vlv_restore_gunit_s0ix_state(struct drm_i915_private *i915)
{
	struct vlv_s0ix_state *s = i915->vlv_s0ix_state;
	struct intel_uncore *uncore = &i915->uncore;
	int i;

	if (!s)
		return;

	 
	intel_uncore_write(uncore, GEN7_WR_WATERMARK, s->wr_watermark);
	intel_uncore_write(uncore, GEN7_GFX_PRIO_CTRL, s->gfx_prio_ctrl);
	intel_uncore_write(uncore, ARB_MODE, s->arb_mode | (0xffff << 16));
	intel_uncore_write(uncore, GEN7_GFX_PEND_TLB0, s->gfx_pend_tlb0);
	intel_uncore_write(uncore, GEN7_GFX_PEND_TLB1, s->gfx_pend_tlb1);

	for (i = 0; i < ARRAY_SIZE(s->lra_limits); i++)
		intel_uncore_write(uncore, GEN7_LRA_LIMITS(i), s->lra_limits[i]);

	intel_uncore_write(uncore, GEN7_MEDIA_MAX_REQ_COUNT, s->media_max_req_count);
	intel_uncore_write(uncore, GEN7_GFX_MAX_REQ_COUNT, s->gfx_max_req_count);

	intel_uncore_write(uncore, RENDER_HWS_PGA_GEN7, s->render_hwsp);
	intel_uncore_write(uncore, GAM_ECOCHK, s->ecochk);
	intel_uncore_write(uncore, BSD_HWS_PGA_GEN7, s->bsd_hwsp);
	intel_uncore_write(uncore, BLT_HWS_PGA_GEN7, s->blt_hwsp);

	intel_uncore_write(uncore, GEN7_TLB_RD_ADDR, s->tlb_rd_addr);

	 
	intel_uncore_write(uncore, VLV_G3DCTL, s->g3dctl);
	intel_uncore_write(uncore, VLV_GSCKGCTL, s->gsckgctl);
	intel_uncore_write(uncore, GEN6_MBCTL, s->mbctl);

	 
	intel_uncore_write(uncore, GEN6_UCGCTL1, s->ucgctl1);
	intel_uncore_write(uncore, GEN6_UCGCTL3, s->ucgctl3);
	intel_uncore_write(uncore, GEN6_RCGCTL1, s->rcgctl1);
	intel_uncore_write(uncore, GEN6_RCGCTL2, s->rcgctl2);
	intel_uncore_write(uncore, GEN6_RSTCTL, s->rstctl);
	intel_uncore_write(uncore, GEN7_MISCCPCTL, s->misccpctl);

	 
	intel_uncore_write(uncore, GEN6_GFXPAUSE, s->gfxpause);
	intel_uncore_write(uncore, GEN6_RPDEUHWTC, s->rpdeuhwtc);
	intel_uncore_write(uncore, GEN6_RPDEUC, s->rpdeuc);
	intel_uncore_write(uncore, ECOBUS, s->ecobus);
	intel_uncore_write(uncore, VLV_PWRDWNUPCTL, s->pwrdwnupctl);
	intel_uncore_write(uncore, GEN6_RP_DOWN_TIMEOUT, s->rp_down_timeout);
	intel_uncore_write(uncore, GEN6_RPDEUCSW, s->rp_deucsw);
	intel_uncore_write(uncore, GEN6_RCUBMABDTMR, s->rcubmabdtmr);
	intel_uncore_write(uncore, VLV_RCEDATA, s->rcedata);
	intel_uncore_write(uncore, VLV_SPAREG2H, s->spare2gh);

	 
	intel_uncore_write(uncore, GTIMR, s->gt_imr);
	intel_uncore_write(uncore, GTIER, s->gt_ier);
	intel_uncore_write(uncore, GEN6_PMIMR, s->pm_imr);
	intel_uncore_write(uncore, GEN6_PMIER, s->pm_ier);

	for (i = 0; i < ARRAY_SIZE(s->gt_scratch); i++)
		intel_uncore_write(uncore, GEN7_GT_SCRATCH(i), s->gt_scratch[i]);

	 
	intel_uncore_write(uncore, TILECTL, s->tilectl);
	intel_uncore_write(uncore, GTFIFOCTL, s->gt_fifoctl);
	 
	intel_uncore_rmw(uncore, VLV_GTLC_WAKE_CTRL, ~VLV_GTLC_ALLOWWAKEREQ,
			 s->gtlc_wake_ctrl & ~VLV_GTLC_ALLOWWAKEREQ);

	intel_uncore_rmw(uncore, VLV_GTLC_SURVIVABILITY_REG, ~VLV_GFX_CLK_FORCE_ON_BIT,
			 s->gtlc_survive & ~VLV_GFX_CLK_FORCE_ON_BIT);

	intel_uncore_write(uncore, VLV_PMWGICZ, s->pmwgicz);

	 
	intel_uncore_write(uncore, VLV_GU_CTL0, s->gu_ctl0);
	intel_uncore_write(uncore, VLV_GU_CTL1, s->gu_ctl1);
	intel_uncore_write(uncore, VLV_PCBR, s->pcbr);
	intel_uncore_write(uncore, VLV_GUNIT_CLOCK_GATE2, s->clock_gate_dis2);
}

static int vlv_wait_for_pw_status(struct drm_i915_private *i915,
				  u32 mask, u32 val)
{
	i915_reg_t reg = VLV_GTLC_PW_STATUS;
	u32 reg_value;
	int ret;

	 
	ret = wait_for(((reg_value =
			 intel_uncore_read_notrace(&i915->uncore, reg)) & mask)
		       == val, 3);

	 
	trace_i915_reg_rw(false, reg, reg_value, sizeof(reg_value), true);

	return ret;
}

static int vlv_force_gfx_clock(struct drm_i915_private *i915, bool force_on)
{
	struct intel_uncore *uncore = &i915->uncore;
	int err;

	intel_uncore_rmw(uncore, VLV_GTLC_SURVIVABILITY_REG, VLV_GFX_CLK_FORCE_ON_BIT,
			 force_on ? VLV_GFX_CLK_FORCE_ON_BIT : 0);

	if (!force_on)
		return 0;

	err = intel_wait_for_register(uncore,
				      VLV_GTLC_SURVIVABILITY_REG,
				      VLV_GFX_CLK_STATUS_BIT,
				      VLV_GFX_CLK_STATUS_BIT,
				      20);
	if (err)
		drm_err(&i915->drm,
			"timeout waiting for GFX clock force-on (%08x)\n",
			intel_uncore_read(uncore, VLV_GTLC_SURVIVABILITY_REG));

	return err;
}

static int vlv_allow_gt_wake(struct drm_i915_private *i915, bool allow)
{
	struct intel_uncore *uncore = &i915->uncore;
	u32 mask;
	u32 val;
	int err;

	intel_uncore_rmw(uncore, VLV_GTLC_WAKE_CTRL, VLV_GTLC_ALLOWWAKEREQ,
			 allow ? VLV_GTLC_ALLOWWAKEREQ : 0);
	intel_uncore_posting_read(uncore, VLV_GTLC_WAKE_CTRL);

	mask = VLV_GTLC_ALLOWWAKEACK;
	val = allow ? mask : 0;

	err = vlv_wait_for_pw_status(i915, mask, val);
	if (err)
		drm_err(&i915->drm, "timeout disabling GT waking\n");

	return err;
}

static void vlv_wait_for_gt_wells(struct drm_i915_private *dev_priv,
				  bool wait_for_on)
{
	u32 mask;
	u32 val;

	mask = VLV_GTLC_PW_MEDIA_STATUS_MASK | VLV_GTLC_PW_RENDER_STATUS_MASK;
	val = wait_for_on ? mask : 0;

	 
	if (vlv_wait_for_pw_status(dev_priv, mask, val))
		drm_dbg(&dev_priv->drm,
			"timeout waiting for GT wells to go %s\n",
			str_on_off(wait_for_on));
}

static void vlv_check_no_gt_access(struct drm_i915_private *i915)
{
	struct intel_uncore *uncore = &i915->uncore;

	if (!(intel_uncore_read(uncore, VLV_GTLC_PW_STATUS) & VLV_GTLC_ALLOWWAKEERR))
		return;

	drm_dbg(&i915->drm, "GT register access while GT waking disabled\n");
	intel_uncore_write(uncore, VLV_GTLC_PW_STATUS, VLV_GTLC_ALLOWWAKEERR);
}

int vlv_suspend_complete(struct drm_i915_private *dev_priv)
{
	u32 mask;
	int err;

	if (!IS_VALLEYVIEW(dev_priv) && !IS_CHERRYVIEW(dev_priv))
		return 0;

	 
	vlv_wait_for_gt_wells(dev_priv, false);

	mask = VLV_GTLC_RENDER_CTX_EXISTS | VLV_GTLC_MEDIA_CTX_EXISTS;
	drm_WARN_ON(&dev_priv->drm,
		    (intel_uncore_read(&dev_priv->uncore, VLV_GTLC_WAKE_CTRL) & mask) != mask);

	vlv_check_no_gt_access(dev_priv);

	err = vlv_force_gfx_clock(dev_priv, true);
	if (err)
		goto err1;

	err = vlv_allow_gt_wake(dev_priv, false);
	if (err)
		goto err2;

	vlv_save_gunit_s0ix_state(dev_priv);

	err = vlv_force_gfx_clock(dev_priv, false);
	if (err)
		goto err2;

	return 0;

err2:
	 
	vlv_allow_gt_wake(dev_priv, true);
err1:
	vlv_force_gfx_clock(dev_priv, false);

	return err;
}

int vlv_resume_prepare(struct drm_i915_private *dev_priv, bool rpm_resume)
{
	int err;
	int ret;

	if (!IS_VALLEYVIEW(dev_priv) && !IS_CHERRYVIEW(dev_priv))
		return 0;

	 
	ret = vlv_force_gfx_clock(dev_priv, true);

	vlv_restore_gunit_s0ix_state(dev_priv);

	err = vlv_allow_gt_wake(dev_priv, true);
	if (!ret)
		ret = err;

	err = vlv_force_gfx_clock(dev_priv, false);
	if (!ret)
		ret = err;

	vlv_check_no_gt_access(dev_priv);

	if (rpm_resume)
		intel_clock_gating_init(dev_priv);

	return ret;
}

int vlv_suspend_init(struct drm_i915_private *i915)
{
	if (!IS_VALLEYVIEW(i915))
		return 0;

	 
	i915->vlv_s0ix_state = kmalloc(sizeof(*i915->vlv_s0ix_state),
				       GFP_KERNEL);
	if (!i915->vlv_s0ix_state)
		return -ENOMEM;

	return 0;
}

void vlv_suspend_cleanup(struct drm_i915_private *i915)
{
	if (!i915->vlv_s0ix_state)
		return;

	kfree(i915->vlv_s0ix_state);
	i915->vlv_s0ix_state = NULL;
}
