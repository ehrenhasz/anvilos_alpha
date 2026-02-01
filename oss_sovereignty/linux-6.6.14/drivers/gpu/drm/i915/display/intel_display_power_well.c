
 

#include "i915_drv.h"
#include "i915_irq.h"
#include "i915_reg.h"
#include "intel_backlight_regs.h"
#include "intel_combo_phy.h"
#include "intel_combo_phy_regs.h"
#include "intel_crt.h"
#include "intel_de.h"
#include "intel_display_irq.h"
#include "intel_display_power_well.h"
#include "intel_display_types.h"
#include "intel_dkl_phy.h"
#include "intel_dkl_phy_regs.h"
#include "intel_dmc.h"
#include "intel_dp_aux_regs.h"
#include "intel_dpio_phy.h"
#include "intel_dpll.h"
#include "intel_hotplug.h"
#include "intel_pcode.h"
#include "intel_pps.h"
#include "intel_tc.h"
#include "intel_vga.h"
#include "skl_watermark.h"
#include "vlv_sideband.h"
#include "vlv_sideband_reg.h"

struct i915_power_well_regs {
	i915_reg_t bios;
	i915_reg_t driver;
	i915_reg_t kvmr;
	i915_reg_t debug;
};

struct i915_power_well_ops {
	const struct i915_power_well_regs *regs;
	 
	void (*sync_hw)(struct drm_i915_private *i915,
			struct i915_power_well *power_well);
	 
	void (*enable)(struct drm_i915_private *i915,
		       struct i915_power_well *power_well);
	 
	void (*disable)(struct drm_i915_private *i915,
			struct i915_power_well *power_well);
	 
	bool (*is_enabled)(struct drm_i915_private *i915,
			   struct i915_power_well *power_well);
};

static const struct i915_power_well_instance *
i915_power_well_instance(const struct i915_power_well *power_well)
{
	return &power_well->desc->instances->list[power_well->instance_idx];
}

struct i915_power_well *
lookup_power_well(struct drm_i915_private *i915,
		  enum i915_power_well_id power_well_id)
{
	struct i915_power_well *power_well;

	for_each_power_well(i915, power_well)
		if (i915_power_well_instance(power_well)->id == power_well_id)
			return power_well;

	 
	drm_WARN(&i915->drm, 1,
		 "Power well %d not defined for this platform\n",
		 power_well_id);
	return &i915->display.power.domains.power_wells[0];
}

void intel_power_well_enable(struct drm_i915_private *i915,
			     struct i915_power_well *power_well)
{
	drm_dbg_kms(&i915->drm, "enabling %s\n", intel_power_well_name(power_well));
	power_well->desc->ops->enable(i915, power_well);
	power_well->hw_enabled = true;
}

void intel_power_well_disable(struct drm_i915_private *i915,
			      struct i915_power_well *power_well)
{
	drm_dbg_kms(&i915->drm, "disabling %s\n", intel_power_well_name(power_well));
	power_well->hw_enabled = false;
	power_well->desc->ops->disable(i915, power_well);
}

void intel_power_well_sync_hw(struct drm_i915_private *i915,
			      struct i915_power_well *power_well)
{
	power_well->desc->ops->sync_hw(i915, power_well);
	power_well->hw_enabled =
		power_well->desc->ops->is_enabled(i915, power_well);
}

void intel_power_well_get(struct drm_i915_private *i915,
			  struct i915_power_well *power_well)
{
	if (!power_well->count++)
		intel_power_well_enable(i915, power_well);
}

void intel_power_well_put(struct drm_i915_private *i915,
			  struct i915_power_well *power_well)
{
	drm_WARN(&i915->drm, !power_well->count,
		 "Use count on power well %s is already zero",
		 i915_power_well_instance(power_well)->name);

	if (!--power_well->count)
		intel_power_well_disable(i915, power_well);
}

bool intel_power_well_is_enabled(struct drm_i915_private *i915,
				 struct i915_power_well *power_well)
{
	return power_well->desc->ops->is_enabled(i915, power_well);
}

bool intel_power_well_is_enabled_cached(struct i915_power_well *power_well)
{
	return power_well->hw_enabled;
}

bool intel_display_power_well_is_enabled(struct drm_i915_private *dev_priv,
					 enum i915_power_well_id power_well_id)
{
	struct i915_power_well *power_well;

	power_well = lookup_power_well(dev_priv, power_well_id);

	return intel_power_well_is_enabled(dev_priv, power_well);
}

bool intel_power_well_is_always_on(struct i915_power_well *power_well)
{
	return power_well->desc->always_on;
}

const char *intel_power_well_name(struct i915_power_well *power_well)
{
	return i915_power_well_instance(power_well)->name;
}

struct intel_power_domain_mask *intel_power_well_domains(struct i915_power_well *power_well)
{
	return &power_well->domains;
}

int intel_power_well_refcount(struct i915_power_well *power_well)
{
	return power_well->count;
}

 
static void hsw_power_well_post_enable(struct drm_i915_private *dev_priv,
				       u8 irq_pipe_mask, bool has_vga)
{
	if (has_vga)
		intel_vga_reset_io_mem(dev_priv);

	if (irq_pipe_mask)
		gen8_irq_power_well_post_enable(dev_priv, irq_pipe_mask);
}

static void hsw_power_well_pre_disable(struct drm_i915_private *dev_priv,
				       u8 irq_pipe_mask)
{
	if (irq_pipe_mask)
		gen8_irq_power_well_pre_disable(dev_priv, irq_pipe_mask);
}

#define ICL_AUX_PW_TO_CH(pw_idx)	\
	((pw_idx) - ICL_PW_CTL_IDX_AUX_A + AUX_CH_A)

#define ICL_TBT_AUX_PW_TO_CH(pw_idx)	\
	((pw_idx) - ICL_PW_CTL_IDX_AUX_TBT1 + AUX_CH_C)

static enum aux_ch icl_aux_pw_to_ch(const struct i915_power_well *power_well)
{
	int pw_idx = i915_power_well_instance(power_well)->hsw.idx;

	return power_well->desc->is_tc_tbt ? ICL_TBT_AUX_PW_TO_CH(pw_idx) :
					     ICL_AUX_PW_TO_CH(pw_idx);
}

static struct intel_digital_port *
aux_ch_to_digital_port(struct drm_i915_private *dev_priv,
		       enum aux_ch aux_ch)
{
	struct intel_digital_port *dig_port = NULL;
	struct intel_encoder *encoder;

	for_each_intel_encoder(&dev_priv->drm, encoder) {
		 
		if (encoder->type == INTEL_OUTPUT_DP_MST)
			continue;

		dig_port = enc_to_dig_port(encoder);
		if (!dig_port)
			continue;

		if (dig_port->aux_ch != aux_ch) {
			dig_port = NULL;
			continue;
		}

		break;
	}

	return dig_port;
}

static enum phy icl_aux_pw_to_phy(struct drm_i915_private *i915,
				  const struct i915_power_well *power_well)
{
	enum aux_ch aux_ch = icl_aux_pw_to_ch(power_well);
	struct intel_digital_port *dig_port = aux_ch_to_digital_port(i915, aux_ch);

	return intel_port_to_phy(i915, dig_port->base.port);
}

static void hsw_wait_for_power_well_enable(struct drm_i915_private *dev_priv,
					   struct i915_power_well *power_well,
					   bool timeout_expected)
{
	const struct i915_power_well_regs *regs = power_well->desc->ops->regs;
	int pw_idx = i915_power_well_instance(power_well)->hsw.idx;
	int timeout = power_well->desc->enable_timeout ? : 1;

	 
	if (IS_DG2(dev_priv) && power_well->desc->fixed_enable_delay) {
		usleep_range(600, 1200);
		return;
	}

	 
	if (intel_de_wait_for_set(dev_priv, regs->driver,
				  HSW_PWR_WELL_CTL_STATE(pw_idx), timeout)) {
		drm_dbg_kms(&dev_priv->drm, "%s power well enable timeout\n",
			    intel_power_well_name(power_well));

		drm_WARN_ON(&dev_priv->drm, !timeout_expected);

	}
}

static u32 hsw_power_well_requesters(struct drm_i915_private *dev_priv,
				     const struct i915_power_well_regs *regs,
				     int pw_idx)
{
	u32 req_mask = HSW_PWR_WELL_CTL_REQ(pw_idx);
	u32 ret;

	ret = intel_de_read(dev_priv, regs->bios) & req_mask ? 1 : 0;
	ret |= intel_de_read(dev_priv, regs->driver) & req_mask ? 2 : 0;
	if (regs->kvmr.reg)
		ret |= intel_de_read(dev_priv, regs->kvmr) & req_mask ? 4 : 0;
	ret |= intel_de_read(dev_priv, regs->debug) & req_mask ? 8 : 0;

	return ret;
}

