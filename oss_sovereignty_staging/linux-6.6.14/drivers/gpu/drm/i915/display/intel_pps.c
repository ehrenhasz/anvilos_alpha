
 

#include "g4x_dp.h"
#include "i915_drv.h"
#include "i915_reg.h"
#include "intel_de.h"
#include "intel_display_power_well.h"
#include "intel_display_types.h"
#include "intel_dp.h"
#include "intel_dpio_phy.h"
#include "intel_dpll.h"
#include "intel_lvds.h"
#include "intel_lvds_regs.h"
#include "intel_pps.h"
#include "intel_pps_regs.h"
#include "intel_quirks.h"

static void vlv_steal_power_sequencer(struct drm_i915_private *dev_priv,
				      enum pipe pipe);

static void pps_init_delays(struct intel_dp *intel_dp);
static void pps_init_registers(struct intel_dp *intel_dp, bool force_disable_vdd);

static const char *pps_name(struct drm_i915_private *i915,
			    struct intel_pps *pps)
{
	if (IS_VALLEYVIEW(i915) || IS_CHERRYVIEW(i915)) {
		switch (pps->pps_pipe) {
		case INVALID_PIPE:
			 
			return "PPS <none>";
		case PIPE_A:
			return "PPS A";
		case PIPE_B:
			return "PPS B";
		default:
			MISSING_CASE(pps->pps_pipe);
			break;
		}
	} else {
		switch (pps->pps_idx) {
		case 0:
			return "PPS 0";
		case 1:
			return "PPS 1";
		default:
			MISSING_CASE(pps->pps_idx);
			break;
		}
	}

	return "PPS <invalid>";
}

intel_wakeref_t intel_pps_lock(struct intel_dp *intel_dp)
{
	struct drm_i915_private *dev_priv = dp_to_i915(intel_dp);
	intel_wakeref_t wakeref;

	 
	wakeref = intel_display_power_get(dev_priv, POWER_DOMAIN_DISPLAY_CORE);
	mutex_lock(&dev_priv->display.pps.mutex);

	return wakeref;
}

intel_wakeref_t intel_pps_unlock(struct intel_dp *intel_dp,
				 intel_wakeref_t wakeref)
{
	struct drm_i915_private *dev_priv = dp_to_i915(intel_dp);

	mutex_unlock(&dev_priv->display.pps.mutex);
	intel_display_power_put(dev_priv, POWER_DOMAIN_DISPLAY_CORE, wakeref);

	return 0;
}

static void
vlv_power_sequencer_kick(struct intel_dp *intel_dp)
{
	struct drm_i915_private *dev_priv = dp_to_i915(intel_dp);
	struct intel_digital_port *dig_port = dp_to_dig_port(intel_dp);
	enum pipe pipe = intel_dp->pps.pps_pipe;
	bool pll_enabled, release_cl_override = false;
	enum dpio_phy phy = DPIO_PHY(pipe);
	enum dpio_channel ch = vlv_pipe_to_channel(pipe);
	u32 DP;

	if (drm_WARN(&dev_priv->drm,
		     intel_de_read(dev_priv, intel_dp->output_reg) & DP_PORT_EN,
		     "skipping %s kick due to [ENCODER:%d:%s] being active\n",
		     pps_name(dev_priv, &intel_dp->pps),
		     dig_port->base.base.base.id, dig_port->base.base.name))
		return;

	drm_dbg_kms(&dev_priv->drm,
		    "kicking %s for [ENCODER:%d:%s]\n",
		    pps_name(dev_priv, &intel_dp->pps),
		    dig_port->base.base.base.id, dig_port->base.base.name);

	 
	DP = intel_de_read(dev_priv, intel_dp->output_reg) & DP_DETECTED;
	DP |= DP_VOLTAGE_0_4 | DP_PRE_EMPHASIS_0;
	DP |= DP_PORT_WIDTH(1);
	DP |= DP_LINK_TRAIN_PAT_1;

	if (IS_CHERRYVIEW(dev_priv))
		DP |= DP_PIPE_SEL_CHV(pipe);
	else
		DP |= DP_PIPE_SEL(pipe);

	pll_enabled = intel_de_read(dev_priv, DPLL(pipe)) & DPLL_VCO_ENABLE;

	 
	if (!pll_enabled) {
		release_cl_override = IS_CHERRYVIEW(dev_priv) &&
			!chv_phy_powergate_ch(dev_priv, phy, ch, true);

		if (vlv_force_pll_on(dev_priv, pipe, vlv_get_dpll(dev_priv))) {
			drm_err(&dev_priv->drm,
				"Failed to force on PLL for pipe %c!\n",
				pipe_name(pipe));
			return;
		}
	}

	 
	intel_de_write(dev_priv, intel_dp->output_reg, DP);
	intel_de_posting_read(dev_priv, intel_dp->output_reg);

	intel_de_write(dev_priv, intel_dp->output_reg, DP | DP_PORT_EN);
	intel_de_posting_read(dev_priv, intel_dp->output_reg);

	intel_de_write(dev_priv, intel_dp->output_reg, DP & ~DP_PORT_EN);
	intel_de_posting_read(dev_priv, intel_dp->output_reg);

	if (!pll_enabled) {
		vlv_force_pll_off(dev_priv, pipe);

		if (release_cl_override)
			chv_phy_powergate_ch(dev_priv, phy, ch, false);
	}
}

static enum pipe vlv_find_free_pps(struct drm_i915_private *dev_priv)
{
	struct intel_encoder *encoder;
	unsigned int pipes = (1 << PIPE_A) | (1 << PIPE_B);

	 
	for_each_intel_dp(&dev_priv->drm, encoder) {
		struct intel_dp *intel_dp = enc_to_intel_dp(encoder);

		if (encoder->type == INTEL_OUTPUT_EDP) {
			drm_WARN_ON(&dev_priv->drm,
				    intel_dp->pps.active_pipe != INVALID_PIPE &&
				    intel_dp->pps.active_pipe !=
				    intel_dp->pps.pps_pipe);

			if (intel_dp->pps.pps_pipe != INVALID_PIPE)
				pipes &= ~(1 << intel_dp->pps.pps_pipe);
		} else {
			drm_WARN_ON(&dev_priv->drm,
				    intel_dp->pps.pps_pipe != INVALID_PIPE);

			if (intel_dp->pps.active_pipe != INVALID_PIPE)
				pipes &= ~(1 << intel_dp->pps.active_pipe);
		}
	}

	if (pipes == 0)
		return INVALID_PIPE;

	return ffs(pipes) - 1;
}

static enum pipe
vlv_power_sequencer_pipe(struct intel_dp *intel_dp)
{
	struct drm_i915_private *dev_priv = dp_to_i915(intel_dp);
	struct intel_digital_port *dig_port = dp_to_dig_port(intel_dp);
	enum pipe pipe;

	lockdep_assert_held(&dev_priv->display.pps.mutex);

	 
	drm_WARN_ON(&dev_priv->drm, !intel_dp_is_edp(intel_dp));

	drm_WARN_ON(&dev_priv->drm, intel_dp->pps.active_pipe != INVALID_PIPE &&
		    intel_dp->pps.active_pipe != intel_dp->pps.pps_pipe);

	if (intel_dp->pps.pps_pipe != INVALID_PIPE)
		return intel_dp->pps.pps_pipe;

	pipe = vlv_find_free_pps(dev_priv);

	 
	if (drm_WARN_ON(&dev_priv->drm, pipe == INVALID_PIPE))
		pipe = PIPE_A;

	vlv_steal_power_sequencer(dev_priv, pipe);
	intel_dp->pps.pps_pipe = pipe;

	drm_dbg_kms(&dev_priv->drm,
		    "picked %s for [ENCODER:%d:%s]\n",
		    pps_name(dev_priv, &intel_dp->pps),
		    dig_port->base.base.base.id, dig_port->base.base.name);

	 
	pps_init_delays(intel_dp);
	pps_init_registers(intel_dp, true);

	 
	vlv_power_sequencer_kick(intel_dp);

	return intel_dp->pps.pps_pipe;
}

