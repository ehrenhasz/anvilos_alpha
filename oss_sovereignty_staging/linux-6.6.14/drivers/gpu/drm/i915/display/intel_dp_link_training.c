 

#include "i915_drv.h"
#include "intel_display_types.h"
#include "intel_dp.h"
#include "intel_dp_link_training.h"

#define LT_MSG_PREFIX			"[CONNECTOR:%d:%s][ENCODER:%d:%s][%s] "
#define LT_MSG_ARGS(_intel_dp, _dp_phy)	(_intel_dp)->attached_connector->base.base.id, \
					(_intel_dp)->attached_connector->base.name, \
					dp_to_dig_port(_intel_dp)->base.base.base.id, \
					dp_to_dig_port(_intel_dp)->base.base.name, \
					drm_dp_phy_name(_dp_phy)

#define lt_dbg(_intel_dp, _dp_phy, _format, ...) \
	drm_dbg_kms(&dp_to_i915(_intel_dp)->drm, \
		    LT_MSG_PREFIX _format, \
		    LT_MSG_ARGS(_intel_dp, _dp_phy), ## __VA_ARGS__)

#define lt_err(_intel_dp, _dp_phy, _format, ...) do { \
	if (intel_digital_port_connected(&dp_to_dig_port(_intel_dp)->base)) \
		drm_err(&dp_to_i915(_intel_dp)->drm, \
			LT_MSG_PREFIX _format, \
			LT_MSG_ARGS(_intel_dp, _dp_phy), ## __VA_ARGS__); \
	else \
		lt_dbg(_intel_dp, _dp_phy, "Sink disconnected: " _format, ## __VA_ARGS__); \
} while (0)

static void intel_dp_reset_lttpr_common_caps(struct intel_dp *intel_dp)
{
	memset(intel_dp->lttpr_common_caps, 0, sizeof(intel_dp->lttpr_common_caps));
}

static void intel_dp_reset_lttpr_count(struct intel_dp *intel_dp)
{
	intel_dp->lttpr_common_caps[DP_PHY_REPEATER_CNT -
				    DP_LT_TUNABLE_PHY_REPEATER_FIELD_DATA_STRUCTURE_REV] = 0;
}

static u8 *intel_dp_lttpr_phy_caps(struct intel_dp *intel_dp,
				   enum drm_dp_phy dp_phy)
{
	return intel_dp->lttpr_phy_caps[dp_phy - DP_PHY_LTTPR1];
}

static void intel_dp_read_lttpr_phy_caps(struct intel_dp *intel_dp,
					 const u8 dpcd[DP_RECEIVER_CAP_SIZE],
					 enum drm_dp_phy dp_phy)
{
	u8 *phy_caps = intel_dp_lttpr_phy_caps(intel_dp, dp_phy);

	if (drm_dp_read_lttpr_phy_caps(&intel_dp->aux, dpcd, dp_phy, phy_caps) < 0) {
		lt_dbg(intel_dp, dp_phy, "failed to read the PHY caps\n");
		return;
	}

	lt_dbg(intel_dp, dp_phy, "PHY capabilities: %*ph\n",
	       (int)sizeof(intel_dp->lttpr_phy_caps[0]),
	       phy_caps);
}

static bool intel_dp_read_lttpr_common_caps(struct intel_dp *intel_dp,
					    const u8 dpcd[DP_RECEIVER_CAP_SIZE])
{
	int ret;

	ret = drm_dp_read_lttpr_common_caps(&intel_dp->aux, dpcd,
					    intel_dp->lttpr_common_caps);
	if (ret < 0)
		goto reset_caps;

	lt_dbg(intel_dp, DP_PHY_DPRX, "LTTPR common capabilities: %*ph\n",
	       (int)sizeof(intel_dp->lttpr_common_caps),
	       intel_dp->lttpr_common_caps);

	 
	if (intel_dp->lttpr_common_caps[0] < 0x14)
		goto reset_caps;

	return true;

reset_caps:
	intel_dp_reset_lttpr_common_caps(intel_dp);
	return false;
}

static bool
intel_dp_set_lttpr_transparent_mode(struct intel_dp *intel_dp, bool enable)
{
	u8 val = enable ? DP_PHY_REPEATER_MODE_TRANSPARENT :
			  DP_PHY_REPEATER_MODE_NON_TRANSPARENT;

	return drm_dp_dpcd_write(&intel_dp->aux, DP_PHY_REPEATER_MODE, &val, 1) == 1;
}

static int intel_dp_init_lttpr(struct intel_dp *intel_dp, const u8 dpcd[DP_RECEIVER_CAP_SIZE])
{
	int lttpr_count;
	int i;

	if (!intel_dp_read_lttpr_common_caps(intel_dp, dpcd))
		return 0;

	lttpr_count = drm_dp_lttpr_count(intel_dp->lttpr_common_caps);
	 
	if (lttpr_count == 0)
		return 0;

	 
	intel_dp_set_lttpr_transparent_mode(intel_dp, true);

	 
	if (lttpr_count < 0)
		return 0;

	if (!intel_dp_set_lttpr_transparent_mode(intel_dp, false)) {
		lt_dbg(intel_dp, DP_PHY_DPRX,
		       "Switching to LTTPR non-transparent LT mode failed, fall-back to transparent mode\n");

		intel_dp_set_lttpr_transparent_mode(intel_dp, true);
		intel_dp_reset_lttpr_count(intel_dp);

		return 0;
	}

	for (i = 0; i < lttpr_count; i++)
		intel_dp_read_lttpr_phy_caps(intel_dp, dpcd, DP_PHY_LTTPR(i));

	return lttpr_count;
}

 
int intel_dp_init_lttpr_and_dprx_caps(struct intel_dp *intel_dp)
{
	struct drm_i915_private *i915 = dp_to_i915(intel_dp);
	int lttpr_count = 0;

	 
	if (!intel_dp_is_edp(intel_dp) &&
	    (DISPLAY_VER(i915) >= 10 && !IS_GEMINILAKE(i915))) {
		u8 dpcd[DP_RECEIVER_CAP_SIZE];

		if (drm_dp_dpcd_probe(&intel_dp->aux, DP_LT_TUNABLE_PHY_REPEATER_FIELD_DATA_STRUCTURE_REV))
			return -EIO;

		if (drm_dp_read_dpcd_caps(&intel_dp->aux, dpcd))
			return -EIO;

		lttpr_count = intel_dp_init_lttpr(intel_dp, dpcd);
	}

	 
	if (drm_dp_read_dpcd_caps(&intel_dp->aux, intel_dp->dpcd)) {
		intel_dp_reset_lttpr_common_caps(intel_dp);
		return -EIO;
	}

	return lttpr_count;
}

static u8 dp_voltage_max(u8 preemph)
{
	switch (preemph & DP_TRAIN_PRE_EMPHASIS_MASK) {
	case DP_TRAIN_PRE_EMPH_LEVEL_0:
		return DP_TRAIN_VOLTAGE_SWING_LEVEL_3;
	case DP_TRAIN_PRE_EMPH_LEVEL_1:
		return DP_TRAIN_VOLTAGE_SWING_LEVEL_2;
	case DP_TRAIN_PRE_EMPH_LEVEL_2:
		return DP_TRAIN_VOLTAGE_SWING_LEVEL_1;
	case DP_TRAIN_PRE_EMPH_LEVEL_3:
	default:
		return DP_TRAIN_VOLTAGE_SWING_LEVEL_0;
	}
}

static u8 intel_dp_lttpr_voltage_max(struct intel_dp *intel_dp,
				     enum drm_dp_phy dp_phy)
{
	const u8 *phy_caps = intel_dp_lttpr_phy_caps(intel_dp, dp_phy);

	if (drm_dp_lttpr_voltage_swing_level_3_supported(phy_caps))
		return DP_TRAIN_VOLTAGE_SWING_LEVEL_3;
	else
		return DP_TRAIN_VOLTAGE_SWING_LEVEL_2;
}

static u8 intel_dp_lttpr_preemph_max(struct intel_dp *intel_dp,
				     enum drm_dp_phy dp_phy)
{
	const u8 *phy_caps = intel_dp_lttpr_phy_caps(intel_dp, dp_phy);

	if (drm_dp_lttpr_pre_emphasis_level_3_supported(phy_caps))
		return DP_TRAIN_PRE_EMPH_LEVEL_3;
	else
		return DP_TRAIN_PRE_EMPH_LEVEL_2;
}

static bool
intel_dp_phy_is_downstream_of_source(struct intel_dp *intel_dp,
				     enum drm_dp_phy dp_phy)
{
	struct drm_i915_private *i915 = dp_to_i915(intel_dp);
	int lttpr_count = drm_dp_lttpr_count(intel_dp->lttpr_common_caps);

	drm_WARN_ON_ONCE(&i915->drm, lttpr_count <= 0 && dp_phy != DP_PHY_DPRX);

	return lttpr_count <= 0 || dp_phy == DP_PHY_LTTPR(lttpr_count - 1);
}

static u8 intel_dp_phy_voltage_max(struct intel_dp *intel_dp,
				   const struct intel_crtc_state *crtc_state,
				   enum drm_dp_phy dp_phy)
{
	struct drm_i915_private *i915 = dp_to_i915(intel_dp);
	u8 voltage_max;

	 
	if (intel_dp_phy_is_downstream_of_source(intel_dp, dp_phy))
		voltage_max = intel_dp->voltage_max(intel_dp, crtc_state);
	else
		voltage_max = intel_dp_lttpr_voltage_max(intel_dp, dp_phy + 1);

	drm_WARN_ON_ONCE(&i915->drm,
			 voltage_max != DP_TRAIN_VOLTAGE_SWING_LEVEL_2 &&
			 voltage_max != DP_TRAIN_VOLTAGE_SWING_LEVEL_3);

	return voltage_max;
}

static u8 intel_dp_phy_preemph_max(struct intel_dp *intel_dp,
				   enum drm_dp_phy dp_phy)
{
	struct drm_i915_private *i915 = dp_to_i915(intel_dp);
	u8 preemph_max;

	 
	if (intel_dp_phy_is_downstream_of_source(intel_dp, dp_phy))
		preemph_max = intel_dp->preemph_max(intel_dp);
	else
		preemph_max = intel_dp_lttpr_preemph_max(intel_dp, dp_phy + 1);

	drm_WARN_ON_ONCE(&i915->drm,
			 preemph_max != DP_TRAIN_PRE_EMPH_LEVEL_2 &&
			 preemph_max != DP_TRAIN_PRE_EMPH_LEVEL_3);

	return preemph_max;
}

static bool has_per_lane_signal_levels(struct intel_dp *intel_dp,
				       enum drm_dp_phy dp_phy)
{
	struct drm_i915_private *i915 = dp_to_i915(intel_dp);

	return !intel_dp_phy_is_downstream_of_source(intel_dp, dp_phy) ||
		DISPLAY_VER(i915) >= 11;
}

 
static u8 intel_dp_get_lane_adjust_tx_ffe_preset(struct intel_dp *intel_dp,
						 const struct intel_crtc_state *crtc_state,
						 enum drm_dp_phy dp_phy,
						 const u8 link_status[DP_LINK_STATUS_SIZE],
						 int lane)
{
	u8 tx_ffe = 0;

	if (has_per_lane_signal_levels(intel_dp, dp_phy)) {
		lane = min(lane, crtc_state->lane_count - 1);
		tx_ffe = drm_dp_get_adjust_tx_ffe_preset(link_status, lane);
	} else {
		for (lane = 0; lane < crtc_state->lane_count; lane++)
			tx_ffe = max(tx_ffe, drm_dp_get_adjust_tx_ffe_preset(link_status, lane));
	}

	return tx_ffe;
}

 
static u8 intel_dp_get_lane_adjust_vswing_preemph(struct intel_dp *intel_dp,
						  const struct intel_crtc_state *crtc_state,
						  enum drm_dp_phy dp_phy,
						  const u8 link_status[DP_LINK_STATUS_SIZE],
						  int lane)
{
	u8 v = 0;
	u8 p = 0;
	u8 voltage_max;
	u8 preemph_max;

	if (has_per_lane_signal_levels(intel_dp, dp_phy)) {
		lane = min(lane, crtc_state->lane_count - 1);

		v = drm_dp_get_adjust_request_voltage(link_status, lane);
		p = drm_dp_get_adjust_request_pre_emphasis(link_status, lane);
	} else {
		for (lane = 0; lane < crtc_state->lane_count; lane++) {
			v = max(v, drm_dp_get_adjust_request_voltage(link_status, lane));
			p = max(p, drm_dp_get_adjust_request_pre_emphasis(link_status, lane));
		}
	}

	preemph_max = intel_dp_phy_preemph_max(intel_dp, dp_phy);
	if (p >= preemph_max)
		p = preemph_max | DP_TRAIN_MAX_PRE_EMPHASIS_REACHED;

	v = min(v, dp_voltage_max(p));

	voltage_max = intel_dp_phy_voltage_max(intel_dp, crtc_state, dp_phy);
	if (v >= voltage_max)
		v = voltage_max | DP_TRAIN_MAX_SWING_REACHED;

	return v | p;
}

static u8 intel_dp_get_lane_adjust_train(struct intel_dp *intel_dp,
					 const struct intel_crtc_state *crtc_state,
					 enum drm_dp_phy dp_phy,
					 const u8 link_status[DP_LINK_STATUS_SIZE],
					 int lane)
{
	if (intel_dp_is_uhbr(crtc_state))
		return intel_dp_get_lane_adjust_tx_ffe_preset(intel_dp, crtc_state,
							      dp_phy, link_status, lane);
	else
		return intel_dp_get_lane_adjust_vswing_preemph(intel_dp, crtc_state,
							       dp_phy, link_status, lane);
}

#define TRAIN_REQ_FMT "%d/%d/%d/%d"
#define _TRAIN_REQ_VSWING_ARGS(link_status, lane) \
	(drm_dp_get_adjust_request_voltage((link_status), (lane)) >> DP_TRAIN_VOLTAGE_SWING_SHIFT)
#define TRAIN_REQ_VSWING_ARGS(link_status) \
	_TRAIN_REQ_VSWING_ARGS(link_status, 0), \
	_TRAIN_REQ_VSWING_ARGS(link_status, 1), \
	_TRAIN_REQ_VSWING_ARGS(link_status, 2), \
	_TRAIN_REQ_VSWING_ARGS(link_status, 3)
#define _TRAIN_REQ_PREEMPH_ARGS(link_status, lane) \
	(drm_dp_get_adjust_request_pre_emphasis((link_status), (lane)) >> DP_TRAIN_PRE_EMPHASIS_SHIFT)
#define TRAIN_REQ_PREEMPH_ARGS(link_status) \
	_TRAIN_REQ_PREEMPH_ARGS(link_status, 0), \
	_TRAIN_REQ_PREEMPH_ARGS(link_status, 1), \
	_TRAIN_REQ_PREEMPH_ARGS(link_status, 2), \
	_TRAIN_REQ_PREEMPH_ARGS(link_status, 3)
#define _TRAIN_REQ_TX_FFE_ARGS(link_status, lane) \
	drm_dp_get_adjust_tx_ffe_preset((link_status), (lane))
#define TRAIN_REQ_TX_FFE_ARGS(link_status) \
	_TRAIN_REQ_TX_FFE_ARGS(link_status, 0), \
	_TRAIN_REQ_TX_FFE_ARGS(link_status, 1), \
	_TRAIN_REQ_TX_FFE_ARGS(link_status, 2), \
	_TRAIN_REQ_TX_FFE_ARGS(link_status, 3)

void
intel_dp_get_adjust_train(struct intel_dp *intel_dp,
			  const struct intel_crtc_state *crtc_state,
			  enum drm_dp_phy dp_phy,
			  const u8 link_status[DP_LINK_STATUS_SIZE])
{
	int lane;

	if (intel_dp_is_uhbr(crtc_state)) {
		lt_dbg(intel_dp, dp_phy,
		       "128b/132b, lanes: %d, "
		       "TX FFE request: " TRAIN_REQ_FMT "\n",
		       crtc_state->lane_count,
		       TRAIN_REQ_TX_FFE_ARGS(link_status));
	} else {
		lt_dbg(intel_dp, dp_phy,
		       "8b/10b, lanes: %d, "
		       "vswing request: " TRAIN_REQ_FMT ", "
		       "pre-emphasis request: " TRAIN_REQ_FMT "\n",
		       crtc_state->lane_count,
		       TRAIN_REQ_VSWING_ARGS(link_status),
		       TRAIN_REQ_PREEMPH_ARGS(link_status));
	}

	for (lane = 0; lane < 4; lane++)
		intel_dp->train_set[lane] =
			intel_dp_get_lane_adjust_train(intel_dp, crtc_state,
						       dp_phy, link_status, lane);
}

static int intel_dp_training_pattern_set_reg(struct intel_dp *intel_dp,
					     enum drm_dp_phy dp_phy)
{
	return dp_phy == DP_PHY_DPRX ?
		DP_TRAINING_PATTERN_SET :
		DP_TRAINING_PATTERN_SET_PHY_REPEATER(dp_phy);
}

static bool
intel_dp_set_link_train(struct intel_dp *intel_dp,
			const struct intel_crtc_state *crtc_state,
			enum drm_dp_phy dp_phy,
			u8 dp_train_pat)
{
	int reg = intel_dp_training_pattern_set_reg(intel_dp, dp_phy);
	u8 buf[sizeof(intel_dp->train_set) + 1];
	int len;

	intel_dp_program_link_training_pattern(intel_dp, crtc_state,
					       dp_phy, dp_train_pat);

	buf[0] = dp_train_pat;
	 
	memcpy(buf + 1, intel_dp->train_set, crtc_state->lane_count);
	len = crtc_state->lane_count + 1;

	return drm_dp_dpcd_write(&intel_dp->aux, reg, buf, len) == len;
}

static char dp_training_pattern_name(u8 train_pat)
{
	switch (train_pat) {
	case DP_TRAINING_PATTERN_1:
	case DP_TRAINING_PATTERN_2:
	case DP_TRAINING_PATTERN_3:
		return '0' + train_pat;
	case DP_TRAINING_PATTERN_4:
		return '4';
	default:
		MISSING_CASE(train_pat);
		return '?';
	}
}

void
intel_dp_program_link_training_pattern(struct intel_dp *intel_dp,
				       const struct intel_crtc_state *crtc_state,
				       enum drm_dp_phy dp_phy,
				       u8 dp_train_pat)
{
	u8 train_pat = intel_dp_training_pattern_symbol(dp_train_pat);

	if (train_pat != DP_TRAINING_PATTERN_DISABLE)
		lt_dbg(intel_dp, dp_phy, "Using DP training pattern TPS%c\n",
		       dp_training_pattern_name(train_pat));

	intel_dp->set_link_train(intel_dp, crtc_state, dp_train_pat);
}

#define TRAIN_SET_FMT "%d%s/%d%s/%d%s/%d%s"
#define _TRAIN_SET_VSWING_ARGS(train_set) \
	((train_set) & DP_TRAIN_VOLTAGE_SWING_MASK) >> DP_TRAIN_VOLTAGE_SWING_SHIFT, \
	(train_set) & DP_TRAIN_MAX_SWING_REACHED ? "(max)" : ""
#define TRAIN_SET_VSWING_ARGS(train_set) \
	_TRAIN_SET_VSWING_ARGS((train_set)[0]), \
	_TRAIN_SET_VSWING_ARGS((train_set)[1]), \
	_TRAIN_SET_VSWING_ARGS((train_set)[2]), \
	_TRAIN_SET_VSWING_ARGS((train_set)[3])
#define _TRAIN_SET_PREEMPH_ARGS(train_set) \
	((train_set) & DP_TRAIN_PRE_EMPHASIS_MASK) >> DP_TRAIN_PRE_EMPHASIS_SHIFT, \
	(train_set) & DP_TRAIN_MAX_PRE_EMPHASIS_REACHED ? "(max)" : ""
#define TRAIN_SET_PREEMPH_ARGS(train_set) \
	_TRAIN_SET_PREEMPH_ARGS((train_set)[0]), \
	_TRAIN_SET_PREEMPH_ARGS((train_set)[1]), \
	_TRAIN_SET_PREEMPH_ARGS((train_set)[2]), \
	_TRAIN_SET_PREEMPH_ARGS((train_set)[3])
#define _TRAIN_SET_TX_FFE_ARGS(train_set) \
	((train_set) & DP_TX_FFE_PRESET_VALUE_MASK), ""
#define TRAIN_SET_TX_FFE_ARGS(train_set) \
	_TRAIN_SET_TX_FFE_ARGS((train_set)[0]), \
	_TRAIN_SET_TX_FFE_ARGS((train_set)[1]), \
	_TRAIN_SET_TX_FFE_ARGS((train_set)[2]), \
	_TRAIN_SET_TX_FFE_ARGS((train_set)[3])

void intel_dp_set_signal_levels(struct intel_dp *intel_dp,
				const struct intel_crtc_state *crtc_state,
				enum drm_dp_phy dp_phy)
{
	struct intel_encoder *encoder = &dp_to_dig_port(intel_dp)->base;

	if (intel_dp_is_uhbr(crtc_state)) {
		lt_dbg(intel_dp, dp_phy,
		       "128b/132b, lanes: %d, "
		       "TX FFE presets: " TRAIN_SET_FMT "\n",
		       crtc_state->lane_count,
		       TRAIN_SET_TX_FFE_ARGS(intel_dp->train_set));
	} else {
		lt_dbg(intel_dp, dp_phy,
		       "8b/10b, lanes: %d, "
		       "vswing levels: " TRAIN_SET_FMT ", "
		       "pre-emphasis levels: " TRAIN_SET_FMT "\n",
		       crtc_state->lane_count,
		       TRAIN_SET_VSWING_ARGS(intel_dp->train_set),
		       TRAIN_SET_PREEMPH_ARGS(intel_dp->train_set));
	}

	if (intel_dp_phy_is_downstream_of_source(intel_dp, dp_phy))
		encoder->set_signal_levels(encoder, crtc_state);
}

static bool
intel_dp_reset_link_train(struct intel_dp *intel_dp,
			  const struct intel_crtc_state *crtc_state,
			  enum drm_dp_phy dp_phy,
			  u8 dp_train_pat)
{
	memset(intel_dp->train_set, 0, sizeof(intel_dp->train_set));
	intel_dp_set_signal_levels(intel_dp, crtc_state, dp_phy);
	return intel_dp_set_link_train(intel_dp, crtc_state, dp_phy, dp_train_pat);
}

static bool
intel_dp_update_link_train(struct intel_dp *intel_dp,
			   const struct intel_crtc_state *crtc_state,
			   enum drm_dp_phy dp_phy)
{
	int reg = dp_phy == DP_PHY_DPRX ?
			    DP_TRAINING_LANE0_SET :
			    DP_TRAINING_LANE0_SET_PHY_REPEATER(dp_phy);
	int ret;

	intel_dp_set_signal_levels(intel_dp, crtc_state, dp_phy);

	ret = drm_dp_dpcd_write(&intel_dp->aux, reg,
				intel_dp->train_set, crtc_state->lane_count);

	return ret == crtc_state->lane_count;
}

 
static bool intel_dp_lane_max_tx_ffe_reached(u8 train_set_lane)
{
	return (train_set_lane & DP_TX_FFE_PRESET_VALUE_MASK) ==
		DP_TX_FFE_PRESET_VALUE_MASK;
}

 
static bool intel_dp_lane_max_vswing_reached(u8 train_set_lane)
{
	u8 v = (train_set_lane & DP_TRAIN_VOLTAGE_SWING_MASK) >>
		DP_TRAIN_VOLTAGE_SWING_SHIFT;
	u8 p = (train_set_lane & DP_TRAIN_PRE_EMPHASIS_MASK) >>
		DP_TRAIN_PRE_EMPHASIS_SHIFT;

	if ((train_set_lane & DP_TRAIN_MAX_SWING_REACHED) == 0)
		return false;

	if (v + p != 3)
		return false;

	return true;
}

static bool intel_dp_link_max_vswing_reached(struct intel_dp *intel_dp,
					     const struct intel_crtc_state *crtc_state)
{
	int lane;

	for (lane = 0; lane < crtc_state->lane_count; lane++) {
		u8 train_set_lane = intel_dp->train_set[lane];

		if (intel_dp_is_uhbr(crtc_state)) {
			if (!intel_dp_lane_max_tx_ffe_reached(train_set_lane))
				return false;
		} else {
			if (!intel_dp_lane_max_vswing_reached(train_set_lane))
				return false;
		}
	}

	return true;
}

static void
intel_dp_update_downspread_ctrl(struct intel_dp *intel_dp,
				const struct intel_crtc_state *crtc_state)
{
	u8 link_config[2];

	link_config[0] = crtc_state->vrr.flipline ? DP_MSA_TIMING_PAR_IGNORE_EN : 0;
	link_config[1] = intel_dp_is_uhbr(crtc_state) ?
			 DP_SET_ANSI_128B132B : DP_SET_ANSI_8B10B;
	drm_dp_dpcd_write(&intel_dp->aux, DP_DOWNSPREAD_CTRL, link_config, 2);
}

static void
intel_dp_update_link_bw_set(struct intel_dp *intel_dp,
			    const struct intel_crtc_state *crtc_state,
			    u8 link_bw, u8 rate_select)
{
	u8 lane_count = crtc_state->lane_count;

	if (crtc_state->enhanced_framing)
		lane_count |= DP_LANE_COUNT_ENHANCED_FRAME_EN;

	if (link_bw) {
		 
		u8 link_config[] = { link_bw, lane_count };

		drm_dp_dpcd_write(&intel_dp->aux, DP_LINK_BW_SET, link_config,
				  ARRAY_SIZE(link_config));
	} else {
		 
		drm_dp_dpcd_writeb(&intel_dp->aux, DP_LANE_COUNT_SET, lane_count);
		drm_dp_dpcd_writeb(&intel_dp->aux, DP_LINK_RATE_SET, rate_select);
	}
}

 
static bool
intel_dp_prepare_link_train(struct intel_dp *intel_dp,
			    const struct intel_crtc_state *crtc_state)
{
	u8 link_bw, rate_select;

	if (intel_dp->prepare_link_retrain)
		intel_dp->prepare_link_retrain(intel_dp, crtc_state);

	intel_dp_compute_rate(intel_dp, crtc_state->port_clock,
			      &link_bw, &rate_select);

	 
	if (!link_bw) {
		__le16 sink_rates[DP_MAX_SUPPORTED_RATES];

		lt_dbg(intel_dp, DP_PHY_DPRX, "Reloading eDP link rates\n");

		drm_dp_dpcd_read(&intel_dp->aux, DP_SUPPORTED_LINK_RATES,
				 sink_rates, sizeof(sink_rates));
	}

	if (link_bw)
		lt_dbg(intel_dp, DP_PHY_DPRX, "Using LINK_BW_SET value %02x\n",
		       link_bw);
	else
		lt_dbg(intel_dp, DP_PHY_DPRX,
		       "Using LINK_RATE_SET value %02x\n",
		       rate_select);
	 
	intel_dp_update_downspread_ctrl(intel_dp, crtc_state);
	intel_dp_update_link_bw_set(intel_dp, crtc_state, link_bw,
				    rate_select);

	return true;
}

static bool intel_dp_adjust_request_changed(const struct intel_crtc_state *crtc_state,
					    const u8 old_link_status[DP_LINK_STATUS_SIZE],
					    const u8 new_link_status[DP_LINK_STATUS_SIZE])
{
	int lane;

	for (lane = 0; lane < crtc_state->lane_count; lane++) {
		u8 old, new;

		if (intel_dp_is_uhbr(crtc_state)) {
			old = drm_dp_get_adjust_tx_ffe_preset(old_link_status, lane);
			new = drm_dp_get_adjust_tx_ffe_preset(new_link_status, lane);
		} else {
			old = drm_dp_get_adjust_request_voltage(old_link_status, lane) |
				drm_dp_get_adjust_request_pre_emphasis(old_link_status, lane);
			new = drm_dp_get_adjust_request_voltage(new_link_status, lane) |
				drm_dp_get_adjust_request_pre_emphasis(new_link_status, lane);
		}

		if (old != new)
			return true;
	}

	return false;
}

void
intel_dp_dump_link_status(struct intel_dp *intel_dp, enum drm_dp_phy dp_phy,
			  const u8 link_status[DP_LINK_STATUS_SIZE])
{
	lt_dbg(intel_dp, dp_phy,
	       "ln0_1:0x%x ln2_3:0x%x align:0x%x sink:0x%x adj_req0_1:0x%x adj_req2_3:0x%x\n",
	       link_status[0], link_status[1], link_status[2],
	       link_status[3], link_status[4], link_status[5]);
}

 
static bool
intel_dp_link_training_clock_recovery(struct intel_dp *intel_dp,
				      const struct intel_crtc_state *crtc_state,
				      enum drm_dp_phy dp_phy)
{
	u8 old_link_status[DP_LINK_STATUS_SIZE] = {};
	int voltage_tries, cr_tries, max_cr_tries;
	u8 link_status[DP_LINK_STATUS_SIZE];
	bool max_vswing_reached = false;
	int delay_us;

	delay_us = drm_dp_read_clock_recovery_delay(&intel_dp->aux,
						    intel_dp->dpcd, dp_phy,
						    intel_dp_is_uhbr(crtc_state));

	 
	if (!intel_dp_reset_link_train(intel_dp, crtc_state, dp_phy,
				       DP_TRAINING_PATTERN_1 |
				       DP_LINK_SCRAMBLING_DISABLE)) {
		lt_err(intel_dp, dp_phy, "Failed to enable link training\n");
		return false;
	}

	 
	if (intel_dp->dpcd[DP_DPCD_REV] >= DP_DPCD_REV_14)
		max_cr_tries = 10;
	else
		max_cr_tries = 80;

	voltage_tries = 1;
	for (cr_tries = 0; cr_tries < max_cr_tries; ++cr_tries) {
		usleep_range(delay_us, 2 * delay_us);

		if (drm_dp_dpcd_read_phy_link_status(&intel_dp->aux, dp_phy,
						     link_status) < 0) {
			lt_err(intel_dp, dp_phy, "Failed to get link status\n");
			return false;
		}

		if (drm_dp_clock_recovery_ok(link_status, crtc_state->lane_count)) {
			lt_dbg(intel_dp, dp_phy, "Clock recovery OK\n");
			return true;
		}

		if (voltage_tries == 5) {
			intel_dp_dump_link_status(intel_dp, dp_phy, link_status);
			lt_dbg(intel_dp, dp_phy, "Same voltage tried 5 times\n");
			return false;
		}

		if (max_vswing_reached) {
			intel_dp_dump_link_status(intel_dp, dp_phy, link_status);
			lt_dbg(intel_dp, dp_phy, "Max Voltage Swing reached\n");
			return false;
		}

		 
		intel_dp_get_adjust_train(intel_dp, crtc_state, dp_phy,
					  link_status);
		if (!intel_dp_update_link_train(intel_dp, crtc_state, dp_phy)) {
			lt_err(intel_dp, dp_phy, "Failed to update link training\n");
			return false;
		}

		if (!intel_dp_adjust_request_changed(crtc_state, old_link_status, link_status))
			++voltage_tries;
		else
			voltage_tries = 1;

		memcpy(old_link_status, link_status, sizeof(link_status));

		if (intel_dp_link_max_vswing_reached(intel_dp, crtc_state))
			max_vswing_reached = true;
	}

	intel_dp_dump_link_status(intel_dp, dp_phy, link_status);
	lt_err(intel_dp, dp_phy, "Failed clock recovery %d times, giving up!\n",
	       max_cr_tries);

	return false;
}

 
static u32 intel_dp_training_pattern(struct intel_dp *intel_dp,
				     const struct intel_crtc_state *crtc_state,
				     enum drm_dp_phy dp_phy)
{
	struct drm_i915_private *i915 = dp_to_i915(intel_dp);
	bool source_tps3, sink_tps3, source_tps4, sink_tps4;

	 
	if (intel_dp_is_uhbr(crtc_state))
		return DP_TRAINING_PATTERN_2;

	 
	source_tps4 = intel_dp_source_supports_tps4(i915);
	sink_tps4 = dp_phy != DP_PHY_DPRX ||
		    drm_dp_tps4_supported(intel_dp->dpcd);
	if (source_tps4 && sink_tps4) {
		return DP_TRAINING_PATTERN_4;
	} else if (crtc_state->port_clock == 810000) {
		if (!source_tps4)
			lt_dbg(intel_dp, dp_phy,
			       "8.1 Gbps link rate without source TPS4 support\n");
		if (!sink_tps4)
			lt_dbg(intel_dp, dp_phy,
			       "8.1 Gbps link rate without sink TPS4 support\n");
	}

	 
	source_tps3 = intel_dp_source_supports_tps3(i915);
	sink_tps3 = dp_phy != DP_PHY_DPRX ||
		    drm_dp_tps3_supported(intel_dp->dpcd);
	if (source_tps3 && sink_tps3) {
		return  DP_TRAINING_PATTERN_3;
	} else if (crtc_state->port_clock >= 540000) {
		if (!source_tps3)
			lt_dbg(intel_dp, dp_phy,
			       ">=5.4/6.48 Gbps link rate without source TPS3 support\n");
		if (!sink_tps3)
			lt_dbg(intel_dp, dp_phy,
			       ">=5.4/6.48 Gbps link rate without sink TPS3 support\n");
	}

	return DP_TRAINING_PATTERN_2;
}

 
static bool
intel_dp_link_training_channel_equalization(struct intel_dp *intel_dp,
					    const struct intel_crtc_state *crtc_state,
					    enum drm_dp_phy dp_phy)
{
	int tries;
	u32 training_pattern;
	u8 link_status[DP_LINK_STATUS_SIZE];
	bool channel_eq = false;
	int delay_us;

	delay_us = drm_dp_read_channel_eq_delay(&intel_dp->aux,
						intel_dp->dpcd, dp_phy,
						intel_dp_is_uhbr(crtc_state));

	training_pattern = intel_dp_training_pattern(intel_dp, crtc_state, dp_phy);
	 
	if (training_pattern != DP_TRAINING_PATTERN_4)
		training_pattern |= DP_LINK_SCRAMBLING_DISABLE;

	 
	if (!intel_dp_set_link_train(intel_dp, crtc_state, dp_phy,
				     training_pattern)) {
		lt_err(intel_dp, dp_phy, "Failed to start channel equalization\n");
		return false;
	}

	for (tries = 0; tries < 5; tries++) {
		usleep_range(delay_us, 2 * delay_us);

		if (drm_dp_dpcd_read_phy_link_status(&intel_dp->aux, dp_phy,
						     link_status) < 0) {
			lt_err(intel_dp, dp_phy, "Failed to get link status\n");
			break;
		}

		 
		if (!drm_dp_clock_recovery_ok(link_status,
					      crtc_state->lane_count)) {
			intel_dp_dump_link_status(intel_dp, dp_phy, link_status);
			lt_dbg(intel_dp, dp_phy,
			       "Clock recovery check failed, cannot continue channel equalization\n");
			break;
		}

		if (drm_dp_channel_eq_ok(link_status,
					 crtc_state->lane_count)) {
			channel_eq = true;
			lt_dbg(intel_dp, dp_phy, "Channel EQ done. DP Training successful\n");
			break;
		}

		 
		intel_dp_get_adjust_train(intel_dp, crtc_state, dp_phy,
					  link_status);
		if (!intel_dp_update_link_train(intel_dp, crtc_state, dp_phy)) {
			lt_err(intel_dp, dp_phy, "Failed to update link training\n");
			break;
		}
	}

	 
	if (tries == 5) {
		intel_dp_dump_link_status(intel_dp, dp_phy, link_status);
		lt_dbg(intel_dp, dp_phy, "Channel equalization failed 5 times\n");
	}

	return channel_eq;
}

static bool intel_dp_disable_dpcd_training_pattern(struct intel_dp *intel_dp,
						   enum drm_dp_phy dp_phy)
{
	int reg = intel_dp_training_pattern_set_reg(intel_dp, dp_phy);
	u8 val = DP_TRAINING_PATTERN_DISABLE;

	return drm_dp_dpcd_write(&intel_dp->aux, reg, &val, 1) == 1;
}

static int
intel_dp_128b132b_intra_hop(struct intel_dp *intel_dp,
			    const struct intel_crtc_state *crtc_state)
{
	u8 sink_status;
	int ret;

	ret = drm_dp_dpcd_readb(&intel_dp->aux, DP_SINK_STATUS, &sink_status);
	if (ret != 1) {
		lt_dbg(intel_dp, DP_PHY_DPRX, "Failed to read sink status\n");
		return ret < 0 ? ret : -EIO;
	}

	return sink_status & DP_INTRA_HOP_AUX_REPLY_INDICATION ? 1 : 0;
}

 
void intel_dp_stop_link_train(struct intel_dp *intel_dp,
			      const struct intel_crtc_state *crtc_state)
{
	intel_dp->link_trained = true;

	intel_dp_disable_dpcd_training_pattern(intel_dp, DP_PHY_DPRX);
	intel_dp_program_link_training_pattern(intel_dp, crtc_state, DP_PHY_DPRX,
					       DP_TRAINING_PATTERN_DISABLE);

	if (intel_dp_is_uhbr(crtc_state) &&
	    wait_for(intel_dp_128b132b_intra_hop(intel_dp, crtc_state) == 0, 500)) {
		lt_dbg(intel_dp, DP_PHY_DPRX, "128b/132b intra-hop not clearing\n");
	}
}

static bool
intel_dp_link_train_phy(struct intel_dp *intel_dp,
			const struct intel_crtc_state *crtc_state,
			enum drm_dp_phy dp_phy)
{
	bool ret = false;

	if (!intel_dp_link_training_clock_recovery(intel_dp, crtc_state, dp_phy))
		goto out;

	if (!intel_dp_link_training_channel_equalization(intel_dp, crtc_state, dp_phy))
		goto out;

	ret = true;

out:
	lt_dbg(intel_dp, dp_phy,
	       "Link Training %s at link rate = %d, lane count = %d\n",
	       ret ? "passed" : "failed",
	       crtc_state->port_clock, crtc_state->lane_count);

	return ret;
}

static void intel_dp_schedule_fallback_link_training(struct intel_dp *intel_dp,
						     const struct intel_crtc_state *crtc_state)
{
	struct intel_connector *intel_connector = intel_dp->attached_connector;
	struct drm_i915_private *i915 = dp_to_i915(intel_dp);

	if (!intel_digital_port_connected(&dp_to_dig_port(intel_dp)->base)) {
		lt_dbg(intel_dp, DP_PHY_DPRX, "Link Training failed on disconnected sink.\n");
		return;
	}

	if (intel_dp->hobl_active) {
		lt_dbg(intel_dp, DP_PHY_DPRX,
		       "Link Training failed with HOBL active, not enabling it from now on\n");
		intel_dp->hobl_failed = true;
	} else if (intel_dp_get_link_train_fallback_values(intel_dp,
							   crtc_state->port_clock,
							   crtc_state->lane_count)) {
		return;
	}

	 
	queue_work(i915->unordered_wq, &intel_connector->modeset_retry_work);
}

 
static bool
intel_dp_link_train_all_phys(struct intel_dp *intel_dp,
			     const struct intel_crtc_state *crtc_state,
			     int lttpr_count)
{
	bool ret = true;
	int i;

	for (i = lttpr_count - 1; i >= 0; i--) {
		enum drm_dp_phy dp_phy = DP_PHY_LTTPR(i);

		ret = intel_dp_link_train_phy(intel_dp, crtc_state, dp_phy);
		intel_dp_disable_dpcd_training_pattern(intel_dp, dp_phy);

		if (!ret)
			break;
	}

	if (ret)
		ret = intel_dp_link_train_phy(intel_dp, crtc_state, DP_PHY_DPRX);

	if (intel_dp->set_idle_link_train)
		intel_dp->set_idle_link_train(intel_dp, crtc_state);

	return ret;
}

 
static bool
intel_dp_128b132b_lane_eq(struct intel_dp *intel_dp,
			  const struct intel_crtc_state *crtc_state)
{
	u8 link_status[DP_LINK_STATUS_SIZE];
	int delay_us;
	int try, max_tries = 20;
	unsigned long deadline;
	bool timeout = false;

	 
	if (!intel_dp_reset_link_train(intel_dp, crtc_state, DP_PHY_DPRX,
				       DP_TRAINING_PATTERN_1)) {
		lt_err(intel_dp, DP_PHY_DPRX, "Failed to start 128b/132b TPS1\n");
		return false;
	}

	delay_us = drm_dp_128b132b_read_aux_rd_interval(&intel_dp->aux);

	 
	if (drm_dp_dpcd_read_link_status(&intel_dp->aux, link_status) < 0) {
		lt_err(intel_dp, DP_PHY_DPRX, "Failed to read TX FFE presets\n");
		return false;
	}

	 
	intel_dp_get_adjust_train(intel_dp, crtc_state, DP_PHY_DPRX, link_status);
	if (!intel_dp_update_link_train(intel_dp, crtc_state, DP_PHY_DPRX)) {
		lt_err(intel_dp, DP_PHY_DPRX, "Failed to set initial TX FFE settings\n");
		return false;
	}

	 
	if (!intel_dp_set_link_train(intel_dp, crtc_state, DP_PHY_DPRX,
				     DP_TRAINING_PATTERN_2)) {
		lt_err(intel_dp, DP_PHY_DPRX, "Failed to start 128b/132b TPS2\n");
		return false;
	}

	 
	deadline = jiffies + msecs_to_jiffies_timeout(400);

	for (try = 0; try < max_tries; try++) {
		usleep_range(delay_us, 2 * delay_us);

		 
		delay_us = drm_dp_128b132b_read_aux_rd_interval(&intel_dp->aux);

		if (drm_dp_dpcd_read_link_status(&intel_dp->aux, link_status) < 0) {
			lt_err(intel_dp, DP_PHY_DPRX, "Failed to read link status\n");
			return false;
		}

		if (drm_dp_128b132b_link_training_failed(link_status)) {
			intel_dp_dump_link_status(intel_dp, DP_PHY_DPRX, link_status);
			lt_err(intel_dp, DP_PHY_DPRX,
			       "Downstream link training failure\n");
			return false;
		}

		if (drm_dp_128b132b_lane_channel_eq_done(link_status, crtc_state->lane_count)) {
			lt_dbg(intel_dp, DP_PHY_DPRX, "Lane channel eq done\n");
			break;
		}

		if (timeout) {
			intel_dp_dump_link_status(intel_dp, DP_PHY_DPRX, link_status);
			lt_err(intel_dp, DP_PHY_DPRX, "Lane channel eq timeout\n");
			return false;
		}

		if (time_after(jiffies, deadline))
			timeout = true;  

		 
		intel_dp_get_adjust_train(intel_dp, crtc_state, DP_PHY_DPRX, link_status);
		if (!intel_dp_update_link_train(intel_dp, crtc_state, DP_PHY_DPRX)) {
			lt_err(intel_dp, DP_PHY_DPRX, "Failed to update TX FFE settings\n");
			return false;
		}
	}

	if (try == max_tries) {
		intel_dp_dump_link_status(intel_dp, DP_PHY_DPRX, link_status);
		lt_err(intel_dp, DP_PHY_DPRX, "Max loop count reached\n");
		return false;
	}

	for (;;) {
		if (time_after(jiffies, deadline))
			timeout = true;  

		if (drm_dp_dpcd_read_link_status(&intel_dp->aux, link_status) < 0) {
			lt_err(intel_dp, DP_PHY_DPRX, "Failed to read link status\n");
			return false;
		}

		if (drm_dp_128b132b_link_training_failed(link_status)) {
			intel_dp_dump_link_status(intel_dp, DP_PHY_DPRX, link_status);
			lt_err(intel_dp, DP_PHY_DPRX, "Downstream link training failure\n");
			return false;
		}

		if (drm_dp_128b132b_eq_interlane_align_done(link_status)) {
			lt_dbg(intel_dp, DP_PHY_DPRX, "Interlane align done\n");
			break;
		}

		if (timeout) {
			intel_dp_dump_link_status(intel_dp, DP_PHY_DPRX, link_status);
			lt_err(intel_dp, DP_PHY_DPRX, "Interlane align timeout\n");
			return false;
		}

		usleep_range(2000, 3000);
	}

	return true;
}

 
static bool
intel_dp_128b132b_lane_cds(struct intel_dp *intel_dp,
			   const struct intel_crtc_state *crtc_state,
			   int lttpr_count)
{
	u8 link_status[DP_LINK_STATUS_SIZE];
	unsigned long deadline;

	if (drm_dp_dpcd_writeb(&intel_dp->aux, DP_TRAINING_PATTERN_SET,
			       DP_TRAINING_PATTERN_2_CDS) != 1) {
		lt_err(intel_dp, DP_PHY_DPRX, "Failed to start 128b/132b TPS2 CDS\n");
		return false;
	}

	 
	deadline = jiffies + msecs_to_jiffies_timeout((lttpr_count + 1) * 20);

	for (;;) {
		bool timeout = false;

		if (time_after(jiffies, deadline))
			timeout = true;  

		usleep_range(2000, 3000);

		if (drm_dp_dpcd_read_link_status(&intel_dp->aux, link_status) < 0) {
			lt_err(intel_dp, DP_PHY_DPRX, "Failed to read link status\n");
			return false;
		}

		if (drm_dp_128b132b_eq_interlane_align_done(link_status) &&
		    drm_dp_128b132b_cds_interlane_align_done(link_status) &&
		    drm_dp_128b132b_lane_symbol_locked(link_status, crtc_state->lane_count)) {
			lt_dbg(intel_dp, DP_PHY_DPRX, "CDS interlane align done\n");
			break;
		}

		if (drm_dp_128b132b_link_training_failed(link_status)) {
			intel_dp_dump_link_status(intel_dp, DP_PHY_DPRX, link_status);
			lt_err(intel_dp, DP_PHY_DPRX, "Downstream link training failure\n");
			return false;
		}

		if (timeout) {
			intel_dp_dump_link_status(intel_dp, DP_PHY_DPRX, link_status);
			lt_err(intel_dp, DP_PHY_DPRX, "CDS timeout\n");
			return false;
		}
	}

	return true;
}

 
static bool
intel_dp_128b132b_link_train(struct intel_dp *intel_dp,
			     const struct intel_crtc_state *crtc_state,
			     int lttpr_count)
{
	bool passed = false;

	if (wait_for(intel_dp_128b132b_intra_hop(intel_dp, crtc_state) == 0, 500)) {
		lt_err(intel_dp, DP_PHY_DPRX, "128b/132b intra-hop not clear\n");
		return false;
	}

	if (intel_dp_128b132b_lane_eq(intel_dp, crtc_state) &&
	    intel_dp_128b132b_lane_cds(intel_dp, crtc_state, lttpr_count))
		passed = true;

	lt_dbg(intel_dp, DP_PHY_DPRX,
	       "128b/132b Link Training %s at link rate = %d, lane count = %d\n",
	       passed ? "passed" : "failed",
	       crtc_state->port_clock, crtc_state->lane_count);

	return passed;
}

 
void intel_dp_start_link_train(struct intel_dp *intel_dp,
			       const struct intel_crtc_state *crtc_state)
{
	struct drm_i915_private *i915 = dp_to_i915(intel_dp);
	bool passed;

	 
	int lttpr_count = intel_dp_init_lttpr_and_dprx_caps(intel_dp);

	if (lttpr_count < 0)
		 
		lttpr_count = 0;

	intel_dp_prepare_link_train(intel_dp, crtc_state);

	if (intel_dp_is_uhbr(crtc_state))
		passed = intel_dp_128b132b_link_train(intel_dp, crtc_state, lttpr_count);
	else
		passed = intel_dp_link_train_all_phys(intel_dp, crtc_state, lttpr_count);

	 
	if (!passed && i915->display.hotplug.ignore_long_hpd) {
		lt_dbg(intel_dp, DP_PHY_DPRX, "Ignore the link failure\n");
		return;
	}

	if (!passed)
		intel_dp_schedule_fallback_link_training(intel_dp, crtc_state);
}

void intel_dp_128b132b_sdp_crc16(struct intel_dp *intel_dp,
				 const struct intel_crtc_state *crtc_state)
{
	 
	if (intel_dp_is_uhbr(crtc_state))
		 
		drm_dp_dpcd_writeb(&intel_dp->aux,
				   DP_SDP_ERROR_DETECTION_CONFIGURATION,
				   DP_SDP_CRC16_128B132B_EN);

	lt_dbg(intel_dp, DP_PHY_DPRX, "DP2.0 SDP CRC16 for 128b/132b enabled\n");
}