static void hsw_wait_for_power_well_disable(struct drm_i915_private *dev_priv,
					    struct i915_power_well *power_well)
{
	const struct i915_power_well_regs *regs = power_well->desc->ops->regs;
	int pw_idx = i915_power_well_instance(power_well)->hsw.idx;
	bool disabled;
	u32 reqs;

	 
	wait_for((disabled = !(intel_de_read(dev_priv, regs->driver) &
			       HSW_PWR_WELL_CTL_STATE(pw_idx))) ||
		 (reqs = hsw_power_well_requesters(dev_priv, regs, pw_idx)), 1);
	if (disabled)
		return;

	drm_dbg_kms(&dev_priv->drm,
		    "%s forced on (bios:%d driver:%d kvmr:%d debug:%d)\n",
		    intel_power_well_name(power_well),
		    !!(reqs & 1), !!(reqs & 2), !!(reqs & 4), !!(reqs & 8));
}

static void gen9_wait_for_power_well_fuses(struct drm_i915_private *dev_priv,
					   enum skl_power_gate pg)
{
	 
	drm_WARN_ON(&dev_priv->drm,
		    intel_de_wait_for_set(dev_priv, SKL_FUSE_STATUS,
					  SKL_FUSE_PG_DIST_STATUS(pg), 1));
}

static void hsw_power_well_enable(struct drm_i915_private *dev_priv,
				  struct i915_power_well *power_well)
{
	const struct i915_power_well_regs *regs = power_well->desc->ops->regs;
	int pw_idx = i915_power_well_instance(power_well)->hsw.idx;

	if (power_well->desc->has_fuses) {
		enum skl_power_gate pg;

		pg = DISPLAY_VER(dev_priv) >= 11 ? ICL_PW_CTL_IDX_TO_PG(pw_idx) :
						 SKL_PW_CTL_IDX_TO_PG(pw_idx);

		 
		if (IS_ALDERLAKE_P(dev_priv) && pg == SKL_PG1)
			intel_de_rmw(dev_priv, GEN8_CHICKEN_DCPR_1, 0, DISABLE_FLR_SRC);

		 
		if (pg == SKL_PG1)
			gen9_wait_for_power_well_fuses(dev_priv, SKL_PG0);
	}

	intel_de_rmw(dev_priv, regs->driver, 0, HSW_PWR_WELL_CTL_REQ(pw_idx));

	hsw_wait_for_power_well_enable(dev_priv, power_well, false);

	if (power_well->desc->has_fuses) {
		enum skl_power_gate pg;

		pg = DISPLAY_VER(dev_priv) >= 11 ? ICL_PW_CTL_IDX_TO_PG(pw_idx) :
						 SKL_PW_CTL_IDX_TO_PG(pw_idx);
		gen9_wait_for_power_well_fuses(dev_priv, pg);
	}

	hsw_power_well_post_enable(dev_priv,
				   power_well->desc->irq_pipe_mask,
				   power_well->desc->has_vga);
}

static void hsw_power_well_disable(struct drm_i915_private *dev_priv,
				   struct i915_power_well *power_well)
{
	const struct i915_power_well_regs *regs = power_well->desc->ops->regs;
	int pw_idx = i915_power_well_instance(power_well)->hsw.idx;

	hsw_power_well_pre_disable(dev_priv,
				   power_well->desc->irq_pipe_mask);

	intel_de_rmw(dev_priv, regs->driver, HSW_PWR_WELL_CTL_REQ(pw_idx), 0);
	hsw_wait_for_power_well_disable(dev_priv, power_well);
}

static bool intel_port_is_edp(struct drm_i915_private *i915, enum port port)
{
	struct intel_encoder *encoder;

	for_each_intel_encoder(&i915->drm, encoder) {
		if (encoder->type == INTEL_OUTPUT_EDP &&
		    encoder->port == port)
			return true;
	}

	return false;
}

static void
icl_combo_phy_aux_power_well_enable(struct drm_i915_private *dev_priv,
				    struct i915_power_well *power_well)
{
	const struct i915_power_well_regs *regs = power_well->desc->ops->regs;
	int pw_idx = i915_power_well_instance(power_well)->hsw.idx;
	enum phy phy = icl_aux_pw_to_phy(dev_priv, power_well);

	drm_WARN_ON(&dev_priv->drm, !IS_ICELAKE(dev_priv));

	intel_de_rmw(dev_priv, regs->driver, 0, HSW_PWR_WELL_CTL_REQ(pw_idx));

	if (DISPLAY_VER(dev_priv) < 12)
		intel_de_rmw(dev_priv, ICL_PORT_CL_DW12(phy),
			     0, ICL_LANE_ENABLE_AUX);

	hsw_wait_for_power_well_enable(dev_priv, power_well, false);

	 
	if (pw_idx >= ICL_PW_CTL_IDX_AUX_A && pw_idx <= ICL_PW_CTL_IDX_AUX_B &&
	    !intel_port_is_edp(dev_priv, (enum port)phy))
		intel_de_rmw(dev_priv, ICL_AUX_ANAOVRD1(pw_idx),
			     0, ICL_AUX_ANAOVRD1_ENABLE | ICL_AUX_ANAOVRD1_LDO_BYPASS);
}

static void
icl_combo_phy_aux_power_well_disable(struct drm_i915_private *dev_priv,
				     struct i915_power_well *power_well)
{
	const struct i915_power_well_regs *regs = power_well->desc->ops->regs;
	int pw_idx = i915_power_well_instance(power_well)->hsw.idx;
	enum phy phy = icl_aux_pw_to_phy(dev_priv, power_well);

	drm_WARN_ON(&dev_priv->drm, !IS_ICELAKE(dev_priv));

	intel_de_rmw(dev_priv, ICL_PORT_CL_DW12(phy), ICL_LANE_ENABLE_AUX, 0);

	intel_de_rmw(dev_priv, regs->driver, HSW_PWR_WELL_CTL_REQ(pw_idx), 0);

	hsw_wait_for_power_well_disable(dev_priv, power_well);
}

#if IS_ENABLED(CONFIG_DRM_I915_DEBUG_RUNTIME_PM)

static void icl_tc_port_assert_ref_held(struct drm_i915_private *dev_priv,
					struct i915_power_well *power_well,
					struct intel_digital_port *dig_port)
{
	if (drm_WARN_ON(&dev_priv->drm, !dig_port))
		return;

	if (DISPLAY_VER(dev_priv) == 11 && intel_tc_cold_requires_aux_pw(dig_port))
		return;

	drm_WARN_ON(&dev_priv->drm, !intel_tc_port_ref_held(dig_port));
}

#else

static void icl_tc_port_assert_ref_held(struct drm_i915_private *dev_priv,
					struct i915_power_well *power_well,
					struct intel_digital_port *dig_port)
{
}

#endif

#define TGL_AUX_PW_TO_TC_PORT(pw_idx)	((pw_idx) - TGL_PW_CTL_IDX_AUX_TC1)

static void icl_tc_cold_exit(struct drm_i915_private *i915)
{
	int ret, tries = 0;

	while (1) {
		ret = snb_pcode_write_timeout(&i915->uncore, ICL_PCODE_EXIT_TCCOLD, 0,
					      250, 1);
		if (ret != -EAGAIN || ++tries == 3)
			break;
		msleep(1);
	}

	 
	if (!ret)
		msleep(1);

	 
	drm_dbg_kms(&i915->drm, "TC cold block %s\n", ret ? "failed" :
		    "succeeded");
}

static void
icl_tc_phy_aux_power_well_enable(struct drm_i915_private *dev_priv,
				 struct i915_power_well *power_well)
{
	enum aux_ch aux_ch = icl_aux_pw_to_ch(power_well);
	struct intel_digital_port *dig_port = aux_ch_to_digital_port(dev_priv, aux_ch);
	const struct i915_power_well_regs *regs = power_well->desc->ops->regs;
	bool is_tbt = power_well->desc->is_tc_tbt;
	bool timeout_expected;

	icl_tc_port_assert_ref_held(dev_priv, power_well, dig_port);

	intel_de_rmw(dev_priv, DP_AUX_CH_CTL(aux_ch),
		     DP_AUX_CH_CTL_TBT_IO, is_tbt ? DP_AUX_CH_CTL_TBT_IO : 0);

	intel_de_rmw(dev_priv, regs->driver,
		     0,
		     HSW_PWR_WELL_CTL_REQ(i915_power_well_instance(power_well)->hsw.idx));

	 
	timeout_expected = is_tbt || intel_tc_cold_requires_aux_pw(dig_port);
	if (DISPLAY_VER(dev_priv) == 11 && intel_tc_cold_requires_aux_pw(dig_port))
		icl_tc_cold_exit(dev_priv);