static int
bxt_power_sequencer_idx(struct intel_dp *intel_dp)
{
	struct drm_i915_private *dev_priv = dp_to_i915(intel_dp);
	int pps_idx = intel_dp->pps.pps_idx;

	lockdep_assert_held(&dev_priv->display.pps.mutex);

	 
	drm_WARN_ON(&dev_priv->drm, !intel_dp_is_edp(intel_dp));

	if (!intel_dp->pps.pps_reset)
		return pps_idx;

	intel_dp->pps.pps_reset = false;

	 
	pps_init_registers(intel_dp, false);

	return pps_idx;
}

typedef bool (*pps_check)(struct drm_i915_private *dev_priv, int pps_idx);

static bool pps_has_pp_on(struct drm_i915_private *dev_priv, int pps_idx)
{
	return intel_de_read(dev_priv, PP_STATUS(pps_idx)) & PP_ON;
}

static bool pps_has_vdd_on(struct drm_i915_private *dev_priv, int pps_idx)
{
	return intel_de_read(dev_priv, PP_CONTROL(pps_idx)) & EDP_FORCE_VDD;
}

static bool pps_any(struct drm_i915_private *dev_priv, int pps_idx)
{
	return true;
}

static enum pipe
vlv_initial_pps_pipe(struct drm_i915_private *dev_priv,
		     enum port port, pps_check check)
{
	enum pipe pipe;

	for (pipe = PIPE_A; pipe <= PIPE_B; pipe++) {
		u32 port_sel = intel_de_read(dev_priv, PP_ON_DELAYS(pipe)) &
			PANEL_PORT_SELECT_MASK;

		if (port_sel != PANEL_PORT_SELECT_VLV(port))
			continue;

		if (!check(dev_priv, pipe))
			continue;

		return pipe;
	}

	return INVALID_PIPE;
}

static void
vlv_initial_power_sequencer_setup(struct intel_dp *intel_dp)
{
	struct drm_i915_private *dev_priv = dp_to_i915(intel_dp);
	struct intel_digital_port *dig_port = dp_to_dig_port(intel_dp);
	enum port port = dig_port->base.port;

	lockdep_assert_held(&dev_priv->display.pps.mutex);

	 
	 
	intel_dp->pps.pps_pipe = vlv_initial_pps_pipe(dev_priv, port,
						      pps_has_pp_on);
	 
	if (intel_dp->pps.pps_pipe == INVALID_PIPE)
		intel_dp->pps.pps_pipe = vlv_initial_pps_pipe(dev_priv, port,
							      pps_has_vdd_on);
	 
	if (intel_dp->pps.pps_pipe == INVALID_PIPE)
		intel_dp->pps.pps_pipe = vlv_initial_pps_pipe(dev_priv, port,
							      pps_any);

	 
	if (intel_dp->pps.pps_pipe == INVALID_PIPE) {
		drm_dbg_kms(&dev_priv->drm,
			    "[ENCODER:%d:%s] no initial power sequencer\n",
			    dig_port->base.base.base.id, dig_port->base.base.name);
		return;
	}

	drm_dbg_kms(&dev_priv->drm,
		    "[ENCODER:%d:%s] initial power sequencer: %s\n",
		    dig_port->base.base.base.id, dig_port->base.base.name,
		    pps_name(dev_priv, &intel_dp->pps));
}

static int intel_num_pps(struct drm_i915_private *i915)
{
	if (IS_VALLEYVIEW(i915) || IS_CHERRYVIEW(i915))
		return 2;

	if (IS_GEMINILAKE(i915) || IS_BROXTON(i915))
		return 2;

	if (INTEL_PCH_TYPE(i915) >= PCH_DG1)
		return 1;

	if (INTEL_PCH_TYPE(i915) >= PCH_ICP)
		return 2;

	return 1;
}

static bool intel_pps_is_valid(struct intel_dp *intel_dp)
{
	struct drm_i915_private *i915 = dp_to_i915(intel_dp);

	if (intel_dp->pps.pps_idx == 1 &&
	    INTEL_PCH_TYPE(i915) >= PCH_ICP &&
	    INTEL_PCH_TYPE(i915) < PCH_MTP)
		return intel_de_read(i915, SOUTH_CHICKEN1) & ICP_SECOND_PPS_IO_SELECT;

	return true;
}

static int
bxt_initial_pps_idx(struct drm_i915_private *i915, pps_check check)
{
	int pps_idx, pps_num = intel_num_pps(i915);

	for (pps_idx = 0; pps_idx < pps_num; pps_idx++) {
		if (check(i915, pps_idx))
			return pps_idx;
	}

	return -1;
}

static bool
pps_initial_setup(struct intel_dp *intel_dp)
{
	struct intel_encoder *encoder = &dp_to_dig_port(intel_dp)->base;
	struct intel_connector *connector = intel_dp->attached_connector;
	struct drm_i915_private *i915 = to_i915(encoder->base.dev);

	lockdep_assert_held(&i915->display.pps.mutex);

	if (IS_VALLEYVIEW(i915) || IS_CHERRYVIEW(i915)) {
		vlv_initial_power_sequencer_setup(intel_dp);
		return true;
	}

	 
	if (intel_num_pps(i915) > 1)
		intel_dp->pps.pps_idx = connector->panel.vbt.backlight.controller;
	else
		intel_dp->pps.pps_idx = 0;

	if (drm_WARN_ON(&i915->drm, intel_dp->pps.pps_idx >= intel_num_pps(i915)))
		intel_dp->pps.pps_idx = -1;

	 
	if (intel_dp->pps.pps_idx < 0)
		intel_dp->pps.pps_idx = bxt_initial_pps_idx(i915, pps_has_pp_on);
	 
	if (intel_dp->pps.pps_idx < 0)
		intel_dp->pps.pps_idx = bxt_initial_pps_idx(i915, pps_has_vdd_on);
	 
	if (intel_dp->pps.pps_idx < 0) {
		intel_dp->pps.pps_idx = bxt_initial_pps_idx(i915, pps_any);

		drm_dbg_kms(&i915->drm,
			    "[ENCODER:%d:%s] no initial power sequencer, assuming %s\n",
			    encoder->base.base.id, encoder->base.name,
			    pps_name(i915, &intel_dp->pps));
	} else {
		drm_dbg_kms(&i915->drm,
			    "[ENCODER:%d:%s] initial power sequencer: %s\n",
			    encoder->base.base.id, encoder->base.name,
			    pps_name(i915, &intel_dp->pps));
	}

	return intel_pps_is_valid(intel_dp);
}

void intel_pps_reset_all(struct drm_i915_private *dev_priv)
{
	struct intel_encoder *encoder;

	if (drm_WARN_ON(&dev_priv->drm, !IS_LP(dev_priv)))
		return;

	if (!HAS_DISPLAY(dev_priv))
		return;

	 

	for_each_intel_dp(&dev_priv->drm, encoder) {
		struct intel_dp *intel_dp = enc_to_intel_dp(encoder);

		drm_WARN_ON(&dev_priv->drm,
			    intel_dp->pps.active_pipe != INVALID_PIPE);

		if (encoder->type != INTEL_OUTPUT_EDP)
			continue;

		if (DISPLAY_VER(dev_priv) >= 9)
			intel_dp->pps.pps_reset = true;
		else
			intel_dp->pps.pps_pipe = INVALID_PIPE;
	}
}

struct pps_registers {
	i915_reg_t pp_ctrl;
	i915_reg_t pp_stat;
	i915_reg_t pp_on;
	i915_reg_t pp_off;
	i915_reg_t pp_div;
};