	hsw_wait_for_power_well_enable(dev_priv, power_well, timeout_expected);

	if (DISPLAY_VER(dev_priv) >= 12 && !is_tbt) {
		enum tc_port tc_port;

		tc_port = TGL_AUX_PW_TO_TC_PORT(i915_power_well_instance(power_well)->hsw.idx);

		if (wait_for(intel_dkl_phy_read(dev_priv, DKL_CMN_UC_DW_27(tc_port)) &
			     DKL_CMN_UC_DW27_UC_HEALTH, 1))
			drm_warn(&dev_priv->drm,
				 "Timeout waiting TC uC health\n");
	}
}

static void
icl_aux_power_well_enable(struct drm_i915_private *dev_priv,
			  struct i915_power_well *power_well)
{
	enum phy phy = icl_aux_pw_to_phy(dev_priv, power_well);

	if (intel_phy_is_tc(dev_priv, phy))
		return icl_tc_phy_aux_power_well_enable(dev_priv, power_well);
	else if (IS_ICELAKE(dev_priv))
		return icl_combo_phy_aux_power_well_enable(dev_priv,
							   power_well);
	else
		return hsw_power_well_enable(dev_priv, power_well);
}

static void
icl_aux_power_well_disable(struct drm_i915_private *dev_priv,
			   struct i915_power_well *power_well)
{
	enum phy phy = icl_aux_pw_to_phy(dev_priv, power_well);

	if (intel_phy_is_tc(dev_priv, phy))
		return hsw_power_well_disable(dev_priv, power_well);
	else if (IS_ICELAKE(dev_priv))
		return icl_combo_phy_aux_power_well_disable(dev_priv,
							    power_well);
	else
		return hsw_power_well_disable(dev_priv, power_well);
}

 
static bool hsw_power_well_enabled(struct drm_i915_private *dev_priv,
				   struct i915_power_well *power_well)
{
	const struct i915_power_well_regs *regs = power_well->desc->ops->regs;
	enum i915_power_well_id id = i915_power_well_instance(power_well)->id;
	int pw_idx = i915_power_well_instance(power_well)->hsw.idx;
	u32 mask = HSW_PWR_WELL_CTL_REQ(pw_idx) |
		   HSW_PWR_WELL_CTL_STATE(pw_idx);
	u32 val;

	val = intel_de_read(dev_priv, regs->driver);

	 
	if (DISPLAY_VER(dev_priv) == 9 && !IS_BROXTON(dev_priv) &&
	    (id == SKL_DISP_PW_1 || id == SKL_DISP_PW_MISC_IO))
		val |= intel_de_read(dev_priv, regs->bios);

	return (val & mask) == mask;
}

static void assert_can_enable_dc9(struct drm_i915_private *dev_priv)
{
	drm_WARN_ONCE(&dev_priv->drm,
		      (intel_de_read(dev_priv, DC_STATE_EN) & DC_STATE_EN_DC9),
		      "DC9 already programmed to be enabled.\n");
	drm_WARN_ONCE(&dev_priv->drm,
		      intel_de_read(dev_priv, DC_STATE_EN) &
		      DC_STATE_EN_UPTO_DC5,
		      "DC5 still not disabled to enable DC9.\n");
	drm_WARN_ONCE(&dev_priv->drm,
		      intel_de_read(dev_priv, HSW_PWR_WELL_CTL2) &
		      HSW_PWR_WELL_CTL_REQ(SKL_PW_CTL_IDX_PW_2),
		      "Power well 2 on.\n");
	drm_WARN_ONCE(&dev_priv->drm, intel_irqs_enabled(dev_priv),
		      "Interrupts not disabled yet.\n");

	  
}

static void assert_can_disable_dc9(struct drm_i915_private *dev_priv)
{
	drm_WARN_ONCE(&dev_priv->drm, intel_irqs_enabled(dev_priv),
		      "Interrupts not disabled yet.\n");
	drm_WARN_ONCE(&dev_priv->drm,
		      intel_de_read(dev_priv, DC_STATE_EN) &
		      DC_STATE_EN_UPTO_DC5,
		      "DC5 still not disabled.\n");

	  
}

static void gen9_write_dc_state(struct drm_i915_private *dev_priv,
				u32 state)
{
	int rewrites = 0;
	int rereads = 0;
	u32 v;

	intel_de_write(dev_priv, DC_STATE_EN, state);

	 
	do  {
		v = intel_de_read(dev_priv, DC_STATE_EN);

		if (v != state) {
			intel_de_write(dev_priv, DC_STATE_EN, state);
			rewrites++;
			rereads = 0;
		} else if (rereads++ > 5) {
			break;
		}

	} while (rewrites < 100);

	if (v != state)
		drm_err(&dev_priv->drm,
			"Writing dc state to 0x%x failed, now 0x%x\n",
			state, v);

	 
	if (rewrites > 1)
		drm_dbg_kms(&dev_priv->drm,
			    "Rewrote dc state to 0x%x %d times\n",
			    state, rewrites);
}

static u32 gen9_dc_mask(struct drm_i915_private *dev_priv)
{
	u32 mask;

	mask = DC_STATE_EN_UPTO_DC5;

	if (DISPLAY_VER(dev_priv) >= 12)
		mask |= DC_STATE_EN_DC3CO | DC_STATE_EN_UPTO_DC6
					  | DC_STATE_EN_DC9;
	else if (DISPLAY_VER(dev_priv) == 11)
		mask |= DC_STATE_EN_UPTO_DC6 | DC_STATE_EN_DC9;
	else if (IS_GEMINILAKE(dev_priv) || IS_BROXTON(dev_priv))
		mask |= DC_STATE_EN_DC9;
	else
		mask |= DC_STATE_EN_UPTO_DC6;

	return mask;
}

void gen9_sanitize_dc_state(struct drm_i915_private *i915)
{
	struct i915_power_domains *power_domains = &i915->display.power.domains;
	u32 val;

	if (!HAS_DISPLAY(i915))
		return;

	val = intel_de_read(i915, DC_STATE_EN) & gen9_dc_mask(i915);

	drm_dbg_kms(&i915->drm,
		    "Resetting DC state tracking from %02x to %02x\n",
		    power_domains->dc_state, val);
	power_domains->dc_state = val;
}

 
void gen9_set_dc_state(struct drm_i915_private *dev_priv, u32 state)
{
	struct i915_power_domains *power_domains = &dev_priv->display.power.domains;
	u32 val;
	u32 mask;

	if (!HAS_DISPLAY(dev_priv))
		return;

	if (drm_WARN_ON_ONCE(&dev_priv->drm,
			     state & ~power_domains->allowed_dc_mask))
		state &= power_domains->allowed_dc_mask;

	val = intel_de_read(dev_priv, DC_STATE_EN);
	mask = gen9_dc_mask(dev_priv);
	drm_dbg_kms(&dev_priv->drm, "Setting DC state from %02x to %02x\n",
		    val & mask, state);

	 
	if ((val & mask) != power_domains->dc_state)
		drm_err(&dev_priv->drm, "DC state mismatch (0x%x -> 0x%x)\n",
			power_domains->dc_state, val & mask);

	val &= ~mask;
	val |= state;

	gen9_write_dc_state(dev_priv, val);

	power_domains->dc_state = val & mask;
}

static void tgl_enable_dc3co(struct drm_i915_private *dev_priv)
{
	drm_dbg_kms(&dev_priv->drm, "Enabling DC3CO\n");
	gen9_set_dc_state(dev_priv, DC_STATE_EN_DC3CO);
}

static void tgl_disable_dc3co(struct drm_i915_private *dev_priv)
{
	drm_dbg_kms(&dev_priv->drm, "Disabling DC3CO\n");
	intel_de_rmw(dev_priv, DC_STATE_EN, DC_STATE_DC3CO_STATUS, 0);
	gen9_set_dc_state(dev_priv, DC_STATE_DISABLE);
	 
	usleep_range(200, 210);
}

static void assert_can_enable_dc5(struct drm_i915_private *dev_priv)
{
	enum i915_power_well_id high_pg;

	 
	if (DISPLAY_VER(dev_priv) == 12)
		high_pg = ICL_DISP_PW_3;
	else
		high_pg = SKL_DISP_PW_2;

	drm_WARN_ONCE(&dev_priv->drm,
		      intel_display_power_well_is_enabled(dev_priv, high_pg),
		      "Power wells above platform's DC5 limit still enabled.\n");

	drm_WARN_ONCE(&dev_priv->drm,
		      (intel_de_read(dev_priv, DC_STATE_EN) &
		       DC_STATE_EN_UPTO_DC5),
		      "DC5 already programmed to be enabled.\n");
	assert_rpm_wakelock_held(&dev_priv->runtime_pm);

	assert_dmc_loaded(dev_priv);
}