static void intel_pps_get_registers(struct intel_dp *intel_dp,
				    struct pps_registers *regs)
{
	struct drm_i915_private *dev_priv = dp_to_i915(intel_dp);
	int pps_idx;

	memset(regs, 0, sizeof(*regs));

	if (IS_VALLEYVIEW(dev_priv) || IS_CHERRYVIEW(dev_priv))
		pps_idx = vlv_power_sequencer_pipe(intel_dp);
	else if (IS_GEMINILAKE(dev_priv) || IS_BROXTON(dev_priv))
		pps_idx = bxt_power_sequencer_idx(intel_dp);
	else
		pps_idx = intel_dp->pps.pps_idx;

	regs->pp_ctrl = PP_CONTROL(pps_idx);
	regs->pp_stat = PP_STATUS(pps_idx);
	regs->pp_on = PP_ON_DELAYS(pps_idx);
	regs->pp_off = PP_OFF_DELAYS(pps_idx);

	 
	if (IS_GEMINILAKE(dev_priv) || IS_BROXTON(dev_priv) ||
	    INTEL_PCH_TYPE(dev_priv) >= PCH_CNP)
		regs->pp_div = INVALID_MMIO_REG;
	else
		regs->pp_div = PP_DIVISOR(pps_idx);
}

static i915_reg_t
_pp_ctrl_reg(struct intel_dp *intel_dp)
{
	struct pps_registers regs;

	intel_pps_get_registers(intel_dp, &regs);

	return regs.pp_ctrl;
}

static i915_reg_t
_pp_stat_reg(struct intel_dp *intel_dp)
{
	struct pps_registers regs;

	intel_pps_get_registers(intel_dp, &regs);

	return regs.pp_stat;
}

static bool edp_have_panel_power(struct intel_dp *intel_dp)
{
	struct drm_i915_private *dev_priv = dp_to_i915(intel_dp);

	lockdep_assert_held(&dev_priv->display.pps.mutex);

	if ((IS_VALLEYVIEW(dev_priv) || IS_CHERRYVIEW(dev_priv)) &&
	    intel_dp->pps.pps_pipe == INVALID_PIPE)
		return false;

	return (intel_de_read(dev_priv, _pp_stat_reg(intel_dp)) & PP_ON) != 0;
}

static bool edp_have_panel_vdd(struct intel_dp *intel_dp)
{
	struct drm_i915_private *dev_priv = dp_to_i915(intel_dp);

	lockdep_assert_held(&dev_priv->display.pps.mutex);

	if ((IS_VALLEYVIEW(dev_priv) || IS_CHERRYVIEW(dev_priv)) &&
	    intel_dp->pps.pps_pipe == INVALID_PIPE)
		return false;

	return intel_de_read(dev_priv, _pp_ctrl_reg(intel_dp)) & EDP_FORCE_VDD;
}

void intel_pps_check_power_unlocked(struct intel_dp *intel_dp)
{
	struct drm_i915_private *dev_priv = dp_to_i915(intel_dp);
	struct intel_digital_port *dig_port = dp_to_dig_port(intel_dp);

	if (!intel_dp_is_edp(intel_dp))
		return;

	if (!edp_have_panel_power(intel_dp) && !edp_have_panel_vdd(intel_dp)) {
		drm_WARN(&dev_priv->drm, 1,
			 "[ENCODER:%d:%s] %s powered off while attempting AUX CH communication.\n",
			 dig_port->base.base.base.id, dig_port->base.base.name,
			 pps_name(dev_priv, &intel_dp->pps));
		drm_dbg_kms(&dev_priv->drm,
			    "[ENCODER:%d:%s] %s PP_STATUS: 0x%08x PP_CONTROL: 0x%08x\n",
			    dig_port->base.base.base.id, dig_port->base.base.name,
			    pps_name(dev_priv, &intel_dp->pps),
			    intel_de_read(dev_priv, _pp_stat_reg(intel_dp)),
			    intel_de_read(dev_priv, _pp_ctrl_reg(intel_dp)));
	}
}

#define IDLE_ON_MASK		(PP_ON | PP_SEQUENCE_MASK | 0                     | PP_SEQUENCE_STATE_MASK)
#define IDLE_ON_VALUE		(PP_ON | PP_SEQUENCE_NONE | 0                     | PP_SEQUENCE_STATE_ON_IDLE)

#define IDLE_OFF_MASK		(PP_ON | PP_SEQUENCE_MASK | 0                     | 0)
#define IDLE_OFF_VALUE		(0     | PP_SEQUENCE_NONE | 0                     | 0)

#define IDLE_CYCLE_MASK		(PP_ON | PP_SEQUENCE_MASK | PP_CYCLE_DELAY_ACTIVE | PP_SEQUENCE_STATE_MASK)
#define IDLE_CYCLE_VALUE	(0     | PP_SEQUENCE_NONE | 0                     | PP_SEQUENCE_STATE_OFF_IDLE)

static void intel_pps_verify_state(struct intel_dp *intel_dp);

static void wait_panel_status(struct intel_dp *intel_dp,
			      u32 mask, u32 value)
{
	struct drm_i915_private *dev_priv = dp_to_i915(intel_dp);
	struct intel_digital_port *dig_port = dp_to_dig_port(intel_dp);
	i915_reg_t pp_stat_reg, pp_ctrl_reg;

	lockdep_assert_held(&dev_priv->display.pps.mutex);

	intel_pps_verify_state(intel_dp);

	pp_stat_reg = _pp_stat_reg(intel_dp);
	pp_ctrl_reg = _pp_ctrl_reg(intel_dp);

	drm_dbg_kms(&dev_priv->drm,
		    "[ENCODER:%d:%s] %s mask: 0x%08x value: 0x%08x PP_STATUS: 0x%08x PP_CONTROL: 0x%08x\n",
		    dig_port->base.base.base.id, dig_port->base.base.name,
		    pps_name(dev_priv, &intel_dp->pps),
		    mask, value,
		    intel_de_read(dev_priv, pp_stat_reg),
		    intel_de_read(dev_priv, pp_ctrl_reg));

	if (intel_de_wait_for_register(dev_priv, pp_stat_reg,
				       mask, value, 5000))
		drm_err(&dev_priv->drm,
			"[ENCODER:%d:%s] %s panel status timeout: PP_STATUS: 0x%08x PP_CONTROL: 0x%08x\n",
			dig_port->base.base.base.id, dig_port->base.base.name,
			pps_name(dev_priv, &intel_dp->pps),
			intel_de_read(dev_priv, pp_stat_reg),
			intel_de_read(dev_priv, pp_ctrl_reg));

	drm_dbg_kms(&dev_priv->drm, "Wait complete\n");
}

static void wait_panel_on(struct intel_dp *intel_dp)
{
	struct drm_i915_private *i915 = dp_to_i915(intel_dp);
	struct intel_digital_port *dig_port = dp_to_dig_port(intel_dp);

	drm_dbg_kms(&i915->drm, "[ENCODER:%d:%s] %s wait for panel power on\n",
		    dig_port->base.base.base.id, dig_port->base.base.name,
		    pps_name(i915, &intel_dp->pps));
	wait_panel_status(intel_dp, IDLE_ON_MASK, IDLE_ON_VALUE);
}

static void wait_panel_off(struct intel_dp *intel_dp)
{
	struct drm_i915_private *i915 = dp_to_i915(intel_dp);
	struct intel_digital_port *dig_port = dp_to_dig_port(intel_dp);

	drm_dbg_kms(&i915->drm, "[ENCODER:%d:%s] %s wait for panel power off time\n",
		    dig_port->base.base.base.id, dig_port->base.base.name,
		    pps_name(i915, &intel_dp->pps));
	wait_panel_status(intel_dp, IDLE_OFF_MASK, IDLE_OFF_VALUE);
}