void gen9_enable_dc5(struct drm_i915_private *dev_priv)
{
	assert_can_enable_dc5(dev_priv);

	drm_dbg_kms(&dev_priv->drm, "Enabling DC5\n");

	 
	if (DISPLAY_VER(dev_priv) == 9 && !IS_BROXTON(dev_priv))
		intel_de_rmw(dev_priv, GEN8_CHICKEN_DCPR_1,
			     0, SKL_SELECT_ALTERNATE_DC_EXIT);

	gen9_set_dc_state(dev_priv, DC_STATE_EN_UPTO_DC5);
}

static void assert_can_enable_dc6(struct drm_i915_private *dev_priv)
{
	drm_WARN_ONCE(&dev_priv->drm,
		      (intel_de_read(dev_priv, UTIL_PIN_CTL) &
		       (UTIL_PIN_ENABLE | UTIL_PIN_MODE_MASK)) ==
		      (UTIL_PIN_ENABLE | UTIL_PIN_MODE_PWM),
		      "Utility pin enabled in PWM mode\n");
	drm_WARN_ONCE(&dev_priv->drm,
		      (intel_de_read(dev_priv, DC_STATE_EN) &
		       DC_STATE_EN_UPTO_DC6),
		      "DC6 already programmed to be enabled.\n");

	assert_dmc_loaded(dev_priv);
}

void skl_enable_dc6(struct drm_i915_private *dev_priv)
{
	assert_can_enable_dc6(dev_priv);

	drm_dbg_kms(&dev_priv->drm, "Enabling DC6\n");

	 
	if (DISPLAY_VER(dev_priv) == 9 && !IS_BROXTON(dev_priv))
		intel_de_rmw(dev_priv, GEN8_CHICKEN_DCPR_1,
			     0, SKL_SELECT_ALTERNATE_DC_EXIT);

	gen9_set_dc_state(dev_priv, DC_STATE_EN_UPTO_DC6);
}

void bxt_enable_dc9(struct drm_i915_private *dev_priv)
{
	assert_can_enable_dc9(dev_priv);

	drm_dbg_kms(&dev_priv->drm, "Enabling DC9\n");
	 
	if (!HAS_PCH_SPLIT(dev_priv))
		intel_pps_reset_all(dev_priv);
	gen9_set_dc_state(dev_priv, DC_STATE_EN_DC9);
}

void bxt_disable_dc9(struct drm_i915_private *dev_priv)
{
	assert_can_disable_dc9(dev_priv);

	drm_dbg_kms(&dev_priv->drm, "Disabling DC9\n");

	gen9_set_dc_state(dev_priv, DC_STATE_DISABLE);

	intel_pps_unlock_regs_wa(dev_priv);
}

static void hsw_power_well_sync_hw(struct drm_i915_private *dev_priv,
				   struct i915_power_well *power_well)
{
	const struct i915_power_well_regs *regs = power_well->desc->ops->regs;
	int pw_idx = i915_power_well_instance(power_well)->hsw.idx;
	u32 mask = HSW_PWR_WELL_CTL_REQ(pw_idx);
	u32 bios_req = intel_de_read(dev_priv, regs->bios);

	 
	if (bios_req & mask) {
		u32 drv_req = intel_de_read(dev_priv, regs->driver);

		if (!(drv_req & mask))
			intel_de_write(dev_priv, regs->driver, drv_req | mask);
		intel_de_write(dev_priv, regs->bios, bios_req & ~mask);
	}
}

static void bxt_dpio_cmn_power_well_enable(struct drm_i915_private *dev_priv,
					   struct i915_power_well *power_well)
{
	bxt_ddi_phy_init(dev_priv, i915_power_well_instance(power_well)->bxt.phy);
}

static void bxt_dpio_cmn_power_well_disable(struct drm_i915_private *dev_priv,
					    struct i915_power_well *power_well)
{
	bxt_ddi_phy_uninit(dev_priv, i915_power_well_instance(power_well)->bxt.phy);
}

static bool bxt_dpio_cmn_power_well_enabled(struct drm_i915_private *dev_priv,
					    struct i915_power_well *power_well)
{
	return bxt_ddi_phy_is_enabled(dev_priv, i915_power_well_instance(power_well)->bxt.phy);
}

static void bxt_verify_ddi_phy_power_wells(struct drm_i915_private *dev_priv)
{
	struct i915_power_well *power_well;

	power_well = lookup_power_well(dev_priv, BXT_DISP_PW_DPIO_CMN_A);
	if (intel_power_well_refcount(power_well) > 0)
		bxt_ddi_phy_verify_state(dev_priv, i915_power_well_instance(power_well)->bxt.phy);

	power_well = lookup_power_well(dev_priv, VLV_DISP_PW_DPIO_CMN_BC);
	if (intel_power_well_refcount(power_well) > 0)
		bxt_ddi_phy_verify_state(dev_priv, i915_power_well_instance(power_well)->bxt.phy);

	if (IS_GEMINILAKE(dev_priv)) {
		power_well = lookup_power_well(dev_priv,
					       GLK_DISP_PW_DPIO_CMN_C);
		if (intel_power_well_refcount(power_well) > 0)
			bxt_ddi_phy_verify_state(dev_priv,
						 i915_power_well_instance(power_well)->bxt.phy);
	}
}

static bool gen9_dc_off_power_well_enabled(struct drm_i915_private *dev_priv,
					   struct i915_power_well *power_well)
{
	return ((intel_de_read(dev_priv, DC_STATE_EN) & DC_STATE_EN_DC3CO) == 0 &&
		(intel_de_read(dev_priv, DC_STATE_EN) & DC_STATE_EN_UPTO_DC5_DC6_MASK) == 0);
}

static void gen9_assert_dbuf_enabled(struct drm_i915_private *dev_priv)
{
	u8 hw_enabled_dbuf_slices = intel_enabled_dbuf_slices_mask(dev_priv);
	u8 enabled_dbuf_slices = dev_priv->display.dbuf.enabled_slices;

	drm_WARN(&dev_priv->drm,
		 hw_enabled_dbuf_slices != enabled_dbuf_slices,
		 "Unexpected DBuf power power state (0x%08x, expected 0x%08x)\n",
		 hw_enabled_dbuf_slices,
		 enabled_dbuf_slices);
}

void gen9_disable_dc_states(struct drm_i915_private *dev_priv)
{
	struct i915_power_domains *power_domains = &dev_priv->display.power.domains;
	struct intel_cdclk_config cdclk_config = {};

	if (power_domains->target_dc_state == DC_STATE_EN_DC3CO) {
		tgl_disable_dc3co(dev_priv);
		return;
	}

	gen9_set_dc_state(dev_priv, DC_STATE_DISABLE);

	if (!HAS_DISPLAY(dev_priv))
		return;

	intel_cdclk_get_cdclk(dev_priv, &cdclk_config);
	 
	drm_WARN_ON(&dev_priv->drm,
		    intel_cdclk_needs_modeset(&dev_priv->display.cdclk.hw,
					      &cdclk_config));

	gen9_assert_dbuf_enabled(dev_priv);

	if (IS_GEMINILAKE(dev_priv) || IS_BROXTON(dev_priv))
		bxt_verify_ddi_phy_power_wells(dev_priv);

	if (DISPLAY_VER(dev_priv) >= 11)
		 
		intel_combo_phy_init(dev_priv);
}

static void gen9_dc_off_power_well_enable(struct drm_i915_private *dev_priv,
					  struct i915_power_well *power_well)
{
	gen9_disable_dc_states(dev_priv);
}

static void gen9_dc_off_power_well_disable(struct drm_i915_private *dev_priv,
					   struct i915_power_well *power_well)
{
	struct i915_power_domains *power_domains = &dev_priv->display.power.domains;

	if (!intel_dmc_has_payload(dev_priv))
		return;

	switch (power_domains->target_dc_state) {
	case DC_STATE_EN_DC3CO:
		tgl_enable_dc3co(dev_priv);
		break;
	case DC_STATE_EN_UPTO_DC6:
		skl_enable_dc6(dev_priv);
		break;
	case DC_STATE_EN_UPTO_DC5:
		gen9_enable_dc5(dev_priv);
		break;
	}
}

static void i9xx_power_well_sync_hw_noop(struct drm_i915_private *dev_priv,
					 struct i915_power_well *power_well)
{
}

static void i9xx_always_on_power_well_noop(struct drm_i915_private *dev_priv,
					   struct i915_power_well *power_well)
{
}

static bool i9xx_always_on_power_well_enabled(struct drm_i915_private *dev_priv,
					     struct i915_power_well *power_well)
{
	return true;
}

static void i830_pipes_power_well_enable(struct drm_i915_private *dev_priv,
					 struct i915_power_well *power_well)
{
	if ((intel_de_read(dev_priv, TRANSCONF(PIPE_A)) & TRANSCONF_ENABLE) == 0)
		i830_enable_pipe(dev_priv, PIPE_A);
	if ((intel_de_read(dev_priv, TRANSCONF(PIPE_B)) & TRANSCONF_ENABLE) == 0)
		i830_enable_pipe(dev_priv, PIPE_B);
}

static void i830_pipes_power_well_disable(struct drm_i915_private *dev_priv,
					  struct i915_power_well *power_well)
{
	i830_disable_pipe(dev_priv, PIPE_B);
	i830_disable_pipe(dev_priv, PIPE_A);
}

static bool i830_pipes_power_well_enabled(struct drm_i915_private *dev_priv,
					  struct i915_power_well *power_well)
{
	return intel_de_read(dev_priv, TRANSCONF(PIPE_A)) & TRANSCONF_ENABLE &&
		intel_de_read(dev_priv, TRANSCONF(PIPE_B)) & TRANSCONF_ENABLE;
}

static void i830_pipes_power_well_sync_hw(struct drm_i915_private *dev_priv,
					  struct i915_power_well *power_well)
{
	if (intel_power_well_refcount(power_well) > 0)
		i830_pipes_power_well_enable(dev_priv, power_well);
	else
		i830_pipes_power_well_disable(dev_priv, power_well);
}

static void vlv_set_power_well(struct drm_i915_private *dev_priv,
			       struct i915_power_well *power_well, bool enable)
{
	int pw_idx = i915_power_well_instance(power_well)->vlv.idx;
	u32 mask;
	u32 state;
	u32 ctrl;

	mask = PUNIT_PWRGT_MASK(pw_idx);
	state = enable ? PUNIT_PWRGT_PWR_ON(pw_idx) :
			 PUNIT_PWRGT_PWR_GATE(pw_idx);

	vlv_punit_get(dev_priv);

#define COND \
	((vlv_punit_read(dev_priv, PUNIT_REG_PWRGT_STATUS) & mask) == state)

	if (COND)
		goto out;

	ctrl = vlv_punit_read(dev_priv, PUNIT_REG_PWRGT_CTRL);
	ctrl &= ~mask;
	ctrl |= state;
	vlv_punit_write(dev_priv, PUNIT_REG_PWRGT_CTRL, ctrl);

	if (wait_for(COND, 100))
		drm_err(&dev_priv->drm,
			"timeout setting power well state %08x (%08x)\n",
			state,
			vlv_punit_read(dev_priv, PUNIT_REG_PWRGT_CTRL));

#undef COND

out:
	vlv_punit_put(dev_priv);
}

static void vlv_power_well_enable(struct drm_i915_private *dev_priv,
				  struct i915_power_well *power_well)
{
	vlv_set_power_well(dev_priv, power_well, true);
}

static void vlv_power_well_disable(struct drm_i915_private *dev_priv,
				   struct i915_power_well *power_well)
{
	vlv_set_power_well(dev_priv, power_well, false);
}

static bool vlv_power_well_enabled(struct drm_i915_private *dev_priv,
				   struct i915_power_well *power_well)
{
	int pw_idx = i915_power_well_instance(power_well)->vlv.idx;
	bool enabled = false;
	u32 mask;
	u32 state;
	u32 ctrl;

	mask = PUNIT_PWRGT_MASK(pw_idx);
	ctrl = PUNIT_PWRGT_PWR_ON(pw_idx);

	vlv_punit_get(dev_priv);

	state = vlv_punit_read(dev_priv, PUNIT_REG_PWRGT_STATUS) & mask;
	 
	drm_WARN_ON(&dev_priv->drm, state != PUNIT_PWRGT_PWR_ON(pw_idx) &&
		    state != PUNIT_PWRGT_PWR_GATE(pw_idx));
	if (state == ctrl)
		enabled = true;

	 
	ctrl = vlv_punit_read(dev_priv, PUNIT_REG_PWRGT_CTRL) & mask;
	drm_WARN_ON(&dev_priv->drm, ctrl != state);

	vlv_punit_put(dev_priv);

	return enabled;
}

static void vlv_init_display_clock_gating(struct drm_i915_private *dev_priv)
{
	 
	intel_de_rmw(dev_priv, DSPCLK_GATE_D(dev_priv),
		     ~DPOUNIT_CLOCK_GATE_DISABLE, VRHUNIT_CLOCK_GATE_DISABLE);

	 
	intel_de_write(dev_priv, MI_ARB_VLV,
		       MI_ARB_DISPLAY_TRICKLE_FEED_DISABLE);
	intel_de_write(dev_priv, CBR1_VLV, 0);

	drm_WARN_ON(&dev_priv->drm, RUNTIME_INFO(dev_priv)->rawclk_freq == 0);
	intel_de_write(dev_priv, RAWCLK_FREQ_VLV,
		       DIV_ROUND_CLOSEST(RUNTIME_INFO(dev_priv)->rawclk_freq,
					 1000));
}

static void vlv_display_power_well_init(struct drm_i915_private *dev_priv)
{
	struct intel_encoder *encoder;
	enum pipe pipe;

	 
	for_each_pipe(dev_priv, pipe) {
		u32 val = intel_de_read(dev_priv, DPLL(pipe));

		val |= DPLL_REF_CLK_ENABLE_VLV | DPLL_VGA_MODE_DIS;
		if (pipe != PIPE_A)
			val |= DPLL_INTEGRATED_CRI_CLK_VLV;

		intel_de_write(dev_priv, DPLL(pipe), val);
	}

	vlv_init_display_clock_gating(dev_priv);

	spin_lock_irq(&dev_priv->irq_lock);
	valleyview_enable_display_irqs(dev_priv);
	spin_unlock_irq(&dev_priv->irq_lock);

	 
	if (dev_priv->display.power.domains.initializing)
		return;

	intel_hpd_init(dev_priv);
	intel_hpd_poll_disable(dev_priv);

	 
	for_each_intel_encoder(&dev_priv->drm, encoder) {
		if (encoder->type == INTEL_OUTPUT_ANALOG)
			intel_crt_reset(&encoder->base);
	}

	intel_vga_redisable_power_on(dev_priv);

	intel_pps_unlock_regs_wa(dev_priv);
}

static void vlv_display_power_well_deinit(struct drm_i915_private *dev_priv)
{
	spin_lock_irq(&dev_priv->irq_lock);
	valleyview_disable_display_irqs(dev_priv);
	spin_unlock_irq(&dev_priv->irq_lock);

	 
	intel_synchronize_irq(dev_priv);

	intel_pps_reset_all(dev_priv);

	 
	if (!dev_priv->drm.dev->power.is_suspended)
		intel_hpd_poll_enable(dev_priv);
}

static void vlv_display_power_well_enable(struct drm_i915_private *dev_priv,
					  struct i915_power_well *power_well)
{
	vlv_set_power_well(dev_priv, power_well, true);

	vlv_display_power_well_init(dev_priv);
}

static void vlv_display_power_well_disable(struct drm_i915_private *dev_priv,
					   struct i915_power_well *power_well)
{
	vlv_display_power_well_deinit(dev_priv);

	vlv_set_power_well(dev_priv, power_well, false);
}

static void vlv_dpio_cmn_power_well_enable(struct drm_i915_private *dev_priv,
					   struct i915_power_well *power_well)
{
	 
	udelay(1);  

	vlv_set_power_well(dev_priv, power_well, true);

	 
	intel_de_rmw(dev_priv, DPIO_CTL, 0, DPIO_CMNRST);
}

static void vlv_dpio_cmn_power_well_disable(struct drm_i915_private *dev_priv,
					    struct i915_power_well *power_well)
{
	enum pipe pipe;

	for_each_pipe(dev_priv, pipe)
		assert_pll_disabled(dev_priv, pipe);

	 
	intel_de_rmw(dev_priv, DPIO_CTL, DPIO_CMNRST, 0);

	vlv_set_power_well(dev_priv, power_well, false);
}

#define BITS_SET(val, bits) (((val) & (bits)) == (bits))