static void wait_panel_power_cycle(struct intel_dp *intel_dp)
{
	struct drm_i915_private *i915 = dp_to_i915(intel_dp);
	struct intel_digital_port *dig_port = dp_to_dig_port(intel_dp);
	ktime_t panel_power_on_time;
	s64 panel_power_off_duration;

	drm_dbg_kms(&i915->drm, "[ENCODER:%d:%s] %s wait for panel power cycle\n",
		    dig_port->base.base.base.id, dig_port->base.base.name,
		    pps_name(i915, &intel_dp->pps));

	 
	panel_power_on_time = ktime_get_boottime();
	panel_power_off_duration = ktime_ms_delta(panel_power_on_time, intel_dp->pps.panel_power_off_time);

	 
	if (panel_power_off_duration < (s64)intel_dp->pps.panel_power_cycle_delay)
		wait_remaining_ms_from_jiffies(jiffies,
				       intel_dp->pps.panel_power_cycle_delay - panel_power_off_duration);

	wait_panel_status(intel_dp, IDLE_CYCLE_MASK, IDLE_CYCLE_VALUE);
}

void intel_pps_wait_power_cycle(struct intel_dp *intel_dp)
{
	intel_wakeref_t wakeref;

	if (!intel_dp_is_edp(intel_dp))
		return;

	with_intel_pps_lock(intel_dp, wakeref)
		wait_panel_power_cycle(intel_dp);
}

static void wait_backlight_on(struct intel_dp *intel_dp)
{
	wait_remaining_ms_from_jiffies(intel_dp->pps.last_power_on,
				       intel_dp->pps.backlight_on_delay);
}

static void edp_wait_backlight_off(struct intel_dp *intel_dp)
{
	wait_remaining_ms_from_jiffies(intel_dp->pps.last_backlight_off,
				       intel_dp->pps.backlight_off_delay);
}

 

static  u32 ilk_get_pp_control(struct intel_dp *intel_dp)
{
	struct drm_i915_private *dev_priv = dp_to_i915(intel_dp);
	u32 control;

	lockdep_assert_held(&dev_priv->display.pps.mutex);

	control = intel_de_read(dev_priv, _pp_ctrl_reg(intel_dp));
	if (drm_WARN_ON(&dev_priv->drm, !HAS_DDI(dev_priv) &&
			(control & PANEL_UNLOCK_MASK) != PANEL_UNLOCK_REGS)) {
		control &= ~PANEL_UNLOCK_MASK;
		control |= PANEL_UNLOCK_REGS;
	}
	return control;
}

 
bool intel_pps_vdd_on_unlocked(struct intel_dp *intel_dp)
{
	struct drm_i915_private *dev_priv = dp_to_i915(intel_dp);
	struct intel_digital_port *dig_port = dp_to_dig_port(intel_dp);
	u32 pp;
	i915_reg_t pp_stat_reg, pp_ctrl_reg;
	bool need_to_disable = !intel_dp->pps.want_panel_vdd;

	lockdep_assert_held(&dev_priv->display.pps.mutex);

	if (!intel_dp_is_edp(intel_dp))
		return false;

	cancel_delayed_work(&intel_dp->pps.panel_vdd_work);
	intel_dp->pps.want_panel_vdd = true;

	if (edp_have_panel_vdd(intel_dp))
		return need_to_disable;

	drm_WARN_ON(&dev_priv->drm, intel_dp->pps.vdd_wakeref);
	intel_dp->pps.vdd_wakeref = intel_display_power_get(dev_priv,
							    intel_aux_power_domain(dig_port));

	pp_stat_reg = _pp_stat_reg(intel_dp);
	pp_ctrl_reg = _pp_ctrl_reg(intel_dp);

	drm_dbg_kms(&dev_priv->drm, "[ENCODER:%d:%s] %s turning VDD on\n",
		    dig_port->base.base.base.id, dig_port->base.base.name,
		    pps_name(dev_priv, &intel_dp->pps));

	if (!edp_have_panel_power(intel_dp))
		wait_panel_power_cycle(intel_dp);

	pp = ilk_get_pp_control(intel_dp);
	pp |= EDP_FORCE_VDD;

	intel_de_write(dev_priv, pp_ctrl_reg, pp);
	intel_de_posting_read(dev_priv, pp_ctrl_reg);
	drm_dbg_kms(&dev_priv->drm, "[ENCODER:%d:%s] %s PP_STATUS: 0x%08x PP_CONTROL: 0x%08x\n",
		    dig_port->base.base.base.id, dig_port->base.base.name,
		    pps_name(dev_priv, &intel_dp->pps),
		    intel_de_read(dev_priv, pp_stat_reg),
		    intel_de_read(dev_priv, pp_ctrl_reg));
	 
	if (!edp_have_panel_power(intel_dp)) {
		drm_dbg_kms(&dev_priv->drm,
			    "[ENCODER:%d:%s] %s panel power wasn't enabled\n",
			    dig_port->base.base.base.id, dig_port->base.base.name,
			    pps_name(dev_priv, &intel_dp->pps));
		msleep(intel_dp->pps.panel_power_up_delay);
	}

	return need_to_disable;
}

 
void intel_pps_vdd_on(struct intel_dp *intel_dp)
{
	struct drm_i915_private *i915 = dp_to_i915(intel_dp);
	intel_wakeref_t wakeref;
	bool vdd;

	if (!intel_dp_is_edp(intel_dp))
		return;

	vdd = false;
	with_intel_pps_lock(intel_dp, wakeref)
		vdd = intel_pps_vdd_on_unlocked(intel_dp);
	I915_STATE_WARN(i915, !vdd, "[ENCODER:%d:%s] %s VDD already requested on\n",
			dp_to_dig_port(intel_dp)->base.base.base.id,
			dp_to_dig_port(intel_dp)->base.base.name,
			pps_name(i915, &intel_dp->pps));
}

static void intel_pps_vdd_off_sync_unlocked(struct intel_dp *intel_dp)
{
	struct drm_i915_private *dev_priv = dp_to_i915(intel_dp);
	struct intel_digital_port *dig_port =
		dp_to_dig_port(intel_dp);
	u32 pp;
	i915_reg_t pp_stat_reg, pp_ctrl_reg;

	lockdep_assert_held(&dev_priv->display.pps.mutex);

	drm_WARN_ON(&dev_priv->drm, intel_dp->pps.want_panel_vdd);

	if (!edp_have_panel_vdd(intel_dp))
		return;

	drm_dbg_kms(&dev_priv->drm, "[ENCODER:%d:%s] %s turning VDD off\n",
		    dig_port->base.base.base.id, dig_port->base.base.name,
		    pps_name(dev_priv, &intel_dp->pps));

	pp = ilk_get_pp_control(intel_dp);
	pp &= ~EDP_FORCE_VDD;

	pp_ctrl_reg = _pp_ctrl_reg(intel_dp);
	pp_stat_reg = _pp_stat_reg(intel_dp);

	intel_de_write(dev_priv, pp_ctrl_reg, pp);
	intel_de_posting_read(dev_priv, pp_ctrl_reg);

	 
	drm_dbg_kms(&dev_priv->drm, "[ENCODER:%d:%s] %s PP_STATUS: 0x%08x PP_CONTROL: 0x%08x\n",
		    dig_port->base.base.base.id, dig_port->base.base.name,
		    pps_name(dev_priv, &intel_dp->pps),
		    intel_de_read(dev_priv, pp_stat_reg),
		    intel_de_read(dev_priv, pp_ctrl_reg));

	if ((pp & PANEL_POWER_ON) == 0)
		intel_dp->pps.panel_power_off_time = ktime_get_boottime();

	intel_display_power_put(dev_priv,
				intel_aux_power_domain(dig_port),
				fetch_and_zero(&intel_dp->pps.vdd_wakeref));
}

void intel_pps_vdd_off_sync(struct intel_dp *intel_dp)
{
	intel_wakeref_t wakeref;

	if (!intel_dp_is_edp(intel_dp))
		return;

	cancel_delayed_work_sync(&intel_dp->pps.panel_vdd_work);
	 
	with_intel_pps_lock(intel_dp, wakeref)
		intel_pps_vdd_off_sync_unlocked(intel_dp);
}

static void edp_panel_vdd_work(struct work_struct *__work)
{
	struct intel_pps *pps = container_of(to_delayed_work(__work),
					     struct intel_pps, panel_vdd_work);
	struct intel_dp *intel_dp = container_of(pps, struct intel_dp, pps);
	intel_wakeref_t wakeref;

	with_intel_pps_lock(intel_dp, wakeref) {
		if (!intel_dp->pps.want_panel_vdd)
			intel_pps_vdd_off_sync_unlocked(intel_dp);
	}
}

static void edp_panel_vdd_schedule_off(struct intel_dp *intel_dp)
{
	struct drm_i915_private *i915 = dp_to_i915(intel_dp);
	unsigned long delay;

	 
	if (intel_dp->pps.initializing)
		return;

	 
	delay = msecs_to_jiffies(intel_dp->pps.panel_power_cycle_delay * 5);
	queue_delayed_work(i915->unordered_wq,
			   &intel_dp->pps.panel_vdd_work, delay);
}

 
void intel_pps_vdd_off_unlocked(struct intel_dp *intel_dp, bool sync)
{
	struct drm_i915_private *dev_priv = dp_to_i915(intel_dp);

	lockdep_assert_held(&dev_priv->display.pps.mutex);

	if (!intel_dp_is_edp(intel_dp))
		return;

	I915_STATE_WARN(dev_priv, !intel_dp->pps.want_panel_vdd,
			"[ENCODER:%d:%s] %s VDD not forced on",
			dp_to_dig_port(intel_dp)->base.base.base.id,
			dp_to_dig_port(intel_dp)->base.base.name,
			pps_name(dev_priv, &intel_dp->pps));

	intel_dp->pps.want_panel_vdd = false;

	if (sync)
		intel_pps_vdd_off_sync_unlocked(intel_dp);
	else
		edp_panel_vdd_schedule_off(intel_dp);
}

void intel_pps_on_unlocked(struct intel_dp *intel_dp)
{
	struct drm_i915_private *dev_priv = dp_to_i915(intel_dp);
	u32 pp;
	i915_reg_t pp_ctrl_reg;

	lockdep_assert_held(&dev_priv->display.pps.mutex);

	if (!intel_dp_is_edp(intel_dp))
		return;

	drm_dbg_kms(&dev_priv->drm, "[ENCODER:%d:%s] %s turn panel power on\n",
		    dp_to_dig_port(intel_dp)->base.base.base.id,
		    dp_to_dig_port(intel_dp)->base.base.name,
		    pps_name(dev_priv, &intel_dp->pps));

	if (drm_WARN(&dev_priv->drm, edp_have_panel_power(intel_dp),
		     "[ENCODER:%d:%s] %s panel power already on\n",
		     dp_to_dig_port(intel_dp)->base.base.base.id,
		     dp_to_dig_port(intel_dp)->base.base.name,
		     pps_name(dev_priv, &intel_dp->pps)))
		return;

	wait_panel_power_cycle(intel_dp);

	pp_ctrl_reg = _pp_ctrl_reg(intel_dp);
	pp = ilk_get_pp_control(intel_dp);
	if (IS_IRONLAKE(dev_priv)) {
		 
		pp &= ~PANEL_POWER_RESET;
		intel_de_write(dev_priv, pp_ctrl_reg, pp);
		intel_de_posting_read(dev_priv, pp_ctrl_reg);
	}

	pp |= PANEL_POWER_ON;
	if (!IS_IRONLAKE(dev_priv))
		pp |= PANEL_POWER_RESET;

	intel_de_write(dev_priv, pp_ctrl_reg, pp);
	intel_de_posting_read(dev_priv, pp_ctrl_reg);

	wait_panel_on(intel_dp);
	intel_dp->pps.last_power_on = jiffies;

	if (IS_IRONLAKE(dev_priv)) {
		pp |= PANEL_POWER_RESET;  
		intel_de_write(dev_priv, pp_ctrl_reg, pp);
		intel_de_posting_read(dev_priv, pp_ctrl_reg);
	}
}

void intel_pps_on(struct intel_dp *intel_dp)
{
	intel_wakeref_t wakeref;

	if (!intel_dp_is_edp(intel_dp))
		return;

	with_intel_pps_lock(intel_dp, wakeref)
		intel_pps_on_unlocked(intel_dp);
}

void intel_pps_off_unlocked(struct intel_dp *intel_dp)
{
	struct drm_i915_private *dev_priv = dp_to_i915(intel_dp);
	struct intel_digital_port *dig_port = dp_to_dig_port(intel_dp);
	u32 pp;
	i915_reg_t pp_ctrl_reg;

	lockdep_assert_held(&dev_priv->display.pps.mutex);

	if (!intel_dp_is_edp(intel_dp))
		return;

	drm_dbg_kms(&dev_priv->drm, "[ENCODER:%d:%s] %s turn panel power off\n",
		    dig_port->base.base.base.id, dig_port->base.base.name,
		    pps_name(dev_priv, &intel_dp->pps));

	drm_WARN(&dev_priv->drm, !intel_dp->pps.want_panel_vdd,
		 "[ENCODER:%d:%s] %s need VDD to turn off panel\n",
		 dig_port->base.base.base.id, dig_port->base.base.name,
		 pps_name(dev_priv, &intel_dp->pps));

	pp = ilk_get_pp_control(intel_dp);
	 
	pp &= ~(PANEL_POWER_ON | PANEL_POWER_RESET | EDP_FORCE_VDD |
		EDP_BLC_ENABLE);

	pp_ctrl_reg = _pp_ctrl_reg(intel_dp);

	intel_dp->pps.want_panel_vdd = false;

	intel_de_write(dev_priv, pp_ctrl_reg, pp);
	intel_de_posting_read(dev_priv, pp_ctrl_reg);

	wait_panel_off(intel_dp);
	intel_dp->pps.panel_power_off_time = ktime_get_boottime();

	 
	intel_display_power_put(dev_priv,
				intel_aux_power_domain(dig_port),
				fetch_and_zero(&intel_dp->pps.vdd_wakeref));
}

void intel_pps_off(struct intel_dp *intel_dp)
{
	intel_wakeref_t wakeref;

	if (!intel_dp_is_edp(intel_dp))
		return;

	with_intel_pps_lock(intel_dp, wakeref)
		intel_pps_off_unlocked(intel_dp);
}

 
void intel_pps_backlight_on(struct intel_dp *intel_dp)
{
	struct drm_i915_private *dev_priv = dp_to_i915(intel_dp);
	intel_wakeref_t wakeref;

	 
	wait_backlight_on(intel_dp);

	with_intel_pps_lock(intel_dp, wakeref) {
		i915_reg_t pp_ctrl_reg = _pp_ctrl_reg(intel_dp);
		u32 pp;

		pp = ilk_get_pp_control(intel_dp);
		pp |= EDP_BLC_ENABLE;

		intel_de_write(dev_priv, pp_ctrl_reg, pp);
		intel_de_posting_read(dev_priv, pp_ctrl_reg);
	}
}

 
void intel_pps_backlight_off(struct intel_dp *intel_dp)
{
	struct drm_i915_private *dev_priv = dp_to_i915(intel_dp);
	intel_wakeref_t wakeref;

	if (!intel_dp_is_edp(intel_dp))
		return;

	with_intel_pps_lock(intel_dp, wakeref) {
		i915_reg_t pp_ctrl_reg = _pp_ctrl_reg(intel_dp);
		u32 pp;

		pp = ilk_get_pp_control(intel_dp);
		pp &= ~EDP_BLC_ENABLE;

		intel_de_write(dev_priv, pp_ctrl_reg, pp);
		intel_de_posting_read(dev_priv, pp_ctrl_reg);
	}

	intel_dp->pps.last_backlight_off = jiffies;
	edp_wait_backlight_off(intel_dp);
}

 
void intel_pps_backlight_power(struct intel_connector *connector, bool enable)
{
	struct drm_i915_private *i915 = to_i915(connector->base.dev);
	struct intel_dp *intel_dp = intel_attached_dp(connector);
	intel_wakeref_t wakeref;
	bool is_enabled;

	is_enabled = false;
	with_intel_pps_lock(intel_dp, wakeref)
		is_enabled = ilk_get_pp_control(intel_dp) & EDP_BLC_ENABLE;
	if (is_enabled == enable)
		return;

	drm_dbg_kms(&i915->drm, "panel power control backlight %s\n",
		    enable ? "enable" : "disable");

	if (enable)
		intel_pps_backlight_on(intel_dp);
	else
		intel_pps_backlight_off(intel_dp);
}