static void assert_chv_phy_status(struct drm_i915_private *dev_priv)
{
	struct i915_power_well *cmn_bc =
		lookup_power_well(dev_priv, VLV_DISP_PW_DPIO_CMN_BC);
	struct i915_power_well *cmn_d =
		lookup_power_well(dev_priv, CHV_DISP_PW_DPIO_CMN_D);
	u32 phy_control = dev_priv->display.power.chv_phy_control;
	u32 phy_status = 0;
	u32 phy_status_mask = 0xffffffff;

	 
	if (!dev_priv->display.power.chv_phy_assert[DPIO_PHY0])
		phy_status_mask &= ~(PHY_STATUS_CMN_LDO(DPIO_PHY0, DPIO_CH0) |
				     PHY_STATUS_SPLINE_LDO(DPIO_PHY0, DPIO_CH0, 0) |
				     PHY_STATUS_SPLINE_LDO(DPIO_PHY0, DPIO_CH0, 1) |
				     PHY_STATUS_CMN_LDO(DPIO_PHY0, DPIO_CH1) |
				     PHY_STATUS_SPLINE_LDO(DPIO_PHY0, DPIO_CH1, 0) |
				     PHY_STATUS_SPLINE_LDO(DPIO_PHY0, DPIO_CH1, 1));

	if (!dev_priv->display.power.chv_phy_assert[DPIO_PHY1])
		phy_status_mask &= ~(PHY_STATUS_CMN_LDO(DPIO_PHY1, DPIO_CH0) |
				     PHY_STATUS_SPLINE_LDO(DPIO_PHY1, DPIO_CH0, 0) |
				     PHY_STATUS_SPLINE_LDO(DPIO_PHY1, DPIO_CH0, 1));

	if (intel_power_well_is_enabled(dev_priv, cmn_bc)) {
		phy_status |= PHY_POWERGOOD(DPIO_PHY0);

		 
		if ((phy_control & PHY_CH_POWER_DOWN_OVRD_EN(DPIO_PHY0, DPIO_CH0)) == 0)
			phy_control |= PHY_CH_POWER_DOWN_OVRD(0xf, DPIO_PHY0, DPIO_CH0);

		if ((phy_control & PHY_CH_POWER_DOWN_OVRD_EN(DPIO_PHY0, DPIO_CH1)) == 0)
			phy_control |= PHY_CH_POWER_DOWN_OVRD(0xf, DPIO_PHY0, DPIO_CH1);

		 
		if (BITS_SET(phy_control,
			     PHY_CH_POWER_DOWN_OVRD(0xf, DPIO_PHY0, DPIO_CH0) |
			     PHY_CH_POWER_DOWN_OVRD(0xf, DPIO_PHY0, DPIO_CH1)))
			phy_status |= PHY_STATUS_CMN_LDO(DPIO_PHY0, DPIO_CH0);

		 
		if (BITS_SET(phy_control,
			     PHY_CH_POWER_DOWN_OVRD(0xf, DPIO_PHY0, DPIO_CH1)) &&
		    (intel_de_read(dev_priv, DPLL(PIPE_B)) & DPLL_VCO_ENABLE) == 0)
			phy_status |= PHY_STATUS_CMN_LDO(DPIO_PHY0, DPIO_CH1);

		if (BITS_SET(phy_control,
			     PHY_CH_POWER_DOWN_OVRD(0x3, DPIO_PHY0, DPIO_CH0)))
			phy_status |= PHY_STATUS_SPLINE_LDO(DPIO_PHY0, DPIO_CH0, 0);
		if (BITS_SET(phy_control,
			     PHY_CH_POWER_DOWN_OVRD(0xc, DPIO_PHY0, DPIO_CH0)))
			phy_status |= PHY_STATUS_SPLINE_LDO(DPIO_PHY0, DPIO_CH0, 1);

		if (BITS_SET(phy_control,
			     PHY_CH_POWER_DOWN_OVRD(0x3, DPIO_PHY0, DPIO_CH1)))
			phy_status |= PHY_STATUS_SPLINE_LDO(DPIO_PHY0, DPIO_CH1, 0);
		if (BITS_SET(phy_control,
			     PHY_CH_POWER_DOWN_OVRD(0xc, DPIO_PHY0, DPIO_CH1)))
			phy_status |= PHY_STATUS_SPLINE_LDO(DPIO_PHY0, DPIO_CH1, 1);
	}

	if (intel_power_well_is_enabled(dev_priv, cmn_d)) {
		phy_status |= PHY_POWERGOOD(DPIO_PHY1);

		 
		if ((phy_control & PHY_CH_POWER_DOWN_OVRD_EN(DPIO_PHY1, DPIO_CH0)) == 0)
			phy_control |= PHY_CH_POWER_DOWN_OVRD(0xf, DPIO_PHY1, DPIO_CH0);

		if (BITS_SET(phy_control,
			     PHY_CH_POWER_DOWN_OVRD(0xf, DPIO_PHY1, DPIO_CH0)))
			phy_status |= PHY_STATUS_CMN_LDO(DPIO_PHY1, DPIO_CH0);

		if (BITS_SET(phy_control,
			     PHY_CH_POWER_DOWN_OVRD(0x3, DPIO_PHY1, DPIO_CH0)))
			phy_status |= PHY_STATUS_SPLINE_LDO(DPIO_PHY1, DPIO_CH0, 0);
		if (BITS_SET(phy_control,
			     PHY_CH_POWER_DOWN_OVRD(0xc, DPIO_PHY1, DPIO_CH0)))
			phy_status |= PHY_STATUS_SPLINE_LDO(DPIO_PHY1, DPIO_CH0, 1);
	}

	phy_status &= phy_status_mask;

	 
	if (intel_de_wait_for_register(dev_priv, DISPLAY_PHY_STATUS,
				       phy_status_mask, phy_status, 10))
		drm_err(&dev_priv->drm,
			"Unexpected PHY_STATUS 0x%08x, expected 0x%08x (PHY_CONTROL=0x%08x)\n",
			intel_de_read(dev_priv, DISPLAY_PHY_STATUS) & phy_status_mask,
			phy_status, dev_priv->display.power.chv_phy_control);
}

#undef BITS_SET

static void chv_dpio_cmn_power_well_enable(struct drm_i915_private *dev_priv,
					   struct i915_power_well *power_well)
{
	enum i915_power_well_id id = i915_power_well_instance(power_well)->id;
	enum dpio_phy phy;
	enum pipe pipe;
	u32 tmp;

	drm_WARN_ON_ONCE(&dev_priv->drm,
			 id != VLV_DISP_PW_DPIO_CMN_BC &&
			 id != CHV_DISP_PW_DPIO_CMN_D);

	if (id == VLV_DISP_PW_DPIO_CMN_BC) {
		pipe = PIPE_A;
		phy = DPIO_PHY0;
	} else {
		pipe = PIPE_C;
		phy = DPIO_PHY1;
	}

	 
	udelay(1);  
	vlv_set_power_well(dev_priv, power_well, true);

	 
	if (intel_de_wait_for_set(dev_priv, DISPLAY_PHY_STATUS,
				  PHY_POWERGOOD(phy), 1))
		drm_err(&dev_priv->drm, "Display PHY %d is not power up\n",
			phy);

	vlv_dpio_get(dev_priv);

	 
	tmp = vlv_dpio_read(dev_priv, pipe, CHV_CMN_DW28);
	tmp |= DPIO_DYNPWRDOWNEN_CH0 | DPIO_CL1POWERDOWNEN |
		DPIO_SUS_CLK_CONFIG_GATE_CLKREQ;
	vlv_dpio_write(dev_priv, pipe, CHV_CMN_DW28, tmp);

	if (id == VLV_DISP_PW_DPIO_CMN_BC) {
		tmp = vlv_dpio_read(dev_priv, pipe, _CHV_CMN_DW6_CH1);
		tmp |= DPIO_DYNPWRDOWNEN_CH1;
		vlv_dpio_write(dev_priv, pipe, _CHV_CMN_DW6_CH1, tmp);
	} else {
		 
		tmp = vlv_dpio_read(dev_priv, pipe, CHV_CMN_DW30);
		tmp |= DPIO_CL2_LDOFUSE_PWRENB;
		vlv_dpio_write(dev_priv, pipe, CHV_CMN_DW30, tmp);
	}

	vlv_dpio_put(dev_priv);

	dev_priv->display.power.chv_phy_control |= PHY_COM_LANE_RESET_DEASSERT(phy);
	intel_de_write(dev_priv, DISPLAY_PHY_CONTROL,
		       dev_priv->display.power.chv_phy_control);

	drm_dbg_kms(&dev_priv->drm,
		    "Enabled DPIO PHY%d (PHY_CONTROL=0x%08x)\n",
		    phy, dev_priv->display.power.chv_phy_control);

	assert_chv_phy_status(dev_priv);
}

static void chv_dpio_cmn_power_well_disable(struct drm_i915_private *dev_priv,
					    struct i915_power_well *power_well)
{
	enum i915_power_well_id id = i915_power_well_instance(power_well)->id;
	enum dpio_phy phy;

	drm_WARN_ON_ONCE(&dev_priv->drm,
			 id != VLV_DISP_PW_DPIO_CMN_BC &&
			 id != CHV_DISP_PW_DPIO_CMN_D);