static void vlv_detach_power_sequencer(struct intel_dp *intel_dp)
{
	struct intel_digital_port *dig_port = dp_to_dig_port(intel_dp);
	struct drm_i915_private *dev_priv = to_i915(dig_port->base.base.dev);
	enum pipe pipe = intel_dp->pps.pps_pipe;
	i915_reg_t pp_on_reg = PP_ON_DELAYS(pipe);

	drm_WARN_ON(&dev_priv->drm, intel_dp->pps.active_pipe != INVALID_PIPE);

	if (drm_WARN_ON(&dev_priv->drm, pipe != PIPE_A && pipe != PIPE_B))
		return;

	intel_pps_vdd_off_sync_unlocked(intel_dp);

	 
	drm_dbg_kms(&dev_priv->drm,
		    "detaching %s from [ENCODER:%d:%s]\n",
		    pps_name(dev_priv, &intel_dp->pps),
		    dig_port->base.base.base.id, dig_port->base.base.name);
	intel_de_write(dev_priv, pp_on_reg, 0);
	intel_de_posting_read(dev_priv, pp_on_reg);

	intel_dp->pps.pps_pipe = INVALID_PIPE;
}

static void vlv_steal_power_sequencer(struct drm_i915_private *dev_priv,
				      enum pipe pipe)
{
	struct intel_encoder *encoder;

	lockdep_assert_held(&dev_priv->display.pps.mutex);

	for_each_intel_dp(&dev_priv->drm, encoder) {
		struct intel_dp *intel_dp = enc_to_intel_dp(encoder);

		drm_WARN(&dev_priv->drm, intel_dp->pps.active_pipe == pipe,
			 "stealing PPS %c from active [ENCODER:%d:%s]\n",
			 pipe_name(pipe), encoder->base.base.id,
			 encoder->base.name);

		if (intel_dp->pps.pps_pipe != pipe)
			continue;

		drm_dbg_kms(&dev_priv->drm,
			    "stealing PPS %c from [ENCODER:%d:%s]\n",
			    pipe_name(pipe), encoder->base.base.id,
			    encoder->base.name);

		 
		vlv_detach_power_sequencer(intel_dp);
	}
}

void vlv_pps_init(struct intel_encoder *encoder,
		  const struct intel_crtc_state *crtc_state)
{
	struct drm_i915_private *dev_priv = to_i915(encoder->base.dev);
	struct intel_dp *intel_dp = enc_to_intel_dp(encoder);
	struct intel_crtc *crtc = to_intel_crtc(crtc_state->uapi.crtc);

	lockdep_assert_held(&dev_priv->display.pps.mutex);

	drm_WARN_ON(&dev_priv->drm, intel_dp->pps.active_pipe != INVALID_PIPE);

	if (intel_dp->pps.pps_pipe != INVALID_PIPE &&
	    intel_dp->pps.pps_pipe != crtc->pipe) {
		 
		vlv_detach_power_sequencer(intel_dp);
	}

	 
	vlv_steal_power_sequencer(dev_priv, crtc->pipe);

	intel_dp->pps.active_pipe = crtc->pipe;

	if (!intel_dp_is_edp(intel_dp))
		return;

	 
	intel_dp->pps.pps_pipe = crtc->pipe;

	drm_dbg_kms(&dev_priv->drm,
		    "initializing %s for [ENCODER:%d:%s]\n",
		    pps_name(dev_priv, &intel_dp->pps),
		    encoder->base.base.id, encoder->base.name);

	 
	pps_init_delays(intel_dp);
	pps_init_registers(intel_dp, true);
}

static void pps_vdd_init(struct intel_dp *intel_dp)
{
	struct drm_i915_private *dev_priv = dp_to_i915(intel_dp);
	struct intel_digital_port *dig_port = dp_to_dig_port(intel_dp);

	lockdep_assert_held(&dev_priv->display.pps.mutex);

	if (!edp_have_panel_vdd(intel_dp))
		return;

	 
	drm_dbg_kms(&dev_priv->drm,
		    "[ENCODER:%d:%s] %s VDD left on by BIOS, adjusting state tracking\n",
		    dig_port->base.base.base.id, dig_port->base.base.name,
		    pps_name(dev_priv, &intel_dp->pps));
	drm_WARN_ON(&dev_priv->drm, intel_dp->pps.vdd_wakeref);
	intel_dp->pps.vdd_wakeref = intel_display_power_get(dev_priv,
							    intel_aux_power_domain(dig_port));
}

bool intel_pps_have_panel_power_or_vdd(struct intel_dp *intel_dp)
{
	intel_wakeref_t wakeref;
	bool have_power = false;

	with_intel_pps_lock(intel_dp, wakeref) {
		have_power = edp_have_panel_power(intel_dp) ||
			     edp_have_panel_vdd(intel_dp);
	}

	return have_power;
}

static void pps_init_timestamps(struct intel_dp *intel_dp)
{
	 
	intel_dp->pps.panel_power_off_time = 0;
	intel_dp->pps.last_power_on = jiffies;
	intel_dp->pps.last_backlight_off = jiffies;
}

static void
intel_pps_readout_hw_state(struct intel_dp *intel_dp, struct edp_power_seq *seq)
{
	struct drm_i915_private *dev_priv = dp_to_i915(intel_dp);
	u32 pp_on, pp_off, pp_ctl;
	struct pps_registers regs;

	intel_pps_get_registers(intel_dp, &regs);

	pp_ctl = ilk_get_pp_control(intel_dp);

	 
	if (!HAS_DDI(dev_priv))
		intel_de_write(dev_priv, regs.pp_ctrl, pp_ctl);

	pp_on = intel_de_read(dev_priv, regs.pp_on);
	pp_off = intel_de_read(dev_priv, regs.pp_off);

	 
	seq->t1_t3 = REG_FIELD_GET(PANEL_POWER_UP_DELAY_MASK, pp_on);
	seq->t8 = REG_FIELD_GET(PANEL_LIGHT_ON_DELAY_MASK, pp_on);
	seq->t9 = REG_FIELD_GET(PANEL_LIGHT_OFF_DELAY_MASK, pp_off);
	seq->t10 = REG_FIELD_GET(PANEL_POWER_DOWN_DELAY_MASK, pp_off);

	if (i915_mmio_reg_valid(regs.pp_div)) {
		u32 pp_div;

		pp_div = intel_de_read(dev_priv, regs.pp_div);

		seq->t11_t12 = REG_FIELD_GET(PANEL_POWER_CYCLE_DELAY_MASK, pp_div) * 1000;
	} else {
		seq->t11_t12 = REG_FIELD_GET(BXT_POWER_CYCLE_DELAY_MASK, pp_ctl) * 1000;
	}
}

static void
intel_pps_dump_state(struct intel_dp *intel_dp, const char *state_name,
		     const struct edp_power_seq *seq)
{
	struct drm_i915_private *i915 = dp_to_i915(intel_dp);

	drm_dbg_kms(&i915->drm, "%s t1_t3 %d t8 %d t9 %d t10 %d t11_t12 %d\n",
		    state_name,
		    seq->t1_t3, seq->t8, seq->t9, seq->t10, seq->t11_t12);
}

static void
intel_pps_verify_state(struct intel_dp *intel_dp)
{
	struct drm_i915_private *i915 = dp_to_i915(intel_dp);
	struct edp_power_seq hw;
	struct edp_power_seq *sw = &intel_dp->pps.pps_delays;

	intel_pps_readout_hw_state(intel_dp, &hw);

	if (hw.t1_t3 != sw->t1_t3 || hw.t8 != sw->t8 || hw.t9 != sw->t9 ||
	    hw.t10 != sw->t10 || hw.t11_t12 != sw->t11_t12) {
		drm_err(&i915->drm, "PPS state mismatch\n");
		intel_pps_dump_state(intel_dp, "sw", sw);
		intel_pps_dump_state(intel_dp, "hw", &hw);
	}
}

static bool pps_delays_valid(struct edp_power_seq *delays)
{
	return delays->t1_t3 || delays->t8 || delays->t9 ||
		delays->t10 || delays->t11_t12;
}

static void pps_init_delays_bios(struct intel_dp *intel_dp,
				 struct edp_power_seq *bios)
{
	struct drm_i915_private *dev_priv = dp_to_i915(intel_dp);

	lockdep_assert_held(&dev_priv->display.pps.mutex);

	if (!pps_delays_valid(&intel_dp->pps.bios_pps_delays))
		intel_pps_readout_hw_state(intel_dp, &intel_dp->pps.bios_pps_delays);

	*bios = intel_dp->pps.bios_pps_delays;

	intel_pps_dump_state(intel_dp, "bios", bios);
}

static void pps_init_delays_vbt(struct intel_dp *intel_dp,
				struct edp_power_seq *vbt)
{
	struct drm_i915_private *dev_priv = dp_to_i915(intel_dp);
	struct intel_connector *connector = intel_dp->attached_connector;

	*vbt = connector->panel.vbt.edp.pps;

	if (!pps_delays_valid(vbt))
		return;

	 
	if (intel_has_quirk(dev_priv, QUIRK_INCREASE_T12_DELAY)) {
		vbt->t11_t12 = max_t(u16, vbt->t11_t12, 1300 * 10);
		drm_dbg_kms(&dev_priv->drm,
			    "Increasing T12 panel delay as per the quirk to %d\n",
			    vbt->t11_t12);
	}

	 
	vbt->t11_t12 += 100 * 10;

	intel_pps_dump_state(intel_dp, "vbt", vbt);
}

static void pps_init_delays_spec(struct intel_dp *intel_dp,
				 struct edp_power_seq *spec)
{
	struct drm_i915_private *dev_priv = dp_to_i915(intel_dp);

	lockdep_assert_held(&dev_priv->display.pps.mutex);

	 
	spec->t1_t3 = 210 * 10;
	spec->t8 = 50 * 10;  
	spec->t9 = 50 * 10;  
	spec->t10 = 500 * 10;
	 
	spec->t11_t12 = (510 + 100) * 10;

	intel_pps_dump_state(intel_dp, "spec", spec);
}

static void pps_init_delays(struct intel_dp *intel_dp)
{
	struct drm_i915_private *dev_priv = dp_to_i915(intel_dp);
	struct edp_power_seq cur, vbt, spec,
		*final = &intel_dp->pps.pps_delays;

	lockdep_assert_held(&dev_priv->display.pps.mutex);

	 
	if (pps_delays_valid(final))
		return;

	pps_init_delays_bios(intel_dp, &cur);
	pps_init_delays_vbt(intel_dp, &vbt);
	pps_init_delays_spec(intel_dp, &spec);

	 
#define assign_final(field)	final->field = (max(cur.field, vbt.field) == 0 ? \
				       spec.field : \
				       max(cur.field, vbt.field))
	assign_final(t1_t3);
	assign_final(t8);
	assign_final(t9);
	assign_final(t10);
	assign_final(t11_t12);
#undef assign_final

#define get_delay(field)	(DIV_ROUND_UP(final->field, 10))
	intel_dp->pps.panel_power_up_delay = get_delay(t1_t3);
	intel_dp->pps.backlight_on_delay = get_delay(t8);
	intel_dp->pps.backlight_off_delay = get_delay(t9);
	intel_dp->pps.panel_power_down_delay = get_delay(t10);
	intel_dp->pps.panel_power_cycle_delay = get_delay(t11_t12);
#undef get_delay

	drm_dbg_kms(&dev_priv->drm,
		    "panel power up delay %d, power down delay %d, power cycle delay %d\n",
		    intel_dp->pps.panel_power_up_delay,
		    intel_dp->pps.panel_power_down_delay,
		    intel_dp->pps.panel_power_cycle_delay);

	drm_dbg_kms(&dev_priv->drm, "backlight on delay %d, off delay %d\n",
		    intel_dp->pps.backlight_on_delay,
		    intel_dp->pps.backlight_off_delay);

	 
	final->t8 = 1;
	final->t9 = 1;

	 
	final->t11_t12 = roundup(final->t11_t12, 100 * 10);
}

static void pps_init_registers(struct intel_dp *intel_dp, bool force_disable_vdd)
{
	struct drm_i915_private *dev_priv = dp_to_i915(intel_dp);
	u32 pp_on, pp_off, port_sel = 0;
	int div = RUNTIME_INFO(dev_priv)->rawclk_freq / 1000;
	struct pps_registers regs;
	enum port port = dp_to_dig_port(intel_dp)->base.port;
	const struct edp_power_seq *seq = &intel_dp->pps.pps_delays;

	lockdep_assert_held(&dev_priv->display.pps.mutex);

	intel_pps_get_registers(intel_dp, &regs);

	 
	if (force_disable_vdd) {
		u32 pp = ilk_get_pp_control(intel_dp);

		drm_WARN(&dev_priv->drm, pp & PANEL_POWER_ON,
			 "Panel power already on\n");

		if (pp & EDP_FORCE_VDD)
			drm_dbg_kms(&dev_priv->drm,
				    "VDD already on, disabling first\n");

		pp &= ~EDP_FORCE_VDD;

		intel_de_write(dev_priv, regs.pp_ctrl, pp);
	}

	pp_on = REG_FIELD_PREP(PANEL_POWER_UP_DELAY_MASK, seq->t1_t3) |
		REG_FIELD_PREP(PANEL_LIGHT_ON_DELAY_MASK, seq->t8);
	pp_off = REG_FIELD_PREP(PANEL_LIGHT_OFF_DELAY_MASK, seq->t9) |
		REG_FIELD_PREP(PANEL_POWER_DOWN_DELAY_MASK, seq->t10);

	 
	if (IS_VALLEYVIEW(dev_priv) || IS_CHERRYVIEW(dev_priv)) {
		port_sel = PANEL_PORT_SELECT_VLV(port);
	} else if (HAS_PCH_IBX(dev_priv) || HAS_PCH_CPT(dev_priv)) {
		switch (port) {
		case PORT_A:
			port_sel = PANEL_PORT_SELECT_DPA;
			break;
		case PORT_C:
			port_sel = PANEL_PORT_SELECT_DPC;
			break;
		case PORT_D:
			port_sel = PANEL_PORT_SELECT_DPD;
			break;
		default:
			MISSING_CASE(port);
			break;
		}
	}

	pp_on |= port_sel;

	intel_de_write(dev_priv, regs.pp_on, pp_on);
	intel_de_write(dev_priv, regs.pp_off, pp_off);

	 
	if (i915_mmio_reg_valid(regs.pp_div))
		intel_de_write(dev_priv, regs.pp_div,
			       REG_FIELD_PREP(PP_REFERENCE_DIVIDER_MASK, (100 * div) / 2 - 1) | REG_FIELD_PREP(PANEL_POWER_CYCLE_DELAY_MASK, DIV_ROUND_UP(seq->t11_t12, 1000)));
	else
		intel_de_rmw(dev_priv, regs.pp_ctrl, BXT_POWER_CYCLE_DELAY_MASK,
			     REG_FIELD_PREP(BXT_POWER_CYCLE_DELAY_MASK,
					    DIV_ROUND_UP(seq->t11_t12, 1000)));

	drm_dbg_kms(&dev_priv->drm,
		    "panel power sequencer register settings: PP_ON %#x, PP_OFF %#x, PP_DIV %#x\n",
		    intel_de_read(dev_priv, regs.pp_on),
		    intel_de_read(dev_priv, regs.pp_off),
		    i915_mmio_reg_valid(regs.pp_div) ?
		    intel_de_read(dev_priv, regs.pp_div) :
		    (intel_de_read(dev_priv, regs.pp_ctrl) & BXT_POWER_CYCLE_DELAY_MASK));
}