	if (id == VLV_DISP_PW_DPIO_CMN_BC) {
		phy = DPIO_PHY0;
		assert_pll_disabled(dev_priv, PIPE_A);
		assert_pll_disabled(dev_priv, PIPE_B);
	} else {
		phy = DPIO_PHY1;
		assert_pll_disabled(dev_priv, PIPE_C);
	}

	dev_priv->display.power.chv_phy_control &= ~PHY_COM_LANE_RESET_DEASSERT(phy);
	intel_de_write(dev_priv, DISPLAY_PHY_CONTROL,
		       dev_priv->display.power.chv_phy_control);

	vlv_set_power_well(dev_priv, power_well, false);

	drm_dbg_kms(&dev_priv->drm,
		    "Disabled DPIO PHY%d (PHY_CONTROL=0x%08x)\n",
		    phy, dev_priv->display.power.chv_phy_control);

	 
	dev_priv->display.power.chv_phy_assert[phy] = true;

	assert_chv_phy_status(dev_priv);
}

static void assert_chv_phy_powergate(struct drm_i915_private *dev_priv, enum dpio_phy phy,
				     enum dpio_channel ch, bool override, unsigned int mask)
{
	enum pipe pipe = phy == DPIO_PHY0 ? PIPE_A : PIPE_C;
	u32 reg, val, expected, actual;

	 
	if (!dev_priv->display.power.chv_phy_assert[phy])
		return;

	if (ch == DPIO_CH0)
		reg = _CHV_CMN_DW0_CH0;
	else
		reg = _CHV_CMN_DW6_CH1;

	vlv_dpio_get(dev_priv);
	val = vlv_dpio_read(dev_priv, pipe, reg);
	vlv_dpio_put(dev_priv);

	 
	if (!override || mask == 0xf) {
		expected = DPIO_ALLDL_POWERDOWN | DPIO_ANYDL_POWERDOWN;
		 
		if (ch == DPIO_CH1 && val == 0)
			expected = 0;
	} else if (mask != 0x0) {
		expected = DPIO_ANYDL_POWERDOWN;
	} else {
		expected = 0;
	}

	if (ch == DPIO_CH0)
		actual = val >> DPIO_ANYDL_POWERDOWN_SHIFT_CH0;
	else
		actual = val >> DPIO_ANYDL_POWERDOWN_SHIFT_CH1;
	actual &= DPIO_ALLDL_POWERDOWN | DPIO_ANYDL_POWERDOWN;

	drm_WARN(&dev_priv->drm, actual != expected,
		 "Unexpected DPIO lane power down: all %d, any %d. Expected: all %d, any %d. (0x%x = 0x%08x)\n",
		 !!(actual & DPIO_ALLDL_POWERDOWN),
		 !!(actual & DPIO_ANYDL_POWERDOWN),
		 !!(expected & DPIO_ALLDL_POWERDOWN),
		 !!(expected & DPIO_ANYDL_POWERDOWN),
		 reg, val);
}

bool chv_phy_powergate_ch(struct drm_i915_private *dev_priv, enum dpio_phy phy,
			  enum dpio_channel ch, bool override)
{
	struct i915_power_domains *power_domains = &dev_priv->display.power.domains;
	bool was_override;

	mutex_lock(&power_domains->lock);

	was_override = dev_priv->display.power.chv_phy_control & PHY_CH_POWER_DOWN_OVRD_EN(phy, ch);

	if (override == was_override)
		goto out;

	if (override)
		dev_priv->display.power.chv_phy_control |= PHY_CH_POWER_DOWN_OVRD_EN(phy, ch);
	else
		dev_priv->display.power.chv_phy_control &= ~PHY_CH_POWER_DOWN_OVRD_EN(phy, ch);

	intel_de_write(dev_priv, DISPLAY_PHY_CONTROL,
		       dev_priv->display.power.chv_phy_control);

	drm_dbg_kms(&dev_priv->drm,
		    "Power gating DPIO PHY%d CH%d (DPIO_PHY_CONTROL=0x%08x)\n",
		    phy, ch, dev_priv->display.power.chv_phy_control);

	assert_chv_phy_status(dev_priv);

out:
	mutex_unlock(&power_domains->lock);

	return was_override;
}

void chv_phy_powergate_lanes(struct intel_encoder *encoder,
			     bool override, unsigned int mask)
{
	struct drm_i915_private *dev_priv = to_i915(encoder->base.dev);
	struct i915_power_domains *power_domains = &dev_priv->display.power.domains;
	enum dpio_phy phy = vlv_dig_port_to_phy(enc_to_dig_port(encoder));
	enum dpio_channel ch = vlv_dig_port_to_channel(enc_to_dig_port(encoder));

	mutex_lock(&power_domains->lock);

	dev_priv->display.power.chv_phy_control &= ~PHY_CH_POWER_DOWN_OVRD(0xf, phy, ch);
	dev_priv->display.power.chv_phy_control |= PHY_CH_POWER_DOWN_OVRD(mask, phy, ch);

	if (override)
		dev_priv->display.power.chv_phy_control |= PHY_CH_POWER_DOWN_OVRD_EN(phy, ch);
	else
		dev_priv->display.power.chv_phy_control &= ~PHY_CH_POWER_DOWN_OVRD_EN(phy, ch);

	intel_de_write(dev_priv, DISPLAY_PHY_CONTROL,
		       dev_priv->display.power.chv_phy_control);

	drm_dbg_kms(&dev_priv->drm,
		    "Power gating DPIO PHY%d CH%d lanes 0x%x (PHY_CONTROL=0x%08x)\n",
		    phy, ch, mask, dev_priv->display.power.chv_phy_control);

	assert_chv_phy_status(dev_priv);

	assert_chv_phy_powergate(dev_priv, phy, ch, override, mask);

	mutex_unlock(&power_domains->lock);
}

static bool chv_pipe_power_well_enabled(struct drm_i915_private *dev_priv,
					struct i915_power_well *power_well)
{
	enum pipe pipe = PIPE_A;
	bool enabled;
	u32 state, ctrl;

	vlv_punit_get(dev_priv);

	state = vlv_punit_read(dev_priv, PUNIT_REG_DSPSSPM) & DP_SSS_MASK(pipe);
	 
	drm_WARN_ON(&dev_priv->drm, state != DP_SSS_PWR_ON(pipe) &&
		    state != DP_SSS_PWR_GATE(pipe));
	enabled = state == DP_SSS_PWR_ON(pipe);

	 
	ctrl = vlv_punit_read(dev_priv, PUNIT_REG_DSPSSPM) & DP_SSC_MASK(pipe);
	drm_WARN_ON(&dev_priv->drm, ctrl << 16 != state);

	vlv_punit_put(dev_priv);

	return enabled;
}

static void chv_set_pipe_power_well(struct drm_i915_private *dev_priv,
				    struct i915_power_well *power_well,
				    bool enable)
{
	enum pipe pipe = PIPE_A;
	u32 state;
	u32 ctrl;

	state = enable ? DP_SSS_PWR_ON(pipe) : DP_SSS_PWR_GATE(pipe);

	vlv_punit_get(dev_priv);

#define COND \
	((vlv_punit_read(dev_priv, PUNIT_REG_DSPSSPM) & DP_SSS_MASK(pipe)) == state)

	if (COND)
		goto out;

	ctrl = vlv_punit_read(dev_priv, PUNIT_REG_DSPSSPM);
	ctrl &= ~DP_SSC_MASK(pipe);
	ctrl |= enable ? DP_SSC_PWR_ON(pipe) : DP_SSC_PWR_GATE(pipe);
	vlv_punit_write(dev_priv, PUNIT_REG_DSPSSPM, ctrl);

	if (wait_for(COND, 100))
		drm_err(&dev_priv->drm,
			"timeout setting power well state %08x (%08x)\n",
			state,
			vlv_punit_read(dev_priv, PUNIT_REG_DSPSSPM));

#undef COND

out:
	vlv_punit_put(dev_priv);
}

static void chv_pipe_power_well_sync_hw(struct drm_i915_private *dev_priv,
					struct i915_power_well *power_well)
{
	intel_de_write(dev_priv, DISPLAY_PHY_CONTROL,
		       dev_priv->display.power.chv_phy_control);
}

static void chv_pipe_power_well_enable(struct drm_i915_private *dev_priv,
				       struct i915_power_well *power_well)
{
	chv_set_pipe_power_well(dev_priv, power_well, true);

	vlv_display_power_well_init(dev_priv);
}

static void chv_pipe_power_well_disable(struct drm_i915_private *dev_priv,
					struct i915_power_well *power_well)
{
	vlv_display_power_well_deinit(dev_priv);

	chv_set_pipe_power_well(dev_priv, power_well, false);
}