void intel_pps_encoder_reset(struct intel_dp *intel_dp)
{
	struct drm_i915_private *i915 = dp_to_i915(intel_dp);
	intel_wakeref_t wakeref;

	if (!intel_dp_is_edp(intel_dp))
		return;

	with_intel_pps_lock(intel_dp, wakeref) {
		 
		if (IS_VALLEYVIEW(i915) || IS_CHERRYVIEW(i915))
			vlv_initial_power_sequencer_setup(intel_dp);

		pps_init_delays(intel_dp);
		pps_init_registers(intel_dp, false);
		pps_vdd_init(intel_dp);

		if (edp_have_panel_vdd(intel_dp))
			edp_panel_vdd_schedule_off(intel_dp);
	}
}

bool intel_pps_init(struct intel_dp *intel_dp)
{
	intel_wakeref_t wakeref;
	bool ret;

	intel_dp->pps.initializing = true;
	INIT_DELAYED_WORK(&intel_dp->pps.panel_vdd_work, edp_panel_vdd_work);

	pps_init_timestamps(intel_dp);

	with_intel_pps_lock(intel_dp, wakeref) {
		ret = pps_initial_setup(intel_dp);

		pps_init_delays(intel_dp);
		pps_init_registers(intel_dp, false);
		pps_vdd_init(intel_dp);
	}

	return ret;
}

static void pps_init_late(struct intel_dp *intel_dp)
{
	struct drm_i915_private *i915 = dp_to_i915(intel_dp);
	struct intel_encoder *encoder = &dp_to_dig_port(intel_dp)->base;
	struct intel_connector *connector = intel_dp->attached_connector;

	if (IS_VALLEYVIEW(i915) || IS_CHERRYVIEW(i915))
		return;

	if (intel_num_pps(i915) < 2)
		return;

	drm_WARN(&i915->drm, connector->panel.vbt.backlight.controller >= 0 &&
		 intel_dp->pps.pps_idx != connector->panel.vbt.backlight.controller,
		 "[ENCODER:%d:%s] power sequencer mismatch: %d (initial) vs. %d (VBT)\n",
		 encoder->base.base.id, encoder->base.name,
		 intel_dp->pps.pps_idx, connector->panel.vbt.backlight.controller);

	if (connector->panel.vbt.backlight.controller >= 0)
		intel_dp->pps.pps_idx = connector->panel.vbt.backlight.controller;
}

void intel_pps_init_late(struct intel_dp *intel_dp)
{
	intel_wakeref_t wakeref;

	with_intel_pps_lock(intel_dp, wakeref) {
		 
		pps_init_late(intel_dp);

		memset(&intel_dp->pps.pps_delays, 0, sizeof(intel_dp->pps.pps_delays));
		pps_init_delays(intel_dp);
		pps_init_registers(intel_dp, false);

		intel_dp->pps.initializing = false;

		if (edp_have_panel_vdd(intel_dp))
			edp_panel_vdd_schedule_off(intel_dp);
	}
}

void intel_pps_unlock_regs_wa(struct drm_i915_private *dev_priv)
{
	int pps_num;
	int pps_idx;

	if (!HAS_DISPLAY(dev_priv) || HAS_DDI(dev_priv))
		return;
	 
	pps_num = intel_num_pps(dev_priv);

	for (pps_idx = 0; pps_idx < pps_num; pps_idx++)
		intel_de_rmw(dev_priv, PP_CONTROL(pps_idx),
			     PANEL_UNLOCK_MASK, PANEL_UNLOCK_REGS);
}

void intel_pps_setup(struct drm_i915_private *i915)
{
	if (HAS_PCH_SPLIT(i915) || IS_GEMINILAKE(i915) || IS_BROXTON(i915))
		i915->display.pps.mmio_base = PCH_PPS_BASE;
	else if (IS_VALLEYVIEW(i915) || IS_CHERRYVIEW(i915))
		i915->display.pps.mmio_base = VLV_PPS_BASE;
	else
		i915->display.pps.mmio_base = PPS_BASE;
}

void assert_pps_unlocked(struct drm_i915_private *dev_priv, enum pipe pipe)
{
	i915_reg_t pp_reg;
	u32 val;
	enum pipe panel_pipe = INVALID_PIPE;
	bool locked = true;

	if (drm_WARN_ON(&dev_priv->drm, HAS_DDI(dev_priv)))
		return;

	if (HAS_PCH_SPLIT(dev_priv)) {
		u32 port_sel;

		pp_reg = PP_CONTROL(0);
		port_sel = intel_de_read(dev_priv, PP_ON_DELAYS(0)) & PANEL_PORT_SELECT_MASK;

		switch (port_sel) {
		case PANEL_PORT_SELECT_LVDS:
			intel_lvds_port_enabled(dev_priv, PCH_LVDS, &panel_pipe);
			break;
		case PANEL_PORT_SELECT_DPA:
			g4x_dp_port_enabled(dev_priv, DP_A, PORT_A, &panel_pipe);
			break;
		case PANEL_PORT_SELECT_DPC:
			g4x_dp_port_enabled(dev_priv, PCH_DP_C, PORT_C, &panel_pipe);
			break;
		case PANEL_PORT_SELECT_DPD:
			g4x_dp_port_enabled(dev_priv, PCH_DP_D, PORT_D, &panel_pipe);
			break;
		default:
			MISSING_CASE(port_sel);
			break;
		}
	} else if (IS_VALLEYVIEW(dev_priv) || IS_CHERRYVIEW(dev_priv)) {
		 
		pp_reg = PP_CONTROL(pipe);
		panel_pipe = pipe;
	} else {
		u32 port_sel;

		pp_reg = PP_CONTROL(0);
		port_sel = intel_de_read(dev_priv, PP_ON_DELAYS(0)) & PANEL_PORT_SELECT_MASK;

		drm_WARN_ON(&dev_priv->drm,
			    port_sel != PANEL_PORT_SELECT_LVDS);
		intel_lvds_port_enabled(dev_priv, LVDS, &panel_pipe);
	}

	val = intel_de_read(dev_priv, pp_reg);
	if (!(val & PANEL_POWER_ON) ||
	    ((val & PANEL_UNLOCK_MASK) == PANEL_UNLOCK_REGS))
		locked = false;

	I915_STATE_WARN(dev_priv, panel_pipe == pipe && locked,
			"panel assertion failure, pipe %c regs locked\n",
			pipe_name(pipe));
}