static void
tgl_tc_cold_request(struct drm_i915_private *i915, bool block)
{
	u8 tries = 0;
	int ret;

	while (1) {
		u32 low_val;
		u32 high_val = 0;

		if (block)
			low_val = TGL_PCODE_EXIT_TCCOLD_DATA_L_BLOCK_REQ;
		else
			low_val = TGL_PCODE_EXIT_TCCOLD_DATA_L_UNBLOCK_REQ;

		 
		ret = snb_pcode_read(&i915->uncore, TGL_PCODE_TCCOLD, &low_val, &high_val);
		if (ret == 0) {
			if (block &&
			    (low_val & TGL_PCODE_EXIT_TCCOLD_DATA_L_EXIT_FAILED))
				ret = -EIO;
			else
				break;
		}

		if (++tries == 3)
			break;

		msleep(1);
	}

	if (ret)
		drm_err(&i915->drm, "TC cold %sblock failed\n",
			block ? "" : "un");
	else
		drm_dbg_kms(&i915->drm, "TC cold %sblock succeeded\n",
			    block ? "" : "un");
}

static void
tgl_tc_cold_off_power_well_enable(struct drm_i915_private *i915,
				  struct i915_power_well *power_well)
{
	tgl_tc_cold_request(i915, true);
}

static void
tgl_tc_cold_off_power_well_disable(struct drm_i915_private *i915,
				   struct i915_power_well *power_well)
{
	tgl_tc_cold_request(i915, false);
}

static void
tgl_tc_cold_off_power_well_sync_hw(struct drm_i915_private *i915,
				   struct i915_power_well *power_well)
{
	if (intel_power_well_refcount(power_well) > 0)
		tgl_tc_cold_off_power_well_enable(i915, power_well);
	else
		tgl_tc_cold_off_power_well_disable(i915, power_well);
}

static bool
tgl_tc_cold_off_power_well_is_enabled(struct drm_i915_private *dev_priv,
				      struct i915_power_well *power_well)
{
	 
	return intel_power_well_refcount(power_well);
}

static void xelpdp_aux_power_well_enable(struct drm_i915_private *dev_priv,
					 struct i915_power_well *power_well)
{
	enum aux_ch aux_ch = i915_power_well_instance(power_well)->xelpdp.aux_ch;

	intel_de_rmw(dev_priv, XELPDP_DP_AUX_CH_CTL(aux_ch),
		     XELPDP_DP_AUX_CH_CTL_POWER_REQUEST,
		     XELPDP_DP_AUX_CH_CTL_POWER_REQUEST);

	 
	usleep_range(600, 1200);
}

static void xelpdp_aux_power_well_disable(struct drm_i915_private *dev_priv,
					  struct i915_power_well *power_well)
{
	enum aux_ch aux_ch = i915_power_well_instance(power_well)->xelpdp.aux_ch;

	intel_de_rmw(dev_priv, XELPDP_DP_AUX_CH_CTL(aux_ch),
		     XELPDP_DP_AUX_CH_CTL_POWER_REQUEST,
		     0);
	usleep_range(10, 30);
}

static bool xelpdp_aux_power_well_enabled(struct drm_i915_private *dev_priv,
					  struct i915_power_well *power_well)
{
	enum aux_ch aux_ch = i915_power_well_instance(power_well)->xelpdp.aux_ch;

	return intel_de_read(dev_priv, XELPDP_DP_AUX_CH_CTL(aux_ch)) &
		XELPDP_DP_AUX_CH_CTL_POWER_STATUS;
}

const struct i915_power_well_ops i9xx_always_on_power_well_ops = {
	.sync_hw = i9xx_power_well_sync_hw_noop,
	.enable = i9xx_always_on_power_well_noop,
	.disable = i9xx_always_on_power_well_noop,
	.is_enabled = i9xx_always_on_power_well_enabled,
};

const struct i915_power_well_ops chv_pipe_power_well_ops = {
	.sync_hw = chv_pipe_power_well_sync_hw,
	.enable = chv_pipe_power_well_enable,
	.disable = chv_pipe_power_well_disable,
	.is_enabled = chv_pipe_power_well_enabled,
};

const struct i915_power_well_ops chv_dpio_cmn_power_well_ops = {
	.sync_hw = i9xx_power_well_sync_hw_noop,
	.enable = chv_dpio_cmn_power_well_enable,
	.disable = chv_dpio_cmn_power_well_disable,
	.is_enabled = vlv_power_well_enabled,
};

const struct i915_power_well_ops i830_pipes_power_well_ops = {
	.sync_hw = i830_pipes_power_well_sync_hw,
	.enable = i830_pipes_power_well_enable,
	.disable = i830_pipes_power_well_disable,
	.is_enabled = i830_pipes_power_well_enabled,
};

static const struct i915_power_well_regs hsw_power_well_regs = {
	.bios	= HSW_PWR_WELL_CTL1,
	.driver	= HSW_PWR_WELL_CTL2,
	.kvmr	= HSW_PWR_WELL_CTL3,
	.debug	= HSW_PWR_WELL_CTL4,
};

const struct i915_power_well_ops hsw_power_well_ops = {
	.regs = &hsw_power_well_regs,
	.sync_hw = hsw_power_well_sync_hw,
	.enable = hsw_power_well_enable,
	.disable = hsw_power_well_disable,
	.is_enabled = hsw_power_well_enabled,
};

const struct i915_power_well_ops gen9_dc_off_power_well_ops = {
	.sync_hw = i9xx_power_well_sync_hw_noop,
	.enable = gen9_dc_off_power_well_enable,
	.disable = gen9_dc_off_power_well_disable,
	.is_enabled = gen9_dc_off_power_well_enabled,
};

const struct i915_power_well_ops bxt_dpio_cmn_power_well_ops = {
	.sync_hw = i9xx_power_well_sync_hw_noop,
	.enable = bxt_dpio_cmn_power_well_enable,
	.disable = bxt_dpio_cmn_power_well_disable,
	.is_enabled = bxt_dpio_cmn_power_well_enabled,
};

const struct i915_power_well_ops vlv_display_power_well_ops = {
	.sync_hw = i9xx_power_well_sync_hw_noop,
	.enable = vlv_display_power_well_enable,
	.disable = vlv_display_power_well_disable,
	.is_enabled = vlv_power_well_enabled,
};

const struct i915_power_well_ops vlv_dpio_cmn_power_well_ops = {
	.sync_hw = i9xx_power_well_sync_hw_noop,
	.enable = vlv_dpio_cmn_power_well_enable,
	.disable = vlv_dpio_cmn_power_well_disable,
	.is_enabled = vlv_power_well_enabled,
};

const struct i915_power_well_ops vlv_dpio_power_well_ops = {
	.sync_hw = i9xx_power_well_sync_hw_noop,
	.enable = vlv_power_well_enable,
	.disable = vlv_power_well_disable,
	.is_enabled = vlv_power_well_enabled,
};

static const struct i915_power_well_regs icl_aux_power_well_regs = {
	.bios	= ICL_PWR_WELL_CTL_AUX1,
	.driver	= ICL_PWR_WELL_CTL_AUX2,
	.debug	= ICL_PWR_WELL_CTL_AUX4,
};

const struct i915_power_well_ops icl_aux_power_well_ops = {
	.regs = &icl_aux_power_well_regs,
	.sync_hw = hsw_power_well_sync_hw,
	.enable = icl_aux_power_well_enable,
	.disable = icl_aux_power_well_disable,
	.is_enabled = hsw_power_well_enabled,
};

static const struct i915_power_well_regs icl_ddi_power_well_regs = {
	.bios	= ICL_PWR_WELL_CTL_DDI1,
	.driver	= ICL_PWR_WELL_CTL_DDI2,
	.debug	= ICL_PWR_WELL_CTL_DDI4,
};

const struct i915_power_well_ops icl_ddi_power_well_ops = {
	.regs = &icl_ddi_power_well_regs,
	.sync_hw = hsw_power_well_sync_hw,
	.enable = hsw_power_well_enable,
	.disable = hsw_power_well_disable,
	.is_enabled = hsw_power_well_enabled,
};

const struct i915_power_well_ops tgl_tc_cold_off_ops = {
	.sync_hw = tgl_tc_cold_off_power_well_sync_hw,
	.enable = tgl_tc_cold_off_power_well_enable,
	.disable = tgl_tc_cold_off_power_well_disable,
	.is_enabled = tgl_tc_cold_off_power_well_is_enabled,
};

const struct i915_power_well_ops xelpdp_aux_power_well_ops = {
	.sync_hw = i9xx_power_well_sync_hw_noop,
	.enable = xelpdp_aux_power_well_enable,
	.disable = xelpdp_aux_power_well_disable,
	.is_enabled = xelpdp_aux_power_well_enabled,
};
