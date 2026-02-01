 

#include <linux/export.h>
#include <linux/i2c.h>
#include <linux/notifier.h>
#include <linux/slab.h>
#include <linux/string_helpers.h>
#include <linux/timekeeping.h>
#include <linux/types.h>

#include <asm/byteorder.h>

#include <drm/display/drm_dp_helper.h>
#include <drm/display/drm_dsc_helper.h>
#include <drm/display/drm_hdmi_helper.h>
#include <drm/drm_atomic_helper.h>
#include <drm/drm_crtc.h>
#include <drm/drm_edid.h>
#include <drm/drm_probe_helper.h>

#include "g4x_dp.h"
#include "i915_drv.h"
#include "i915_irq.h"
#include "i915_reg.h"
#include "intel_atomic.h"
#include "intel_audio.h"
#include "intel_backlight.h"
#include "intel_combo_phy_regs.h"
#include "intel_connector.h"
#include "intel_crtc.h"
#include "intel_cx0_phy.h"
#include "intel_ddi.h"
#include "intel_de.h"
#include "intel_display_types.h"
#include "intel_dp.h"
#include "intel_dp_aux.h"
#include "intel_dp_hdcp.h"
#include "intel_dp_link_training.h"
#include "intel_dp_mst.h"
#include "intel_dpio_phy.h"
#include "intel_dpll.h"
#include "intel_fifo_underrun.h"
#include "intel_hdcp.h"
#include "intel_hdmi.h"
#include "intel_hotplug.h"
#include "intel_hotplug_irq.h"
#include "intel_lspcon.h"
#include "intel_lvds.h"
#include "intel_panel.h"
#include "intel_pch_display.h"
#include "intel_pps.h"
#include "intel_psr.h"
#include "intel_tc.h"
#include "intel_vdsc.h"
#include "intel_vrr.h"
#include "intel_crtc_state_dump.h"

 
#define DP_DSC_PEAK_PIXEL_RATE			2720000
#define DP_DSC_MAX_ENC_THROUGHPUT_0		340000
#define DP_DSC_MAX_ENC_THROUGHPUT_1		400000

 
#define DP_DSC_FEC_OVERHEAD_FACTOR		972261

 
#define INTEL_DP_RESOLUTION_SHIFT_MASK	0
#define INTEL_DP_RESOLUTION_PREFERRED	(1 << INTEL_DP_RESOLUTION_SHIFT_MASK)
#define INTEL_DP_RESOLUTION_STANDARD	(2 << INTEL_DP_RESOLUTION_SHIFT_MASK)
#define INTEL_DP_RESOLUTION_FAILSAFE	(3 << INTEL_DP_RESOLUTION_SHIFT_MASK)


 
static const u8 valid_dsc_bpp[] = {6, 8, 10, 12, 15};

 
static const u8 valid_dsc_slicecount[] = {1, 2, 4};

 
bool intel_dp_is_edp(struct intel_dp *intel_dp)
{
	struct intel_digital_port *dig_port = dp_to_dig_port(intel_dp);

	return dig_port->base.type == INTEL_OUTPUT_EDP;
}

static void intel_dp_unset_edid(struct intel_dp *intel_dp);

 
bool intel_dp_is_uhbr(const struct intel_crtc_state *crtc_state)
{
	return crtc_state->port_clock >= 1000000;
}

static void intel_dp_set_default_sink_rates(struct intel_dp *intel_dp)
{
	intel_dp->sink_rates[0] = 162000;
	intel_dp->num_sink_rates = 1;
}

 
static void intel_dp_set_dpcd_sink_rates(struct intel_dp *intel_dp)
{
	static const int dp_rates[] = {
		162000, 270000, 540000, 810000
	};
	int i, max_rate;
	int max_lttpr_rate;

	if (drm_dp_has_quirk(&intel_dp->desc, DP_DPCD_QUIRK_CAN_DO_MAX_LINK_RATE_3_24_GBPS)) {
		 
		static const int quirk_rates[] = { 162000, 270000, 324000 };

		memcpy(intel_dp->sink_rates, quirk_rates, sizeof(quirk_rates));
		intel_dp->num_sink_rates = ARRAY_SIZE(quirk_rates);

		return;
	}

	 
	max_rate = drm_dp_bw_code_to_link_rate(intel_dp->dpcd[DP_MAX_LINK_RATE]);
	max_lttpr_rate = drm_dp_lttpr_max_link_rate(intel_dp->lttpr_common_caps);
	if (max_lttpr_rate)
		max_rate = min(max_rate, max_lttpr_rate);

	for (i = 0; i < ARRAY_SIZE(dp_rates); i++) {
		if (dp_rates[i] > max_rate)
			break;
		intel_dp->sink_rates[i] = dp_rates[i];
	}

	 
	if (intel_dp->dpcd[DP_MAIN_LINK_CHANNEL_CODING] & DP_CAP_ANSI_128B132B) {
		u8 uhbr_rates = 0;

		BUILD_BUG_ON(ARRAY_SIZE(intel_dp->sink_rates) < ARRAY_SIZE(dp_rates) + 3);

		drm_dp_dpcd_readb(&intel_dp->aux,
				  DP_128B132B_SUPPORTED_LINK_RATES, &uhbr_rates);

		if (drm_dp_lttpr_count(intel_dp->lttpr_common_caps)) {
			 
			if (intel_dp->lttpr_common_caps[0] >= 0x20 &&
			    intel_dp->lttpr_common_caps[DP_MAIN_LINK_CHANNEL_CODING_PHY_REPEATER -
							DP_LT_TUNABLE_PHY_REPEATER_FIELD_DATA_STRUCTURE_REV] &
			    DP_PHY_REPEATER_128B132B_SUPPORTED) {
				 
				uhbr_rates &= intel_dp->lttpr_common_caps[DP_PHY_REPEATER_128B132B_RATES -
									  DP_LT_TUNABLE_PHY_REPEATER_FIELD_DATA_STRUCTURE_REV];
			} else {
				 
				uhbr_rates = 0;
			}
		}

		if (uhbr_rates & DP_UHBR10)
			intel_dp->sink_rates[i++] = 1000000;
		if (uhbr_rates & DP_UHBR13_5)
			intel_dp->sink_rates[i++] = 1350000;
		if (uhbr_rates & DP_UHBR20)
			intel_dp->sink_rates[i++] = 2000000;
	}

	intel_dp->num_sink_rates = i;
}

static void intel_dp_set_sink_rates(struct intel_dp *intel_dp)
{
	struct intel_connector *connector = intel_dp->attached_connector;
	struct intel_digital_port *intel_dig_port = dp_to_dig_port(intel_dp);
	struct intel_encoder *encoder = &intel_dig_port->base;

	intel_dp_set_dpcd_sink_rates(intel_dp);

	if (intel_dp->num_sink_rates)
		return;

	drm_err(&dp_to_i915(intel_dp)->drm,
		"[CONNECTOR:%d:%s][ENCODER:%d:%s] Invalid DPCD with no link rates, using defaults\n",
		connector->base.base.id, connector->base.name,
		encoder->base.base.id, encoder->base.name);

	intel_dp_set_default_sink_rates(intel_dp);
}

static void intel_dp_set_default_max_sink_lane_count(struct intel_dp *intel_dp)
{
	intel_dp->max_sink_lane_count = 1;
}

static void intel_dp_set_max_sink_lane_count(struct intel_dp *intel_dp)
{
	struct intel_connector *connector = intel_dp->attached_connector;
	struct intel_digital_port *intel_dig_port = dp_to_dig_port(intel_dp);
	struct intel_encoder *encoder = &intel_dig_port->base;

	intel_dp->max_sink_lane_count = drm_dp_max_lane_count(intel_dp->dpcd);

	switch (intel_dp->max_sink_lane_count) {
	case 1:
	case 2:
	case 4:
		return;
	}

	drm_err(&dp_to_i915(intel_dp)->drm,
		"[CONNECTOR:%d:%s][ENCODER:%d:%s] Invalid DPCD max lane count (%d), using default\n",
		connector->base.base.id, connector->base.name,
		encoder->base.base.id, encoder->base.name,
		intel_dp->max_sink_lane_count);

	intel_dp_set_default_max_sink_lane_count(intel_dp);
}

 
static int intel_dp_rate_limit_len(const int *rates, int len, int max_rate)
{
	int i;

	 
	for (i = 0; i < len; i++) {
		if (rates[len - i - 1] <= max_rate)
			return len - i;
	}

	return 0;
}

 
static int intel_dp_common_len_rate_limit(const struct intel_dp *intel_dp,
					  int max_rate)
{
	return intel_dp_rate_limit_len(intel_dp->common_rates,
				       intel_dp->num_common_rates, max_rate);
}

static int intel_dp_common_rate(struct intel_dp *intel_dp, int index)
{
	if (drm_WARN_ON(&dp_to_i915(intel_dp)->drm,
			index < 0 || index >= intel_dp->num_common_rates))
		return 162000;

	return intel_dp->common_rates[index];
}

 
static int intel_dp_max_common_rate(struct intel_dp *intel_dp)
{
	return intel_dp_common_rate(intel_dp, intel_dp->num_common_rates - 1);
}

static int intel_dp_max_source_lane_count(struct intel_digital_port *dig_port)
{
	int vbt_max_lanes = intel_bios_dp_max_lane_count(dig_port->base.devdata);
	int max_lanes = dig_port->max_lanes;

	if (vbt_max_lanes)
		max_lanes = min(max_lanes, vbt_max_lanes);

	return max_lanes;
}

 
static int intel_dp_max_common_lane_count(struct intel_dp *intel_dp)
{
	struct intel_digital_port *dig_port = dp_to_dig_port(intel_dp);
	int source_max = intel_dp_max_source_lane_count(dig_port);
	int sink_max = intel_dp->max_sink_lane_count;
	int fia_max = intel_tc_port_fia_max_lane_count(dig_port);
	int lttpr_max = drm_dp_lttpr_max_lane_count(intel_dp->lttpr_common_caps);

	if (lttpr_max)
		sink_max = min(sink_max, lttpr_max);

	return min3(source_max, sink_max, fia_max);
}

int intel_dp_max_lane_count(struct intel_dp *intel_dp)
{
	switch (intel_dp->max_link_lane_count) {
	case 1:
	case 2:
	case 4:
		return intel_dp->max_link_lane_count;
	default:
		MISSING_CASE(intel_dp->max_link_lane_count);
		return 1;
	}
}

 
int
intel_dp_link_required(int pixel_clock, int bpp)
{
	 
	return DIV_ROUND_UP(pixel_clock * bpp, 8);
}

 
int
intel_dp_max_data_rate(int max_link_rate, int max_lanes)
{
	if (max_link_rate >= 1000000) {
		 
		int max_link_rate_kbps = max_link_rate * 10;

		max_link_rate_kbps = DIV_ROUND_CLOSEST_ULL(mul_u32_u32(max_link_rate_kbps, 9671), 10000);
		max_link_rate = max_link_rate_kbps / 8;
	}

	 

	return max_link_rate * max_lanes;
}

bool intel_dp_can_bigjoiner(struct intel_dp *intel_dp)
{
	struct intel_digital_port *intel_dig_port = dp_to_dig_port(intel_dp);
	struct intel_encoder *encoder = &intel_dig_port->base;
	struct drm_i915_private *dev_priv = to_i915(encoder->base.dev);

	return DISPLAY_VER(dev_priv) >= 12 ||
		(DISPLAY_VER(dev_priv) == 11 &&
		 encoder->port != PORT_A);
}

static int dg2_max_source_rate(struct intel_dp *intel_dp)
{
	return intel_dp_is_edp(intel_dp) ? 810000 : 1350000;
}

static int icl_max_source_rate(struct intel_dp *intel_dp)
{
	struct intel_digital_port *dig_port = dp_to_dig_port(intel_dp);
	struct drm_i915_private *dev_priv = to_i915(dig_port->base.base.dev);
	enum phy phy = intel_port_to_phy(dev_priv, dig_port->base.port);

	if (intel_phy_is_combo(dev_priv, phy) && !intel_dp_is_edp(intel_dp))
		return 540000;

	return 810000;
}

static int ehl_max_source_rate(struct intel_dp *intel_dp)
{
	if (intel_dp_is_edp(intel_dp))
		return 540000;

	return 810000;
}

static int mtl_max_source_rate(struct intel_dp *intel_dp)
{
	struct intel_digital_port *dig_port = dp_to_dig_port(intel_dp);
	struct drm_i915_private *i915 = to_i915(dig_port->base.base.dev);
	enum phy phy = intel_port_to_phy(i915, dig_port->base.port);

	if (intel_is_c10phy(i915, phy))
		return 810000;

	return 2000000;
}

static int vbt_max_link_rate(struct intel_dp *intel_dp)
{
	struct intel_encoder *encoder = &dp_to_dig_port(intel_dp)->base;
	int max_rate;

	max_rate = intel_bios_dp_max_link_rate(encoder->devdata);

	if (intel_dp_is_edp(intel_dp)) {
		struct intel_connector *connector = intel_dp->attached_connector;
		int edp_max_rate = connector->panel.vbt.edp.max_link_rate;

		if (max_rate && edp_max_rate)
			max_rate = min(max_rate, edp_max_rate);
		else if (edp_max_rate)
			max_rate = edp_max_rate;
	}

	return max_rate;
}

static void
intel_dp_set_source_rates(struct intel_dp *intel_dp)
{
	 
	static const int mtl_rates[] = {
		162000, 216000, 243000, 270000, 324000, 432000, 540000, 675000,
		810000,	1000000, 1350000, 2000000,
	};
	static const int icl_rates[] = {
		162000, 216000, 270000, 324000, 432000, 540000, 648000, 810000,
		1000000, 1350000,
	};
	static const int bxt_rates[] = {
		162000, 216000, 243000, 270000, 324000, 432000, 540000
	};
	static const int skl_rates[] = {
		162000, 216000, 270000, 324000, 432000, 540000
	};
	static const int hsw_rates[] = {
		162000, 270000, 540000
	};
	static const int g4x_rates[] = {
		162000, 270000
	};
	struct intel_digital_port *dig_port = dp_to_dig_port(intel_dp);
	struct drm_i915_private *dev_priv = to_i915(dig_port->base.base.dev);
	const int *source_rates;
	int size, max_rate = 0, vbt_max_rate;

	 
	drm_WARN_ON(&dev_priv->drm,
		    intel_dp->source_rates || intel_dp->num_source_rates);

	if (DISPLAY_VER(dev_priv) >= 14) {
		source_rates = mtl_rates;
		size = ARRAY_SIZE(mtl_rates);
		max_rate = mtl_max_source_rate(intel_dp);
	} else if (DISPLAY_VER(dev_priv) >= 11) {
		source_rates = icl_rates;
		size = ARRAY_SIZE(icl_rates);
		if (IS_DG2(dev_priv))
			max_rate = dg2_max_source_rate(intel_dp);
		else if (IS_ALDERLAKE_P(dev_priv) || IS_ALDERLAKE_S(dev_priv) ||
			 IS_DG1(dev_priv) || IS_ROCKETLAKE(dev_priv))
			max_rate = 810000;
		else if (IS_JASPERLAKE(dev_priv) || IS_ELKHARTLAKE(dev_priv))
			max_rate = ehl_max_source_rate(intel_dp);
		else
			max_rate = icl_max_source_rate(intel_dp);
	} else if (IS_GEMINILAKE(dev_priv) || IS_BROXTON(dev_priv)) {
		source_rates = bxt_rates;
		size = ARRAY_SIZE(bxt_rates);
	} else if (DISPLAY_VER(dev_priv) == 9) {
		source_rates = skl_rates;
		size = ARRAY_SIZE(skl_rates);
	} else if ((IS_HASWELL(dev_priv) && !IS_HASWELL_ULX(dev_priv)) ||
		   IS_BROADWELL(dev_priv)) {
		source_rates = hsw_rates;
		size = ARRAY_SIZE(hsw_rates);
	} else {
		source_rates = g4x_rates;
		size = ARRAY_SIZE(g4x_rates);
	}

	vbt_max_rate = vbt_max_link_rate(intel_dp);
	if (max_rate && vbt_max_rate)
		max_rate = min(max_rate, vbt_max_rate);
	else if (vbt_max_rate)
		max_rate = vbt_max_rate;

	if (max_rate)
		size = intel_dp_rate_limit_len(source_rates, size, max_rate);

	intel_dp->source_rates = source_rates;
	intel_dp->num_source_rates = size;
}

static int intersect_rates(const int *source_rates, int source_len,
			   const int *sink_rates, int sink_len,
			   int *common_rates)
{
	int i = 0, j = 0, k = 0;

	while (i < source_len && j < sink_len) {
		if (source_rates[i] == sink_rates[j]) {
			if (WARN_ON(k >= DP_MAX_SUPPORTED_RATES))
				return k;
			common_rates[k] = source_rates[i];
			++k;
			++i;
			++j;
		} else if (source_rates[i] < sink_rates[j]) {
			++i;
		} else {
			++j;
		}
	}
	return k;
}

 
static int intel_dp_rate_index(const int *rates, int len, int rate)
{
	int i;

	for (i = 0; i < len; i++)
		if (rate == rates[i])
			return i;

	return -1;
}

static void intel_dp_set_common_rates(struct intel_dp *intel_dp)
{
	struct drm_i915_private *i915 = dp_to_i915(intel_dp);

	drm_WARN_ON(&i915->drm,
		    !intel_dp->num_source_rates || !intel_dp->num_sink_rates);

	intel_dp->num_common_rates = intersect_rates(intel_dp->source_rates,
						     intel_dp->num_source_rates,
						     intel_dp->sink_rates,
						     intel_dp->num_sink_rates,
						     intel_dp->common_rates);

	 
	if (drm_WARN_ON(&i915->drm, intel_dp->num_common_rates == 0)) {
		intel_dp->common_rates[0] = 162000;
		intel_dp->num_common_rates = 1;
	}
}

static bool intel_dp_link_params_valid(struct intel_dp *intel_dp, int link_rate,
				       u8 lane_count)
{
	 
	if (link_rate == 0 ||
	    link_rate > intel_dp->max_link_rate)
		return false;

	if (lane_count == 0 ||
	    lane_count > intel_dp_max_lane_count(intel_dp))
		return false;

	return true;
}

static bool intel_dp_can_link_train_fallback_for_edp(struct intel_dp *intel_dp,
						     int link_rate,
						     u8 lane_count)
{
	 
	const struct drm_display_mode *fixed_mode =
		intel_panel_preferred_fixed_mode(intel_dp->attached_connector);
	int mode_rate, max_rate;

	mode_rate = intel_dp_link_required(fixed_mode->clock, 18);
	max_rate = intel_dp_max_data_rate(link_rate, lane_count);
	if (mode_rate > max_rate)
		return false;

	return true;
}

int intel_dp_get_link_train_fallback_values(struct intel_dp *intel_dp,
					    int link_rate, u8 lane_count)
{
	struct drm_i915_private *i915 = dp_to_i915(intel_dp);
	int index;

	 
	if (intel_dp->is_mst) {
		drm_err(&i915->drm, "Link Training Unsuccessful\n");
		return -1;
	}

	if (intel_dp_is_edp(intel_dp) && !intel_dp->use_max_params) {
		drm_dbg_kms(&i915->drm,
			    "Retrying Link training for eDP with max parameters\n");
		intel_dp->use_max_params = true;
		return 0;
	}

	index = intel_dp_rate_index(intel_dp->common_rates,
				    intel_dp->num_common_rates,
				    link_rate);
	if (index > 0) {
		if (intel_dp_is_edp(intel_dp) &&
		    !intel_dp_can_link_train_fallback_for_edp(intel_dp,
							      intel_dp_common_rate(intel_dp, index - 1),
							      lane_count)) {
			drm_dbg_kms(&i915->drm,
				    "Retrying Link training for eDP with same parameters\n");
			return 0;
		}
		intel_dp->max_link_rate = intel_dp_common_rate(intel_dp, index - 1);
		intel_dp->max_link_lane_count = lane_count;
	} else if (lane_count > 1) {
		if (intel_dp_is_edp(intel_dp) &&
		    !intel_dp_can_link_train_fallback_for_edp(intel_dp,
							      intel_dp_max_common_rate(intel_dp),
							      lane_count >> 1)) {
			drm_dbg_kms(&i915->drm,
				    "Retrying Link training for eDP with same parameters\n");
			return 0;
		}
		intel_dp->max_link_rate = intel_dp_max_common_rate(intel_dp);
		intel_dp->max_link_lane_count = lane_count >> 1;
	} else {
		drm_err(&i915->drm, "Link Training Unsuccessful\n");
		return -1;
	}

	return 0;
}

u32 intel_dp_mode_to_fec_clock(u32 mode_clock)
{
	return div_u64(mul_u32_u32(mode_clock, 1000000U),
		       DP_DSC_FEC_OVERHEAD_FACTOR);
}

static int
small_joiner_ram_size_bits(struct drm_i915_private *i915)
{
	if (DISPLAY_VER(i915) >= 13)
		return 17280 * 8;
	else if (DISPLAY_VER(i915) >= 11)
		return 7680 * 8;
	else
		return 6144 * 8;
}

u32 intel_dp_dsc_nearest_valid_bpp(struct drm_i915_private *i915, u32 bpp, u32 pipe_bpp)
{
	u32 bits_per_pixel = bpp;
	int i;

	 
	if (bits_per_pixel < valid_dsc_bpp[0]) {
		drm_dbg_kms(&i915->drm, "Unsupported BPP %u, min %u\n",
			    bits_per_pixel, valid_dsc_bpp[0]);
		return 0;
	}

	 
	if (DISPLAY_VER(i915) >= 13) {
		bits_per_pixel = min(bits_per_pixel, pipe_bpp - 1);

		 
		if (bits_per_pixel < 8) {
			drm_dbg_kms(&i915->drm, "Unsupported BPP %u, min 8\n",
				    bits_per_pixel);
			return 0;
		}
		bits_per_pixel = min_t(u32, bits_per_pixel, 27);
	} else {
		 
		for (i = 0; i < ARRAY_SIZE(valid_dsc_bpp) - 1; i++) {
			if (bits_per_pixel < valid_dsc_bpp[i + 1])
				break;
		}
		drm_dbg_kms(&i915->drm, "Set dsc bpp from %d to VESA %d\n",
			    bits_per_pixel, valid_dsc_bpp[i]);

		bits_per_pixel = valid_dsc_bpp[i];
	}

	return bits_per_pixel;
}

u16 intel_dp_dsc_get_output_bpp(struct drm_i915_private *i915,
				u32 link_clock, u32 lane_count,
				u32 mode_clock, u32 mode_hdisplay,
				bool bigjoiner,
				u32 pipe_bpp,
				u32 timeslots)
{
	u32 bits_per_pixel, max_bpp_small_joiner_ram;

	 
	bits_per_pixel = ((link_clock * lane_count) * timeslots) /
			 (intel_dp_mode_to_fec_clock(mode_clock) * 8);

	drm_dbg_kms(&i915->drm, "Max link bpp is %u for %u timeslots "
				"total bw %u pixel clock %u\n",
				bits_per_pixel, timeslots,
				(link_clock * lane_count * 8),
				intel_dp_mode_to_fec_clock(mode_clock));

	 
	max_bpp_small_joiner_ram = small_joiner_ram_size_bits(i915) /
		mode_hdisplay;

	if (bigjoiner)
		max_bpp_small_joiner_ram *= 2;

	 
	bits_per_pixel = min(bits_per_pixel, max_bpp_small_joiner_ram);

	if (bigjoiner) {
		u32 max_bpp_bigjoiner =
			i915->display.cdclk.max_cdclk_freq * 48 /
			intel_dp_mode_to_fec_clock(mode_clock);

		bits_per_pixel = min(bits_per_pixel, max_bpp_bigjoiner);
	}

	bits_per_pixel = intel_dp_dsc_nearest_valid_bpp(i915, bits_per_pixel, pipe_bpp);

	 
	return bits_per_pixel << 4;
}

u8 intel_dp_dsc_get_slice_count(struct intel_dp *intel_dp,
				int mode_clock, int mode_hdisplay,
				bool bigjoiner)
{
	struct drm_i915_private *i915 = dp_to_i915(intel_dp);
	u8 min_slice_count, i;
	int max_slice_width;

	if (mode_clock <= DP_DSC_PEAK_PIXEL_RATE)
		min_slice_count = DIV_ROUND_UP(mode_clock,
					       DP_DSC_MAX_ENC_THROUGHPUT_0);
	else
		min_slice_count = DIV_ROUND_UP(mode_clock,
					       DP_DSC_MAX_ENC_THROUGHPUT_1);

	 
	if (mode_clock >= ((i915->display.cdclk.max_cdclk_freq * 85) / 100))
		min_slice_count = max_t(u8, min_slice_count, 2);

	max_slice_width = drm_dp_dsc_sink_max_slice_width(intel_dp->dsc_dpcd);
	if (max_slice_width < DP_DSC_MIN_SLICE_WIDTH_VALUE) {
		drm_dbg_kms(&i915->drm,
			    "Unsupported slice width %d by DP DSC Sink device\n",
			    max_slice_width);
		return 0;
	}
	 
	min_slice_count = max_t(u8, min_slice_count,
				DIV_ROUND_UP(mode_hdisplay,
					     max_slice_width));

	 
	for (i = 0; i < ARRAY_SIZE(valid_dsc_slicecount); i++) {
		u8 test_slice_count = valid_dsc_slicecount[i] << bigjoiner;

		if (test_slice_count >
		    drm_dp_dsc_sink_max_slice_count(intel_dp->dsc_dpcd, false))
			break;

		 
		if (bigjoiner && test_slice_count < 4)
			continue;

		if (min_slice_count <= test_slice_count)
			return test_slice_count;
	}

	drm_dbg_kms(&i915->drm, "Unsupported Slice Count %d\n",
		    min_slice_count);
	return 0;
}

static bool source_can_output(struct intel_dp *intel_dp,
			      enum intel_output_format format)
{
	struct drm_i915_private *i915 = dp_to_i915(intel_dp);

	switch (format) {
	case INTEL_OUTPUT_FORMAT_RGB:
		return true;

	case INTEL_OUTPUT_FORMAT_YCBCR444:
		 
		return !HAS_GMCH(i915) && !IS_IRONLAKE(i915);

	case INTEL_OUTPUT_FORMAT_YCBCR420:
		 
		return DISPLAY_VER(i915) >= 11;

	default:
		MISSING_CASE(format);
		return false;
	}
}

static bool
dfp_can_convert_from_rgb(struct intel_dp *intel_dp,
			 enum intel_output_format sink_format)
{
	if (!drm_dp_is_branch(intel_dp->dpcd))
		return false;

	if (sink_format == INTEL_OUTPUT_FORMAT_YCBCR444)
		return intel_dp->dfp.rgb_to_ycbcr;

	if (sink_format == INTEL_OUTPUT_FORMAT_YCBCR420)
		return intel_dp->dfp.rgb_to_ycbcr &&
			intel_dp->dfp.ycbcr_444_to_420;

	return false;
}

static bool
dfp_can_convert_from_ycbcr444(struct intel_dp *intel_dp,
			      enum intel_output_format sink_format)
{
	if (!drm_dp_is_branch(intel_dp->dpcd))
		return false;

	if (sink_format == INTEL_OUTPUT_FORMAT_YCBCR420)
		return intel_dp->dfp.ycbcr_444_to_420;

	return false;
}

static enum intel_output_format
intel_dp_output_format(struct intel_connector *connector,
		       enum intel_output_format sink_format)
{
	struct intel_dp *intel_dp = intel_attached_dp(connector);
	struct drm_i915_private *i915 = dp_to_i915(intel_dp);
	enum intel_output_format output_format;

	if (intel_dp->force_dsc_output_format)
		return intel_dp->force_dsc_output_format;

	if (sink_format == INTEL_OUTPUT_FORMAT_RGB ||
	    dfp_can_convert_from_rgb(intel_dp, sink_format))
		output_format = INTEL_OUTPUT_FORMAT_RGB;

	else if (sink_format == INTEL_OUTPUT_FORMAT_YCBCR444 ||
		 dfp_can_convert_from_ycbcr444(intel_dp, sink_format))
		output_format = INTEL_OUTPUT_FORMAT_YCBCR444;

	else
		output_format = INTEL_OUTPUT_FORMAT_YCBCR420;

	drm_WARN_ON(&i915->drm, !source_can_output(intel_dp, output_format));

	return output_format;
}

int intel_dp_min_bpp(enum intel_output_format output_format)
{
	if (output_format == INTEL_OUTPUT_FORMAT_RGB)
		return 6 * 3;
	else
		return 8 * 3;
}

static int intel_dp_output_bpp(enum intel_output_format output_format, int bpp)
{
	 
	if (output_format == INTEL_OUTPUT_FORMAT_YCBCR420)
		bpp /= 2;

	return bpp;
}

static enum intel_output_format
intel_dp_sink_format(struct intel_connector *connector,
		     const struct drm_display_mode *mode)
{
	const struct drm_display_info *info = &connector->base.display_info;

	if (drm_mode_is_420_only(info, mode))
		return INTEL_OUTPUT_FORMAT_YCBCR420;

	return INTEL_OUTPUT_FORMAT_RGB;
}

static int
intel_dp_mode_min_output_bpp(struct intel_connector *connector,
			     const struct drm_display_mode *mode)
{
	enum intel_output_format output_format, sink_format;

	sink_format = intel_dp_sink_format(connector, mode);

	output_format = intel_dp_output_format(connector, sink_format);

	return intel_dp_output_bpp(output_format, intel_dp_min_bpp(output_format));
}

static bool intel_dp_hdisplay_bad(struct drm_i915_private *dev_priv,
				  int hdisplay)
{
	 
	return hdisplay == 4096 && !HAS_DDI(dev_priv);
}

static int intel_dp_max_tmds_clock(struct intel_dp *intel_dp)
{
	struct intel_connector *connector = intel_dp->attached_connector;
	const struct drm_display_info *info = &connector->base.display_info;
	int max_tmds_clock = intel_dp->dfp.max_tmds_clock;

	 
	if (max_tmds_clock && info->max_tmds_clock)
		max_tmds_clock = min(max_tmds_clock, info->max_tmds_clock);

	return max_tmds_clock;
}

static enum drm_mode_status
intel_dp_tmds_clock_valid(struct intel_dp *intel_dp,
			  int clock, int bpc,
			  enum intel_output_format sink_format,
			  bool respect_downstream_limits)
{
	int tmds_clock, min_tmds_clock, max_tmds_clock;

	if (!respect_downstream_limits)
		return MODE_OK;

	tmds_clock = intel_hdmi_tmds_clock(clock, bpc, sink_format);

	min_tmds_clock = intel_dp->dfp.min_tmds_clock;
	max_tmds_clock = intel_dp_max_tmds_clock(intel_dp);

	if (min_tmds_clock && tmds_clock < min_tmds_clock)
		return MODE_CLOCK_LOW;

	if (max_tmds_clock && tmds_clock > max_tmds_clock)
		return MODE_CLOCK_HIGH;

	return MODE_OK;
}

static enum drm_mode_status
intel_dp_mode_valid_downstream(struct intel_connector *connector,
			       const struct drm_display_mode *mode,
			       int target_clock)
{
	struct intel_dp *intel_dp = intel_attached_dp(connector);
	const struct drm_display_info *info = &connector->base.display_info;
	enum drm_mode_status status;
	enum intel_output_format sink_format;

	 
	if (intel_dp->dfp.pcon_max_frl_bw) {
		int target_bw;
		int max_frl_bw;
		int bpp = intel_dp_mode_min_output_bpp(connector, mode);

		target_bw = bpp * target_clock;

		max_frl_bw = intel_dp->dfp.pcon_max_frl_bw;

		 
		max_frl_bw = max_frl_bw * 1000000;

		if (target_bw > max_frl_bw)
			return MODE_CLOCK_HIGH;

		return MODE_OK;
	}

	if (intel_dp->dfp.max_dotclock &&
	    target_clock > intel_dp->dfp.max_dotclock)
		return MODE_CLOCK_HIGH;

	sink_format = intel_dp_sink_format(connector, mode);

	 
	status = intel_dp_tmds_clock_valid(intel_dp, target_clock,
					   8, sink_format, true);

	if (status != MODE_OK) {
		if (sink_format == INTEL_OUTPUT_FORMAT_YCBCR420 ||
		    !connector->base.ycbcr_420_allowed ||
		    !drm_mode_is_420_also(info, mode))
			return status;
		sink_format = INTEL_OUTPUT_FORMAT_YCBCR420;
		status = intel_dp_tmds_clock_valid(intel_dp, target_clock,
						   8, sink_format, true);
		if (status != MODE_OK)
			return status;
	}

	return MODE_OK;
}

bool intel_dp_need_bigjoiner(struct intel_dp *intel_dp,
			     int hdisplay, int clock)
{
	struct drm_i915_private *i915 = dp_to_i915(intel_dp);

	if (!intel_dp_can_bigjoiner(intel_dp))
		return false;

	return clock > i915->max_dotclk_freq || hdisplay > 5120;
}

static enum drm_mode_status
intel_dp_mode_valid(struct drm_connector *_connector,
		    struct drm_display_mode *mode)
{
	struct intel_connector *connector = to_intel_connector(_connector);
	struct intel_dp *intel_dp = intel_attached_dp(connector);
	struct drm_i915_private *dev_priv = to_i915(connector->base.dev);
	const struct drm_display_mode *fixed_mode;
	int target_clock = mode->clock;
	int max_rate, mode_rate, max_lanes, max_link_clock;
	int max_dotclk = dev_priv->max_dotclk_freq;
	u16 dsc_max_output_bpp = 0;
	u8 dsc_slice_count = 0;
	enum drm_mode_status status;
	bool dsc = false, bigjoiner = false;

	status = intel_cpu_transcoder_mode_valid(dev_priv, mode);
	if (status != MODE_OK)
		return status;

	if (mode->flags & DRM_MODE_FLAG_DBLCLK)
		return MODE_H_ILLEGAL;

	fixed_mode = intel_panel_fixed_mode(connector, mode);
	if (intel_dp_is_edp(intel_dp) && fixed_mode) {
		status = intel_panel_mode_valid(connector, mode);
		if (status != MODE_OK)
			return status;

		target_clock = fixed_mode->clock;
	}

	if (mode->clock < 10000)
		return MODE_CLOCK_LOW;

	if (intel_dp_need_bigjoiner(intel_dp, mode->hdisplay, target_clock)) {
		bigjoiner = true;
		max_dotclk *= 2;
	}
	if (target_clock > max_dotclk)
		return MODE_CLOCK_HIGH;

	if (intel_dp_hdisplay_bad(dev_priv, mode->hdisplay))
		return MODE_H_ILLEGAL;

	max_link_clock = intel_dp_max_link_rate(intel_dp);
	max_lanes = intel_dp_max_lane_count(intel_dp);

	max_rate = intel_dp_max_data_rate(max_link_clock, max_lanes);
	mode_rate = intel_dp_link_required(target_clock,
					   intel_dp_mode_min_output_bpp(connector, mode));

	if (HAS_DSC(dev_priv) &&
	    drm_dp_sink_supports_dsc(intel_dp->dsc_dpcd)) {
		 
		int pipe_bpp = intel_dp_dsc_compute_bpp(intel_dp, U8_MAX);

		 
		if (intel_dp_is_edp(intel_dp)) {
			dsc_max_output_bpp =
				drm_edp_dsc_sink_output_bpp(intel_dp->dsc_dpcd) >> 4;
			dsc_slice_count =
				drm_dp_dsc_sink_max_slice_count(intel_dp->dsc_dpcd,
								true);
		} else if (drm_dp_sink_supports_fec(intel_dp->fec_capable)) {
			dsc_max_output_bpp =
				intel_dp_dsc_get_output_bpp(dev_priv,
							    max_link_clock,
							    max_lanes,
							    target_clock,
							    mode->hdisplay,
							    bigjoiner,
							    pipe_bpp, 64) >> 4;
			dsc_slice_count =
				intel_dp_dsc_get_slice_count(intel_dp,
							     target_clock,
							     mode->hdisplay,
							     bigjoiner);
		}

		dsc = dsc_max_output_bpp && dsc_slice_count;
	}

	 
	if (DISPLAY_VER(dev_priv) < 13 && bigjoiner && !dsc)
		return MODE_CLOCK_HIGH;

	if (mode_rate > max_rate && !dsc)
		return MODE_CLOCK_HIGH;

	status = intel_dp_mode_valid_downstream(connector, mode, target_clock);
	if (status != MODE_OK)
		return status;

	return intel_mode_valid_max_plane_size(dev_priv, mode, bigjoiner);
}

bool intel_dp_source_supports_tps3(struct drm_i915_private *i915)
{
	return DISPLAY_VER(i915) >= 9 || IS_BROADWELL(i915) || IS_HASWELL(i915);
}

bool intel_dp_source_supports_tps4(struct drm_i915_private *i915)
{
	return DISPLAY_VER(i915) >= 10;
}

static void snprintf_int_array(char *str, size_t len,
			       const int *array, int nelem)
{
	int i;

	str[0] = '\0';

	for (i = 0; i < nelem; i++) {
		int r = snprintf(str, len, "%s%d", i ? ", " : "", array[i]);
		if (r >= len)
			return;
		str += r;
		len -= r;
	}
}

static void intel_dp_print_rates(struct intel_dp *intel_dp)
{
	struct drm_i915_private *i915 = dp_to_i915(intel_dp);
	char str[128];  

	if (!drm_debug_enabled(DRM_UT_KMS))
		return;

	snprintf_int_array(str, sizeof(str),
			   intel_dp->source_rates, intel_dp->num_source_rates);
	drm_dbg_kms(&i915->drm, "source rates: %s\n", str);

	snprintf_int_array(str, sizeof(str),
			   intel_dp->sink_rates, intel_dp->num_sink_rates);
	drm_dbg_kms(&i915->drm, "sink rates: %s\n", str);

	snprintf_int_array(str, sizeof(str),
			   intel_dp->common_rates, intel_dp->num_common_rates);
	drm_dbg_kms(&i915->drm, "common rates: %s\n", str);
}

int
intel_dp_max_link_rate(struct intel_dp *intel_dp)
{
	int len;

	len = intel_dp_common_len_rate_limit(intel_dp, intel_dp->max_link_rate);

	return intel_dp_common_rate(intel_dp, len - 1);
}

int intel_dp_rate_select(struct intel_dp *intel_dp, int rate)
{
	struct drm_i915_private *i915 = dp_to_i915(intel_dp);
	int i = intel_dp_rate_index(intel_dp->sink_rates,
				    intel_dp->num_sink_rates, rate);

	if (drm_WARN_ON(&i915->drm, i < 0))
		i = 0;

	return i;
}

void intel_dp_compute_rate(struct intel_dp *intel_dp, int port_clock,
			   u8 *link_bw, u8 *rate_select)
{
	 
	if (intel_dp->use_rate_select) {
		*link_bw = 0;
		*rate_select =
			intel_dp_rate_select(intel_dp, port_clock);
	} else {
		*link_bw = drm_dp_link_rate_to_bw_code(port_clock);
		*rate_select = 0;
	}
}

bool intel_dp_has_hdmi_sink(struct intel_dp *intel_dp)
{
	struct intel_connector *connector = intel_dp->attached_connector;

	return connector->base.display_info.is_hdmi;
}

static bool intel_dp_source_supports_fec(struct intel_dp *intel_dp,
					 const struct intel_crtc_state *pipe_config)
{
	struct drm_i915_private *dev_priv = dp_to_i915(intel_dp);

	 
	if (DISPLAY_VER(dev_priv) >= 12)
		return true;

	if (DISPLAY_VER(dev_priv) == 11 && pipe_config->cpu_transcoder != TRANSCODER_A)
		return true;

	return false;
}

static bool intel_dp_supports_fec(struct intel_dp *intel_dp,
				  const struct intel_crtc_state *pipe_config)
{
	return intel_dp_source_supports_fec(intel_dp, pipe_config) &&
		drm_dp_sink_supports_fec(intel_dp->fec_capable);
}

static bool intel_dp_supports_dsc(struct intel_dp *intel_dp,
				  const struct intel_crtc_state *crtc_state)
{
	if (intel_crtc_has_type(crtc_state, INTEL_OUTPUT_DP) && !crtc_state->fec_enable)
		return false;

	return intel_dsc_source_support(crtc_state) &&
		drm_dp_sink_supports_dsc(intel_dp->dsc_dpcd);
}

static int intel_dp_hdmi_compute_bpc(struct intel_dp *intel_dp,
				     const struct intel_crtc_state *crtc_state,
				     int bpc, bool respect_downstream_limits)
{
	int clock = crtc_state->hw.adjusted_mode.crtc_clock;

	 
	bpc = max(bpc, 8);

	 
	if (!respect_downstream_limits)
		bpc = 8;

	for (; bpc >= 8; bpc -= 2) {
		if (intel_hdmi_bpc_possible(crtc_state, bpc,
					    intel_dp_has_hdmi_sink(intel_dp)) &&
		    intel_dp_tmds_clock_valid(intel_dp, clock, bpc, crtc_state->sink_format,
					      respect_downstream_limits) == MODE_OK)
			return bpc;
	}

	return -EINVAL;
}

static int intel_dp_max_bpp(struct intel_dp *intel_dp,
			    const struct intel_crtc_state *crtc_state,
			    bool respect_downstream_limits)
{
	struct drm_i915_private *dev_priv = dp_to_i915(intel_dp);
	struct intel_connector *intel_connector = intel_dp->attached_connector;
	int bpp, bpc;

	bpc = crtc_state->pipe_bpp / 3;

	if (intel_dp->dfp.max_bpc)
		bpc = min_t(int, bpc, intel_dp->dfp.max_bpc);

	if (intel_dp->dfp.min_tmds_clock) {
		int max_hdmi_bpc;

		max_hdmi_bpc = intel_dp_hdmi_compute_bpc(intel_dp, crtc_state, bpc,
							 respect_downstream_limits);
		if (max_hdmi_bpc < 0)
			return 0;

		bpc = min(bpc, max_hdmi_bpc);
	}

	bpp = bpc * 3;
	if (intel_dp_is_edp(intel_dp)) {
		 
		if (intel_connector->base.display_info.bpc == 0 &&
		    intel_connector->panel.vbt.edp.bpp &&
		    intel_connector->panel.vbt.edp.bpp < bpp) {
			drm_dbg_kms(&dev_priv->drm,
				    "clamping bpp for eDP panel to BIOS-provided %i\n",
				    intel_connector->panel.vbt.edp.bpp);
			bpp = intel_connector->panel.vbt.edp.bpp;
		}
	}

	return bpp;
}

 
void
intel_dp_adjust_compliance_config(struct intel_dp *intel_dp,
				  struct intel_crtc_state *pipe_config,
				  struct link_config_limits *limits)
{
	struct drm_i915_private *i915 = dp_to_i915(intel_dp);

	 
	if (intel_dp->compliance.test_data.bpc != 0) {
		int bpp = 3 * intel_dp->compliance.test_data.bpc;

		limits->min_bpp = limits->max_bpp = bpp;
		pipe_config->dither_force_disable = bpp == 6 * 3;

		drm_dbg_kms(&i915->drm, "Setting pipe_bpp to %d\n", bpp);
	}

	 
	if (intel_dp->compliance.test_type == DP_TEST_LINK_TRAINING) {
		int index;

		 
		if (intel_dp_link_params_valid(intel_dp, intel_dp->compliance.test_link_rate,
					       intel_dp->compliance.test_lane_count)) {
			index = intel_dp_rate_index(intel_dp->common_rates,
						    intel_dp->num_common_rates,
						    intel_dp->compliance.test_link_rate);
			if (index >= 0)
				limits->min_rate = limits->max_rate =
					intel_dp->compliance.test_link_rate;
			limits->min_lane_count = limits->max_lane_count =
				intel_dp->compliance.test_lane_count;
		}
	}
}

static bool has_seamless_m_n(struct intel_connector *connector)
{
	struct drm_i915_private *i915 = to_i915(connector->base.dev);

	 
	return HAS_DOUBLE_BUFFERED_M_N(i915) &&
		intel_panel_drrs_type(connector) == DRRS_TYPE_SEAMLESS;
}

static int intel_dp_mode_clock(const struct intel_crtc_state *crtc_state,
			       const struct drm_connector_state *conn_state)
{
	struct intel_connector *connector = to_intel_connector(conn_state->connector);
	const struct drm_display_mode *adjusted_mode = &crtc_state->hw.adjusted_mode;

	 
	if (has_seamless_m_n(connector))
		return intel_panel_highest_mode(connector, adjusted_mode)->clock;
	else
		return adjusted_mode->crtc_clock;
}

 
static int
intel_dp_compute_link_config_wide(struct intel_dp *intel_dp,
				  struct intel_crtc_state *pipe_config,
				  const struct drm_connector_state *conn_state,
				  const struct link_config_limits *limits)
{
	int bpp, i, lane_count, clock = intel_dp_mode_clock(pipe_config, conn_state);
	int mode_rate, link_rate, link_avail;

	for (bpp = limits->max_bpp; bpp >= limits->min_bpp; bpp -= 2 * 3) {
		int output_bpp = intel_dp_output_bpp(pipe_config->output_format, bpp);

		mode_rate = intel_dp_link_required(clock, output_bpp);

		for (i = 0; i < intel_dp->num_common_rates; i++) {
			link_rate = intel_dp_common_rate(intel_dp, i);
			if (link_rate < limits->min_rate ||
			    link_rate > limits->max_rate)
				continue;

			for (lane_count = limits->min_lane_count;
			     lane_count <= limits->max_lane_count;
			     lane_count <<= 1) {
				link_avail = intel_dp_max_data_rate(link_rate,
								    lane_count);

				if (mode_rate <= link_avail) {
					pipe_config->lane_count = lane_count;
					pipe_config->pipe_bpp = bpp;
					pipe_config->port_clock = link_rate;

					return 0;
				}
			}
		}
	}

	return -EINVAL;
}

int intel_dp_dsc_compute_bpp(struct intel_dp *intel_dp, u8 max_req_bpc)
{
	struct drm_i915_private *i915 = dp_to_i915(intel_dp);
	int i, num_bpc;
	u8 dsc_bpc[3] = {0};
	u8 dsc_max_bpc;

	 
	if (DISPLAY_VER(i915) >= 12)
		dsc_max_bpc = min_t(u8, 12, max_req_bpc);
	else
		dsc_max_bpc = min_t(u8, 10, max_req_bpc);

	num_bpc = drm_dp_dsc_sink_supported_input_bpcs(intel_dp->dsc_dpcd,
						       dsc_bpc);
	for (i = 0; i < num_bpc; i++) {
		if (dsc_max_bpc >= dsc_bpc[i])
			return dsc_bpc[i] * 3;
	}

	return 0;
}

static int intel_dp_source_dsc_version_minor(struct intel_dp *intel_dp)
{
	struct drm_i915_private *i915 = dp_to_i915(intel_dp);

	return DISPLAY_VER(i915) >= 14 ? 2 : 1;
}

static int intel_dp_sink_dsc_version_minor(struct intel_dp *intel_dp)
{
	return (intel_dp->dsc_dpcd[DP_DSC_REV - DP_DSC_SUPPORT] & DP_DSC_MINOR_MASK) >>
		DP_DSC_MINOR_SHIFT;
}

static int intel_dp_get_slice_height(int vactive)
{
	int slice_height;

	 
	for (slice_height = 108; slice_height <= vactive; slice_height += 2)
		if (vactive % slice_height == 0)
			return slice_height;

	 
	return 2;
}

static int intel_dp_dsc_compute_params(struct intel_encoder *encoder,
				       struct intel_crtc_state *crtc_state)
{
	struct drm_i915_private *i915 = to_i915(encoder->base.dev);
	struct intel_dp *intel_dp = enc_to_intel_dp(encoder);
	struct drm_dsc_config *vdsc_cfg = &crtc_state->dsc.config;
	u8 line_buf_depth;
	int ret;

	 
	vdsc_cfg->rc_model_size = DSC_RC_MODEL_SIZE_CONST;
	vdsc_cfg->pic_height = crtc_state->hw.adjusted_mode.crtc_vdisplay;

	vdsc_cfg->slice_height = intel_dp_get_slice_height(vdsc_cfg->pic_height);

	ret = intel_dsc_compute_params(crtc_state);
	if (ret)
		return ret;

	vdsc_cfg->dsc_version_major =
		(intel_dp->dsc_dpcd[DP_DSC_REV - DP_DSC_SUPPORT] &
		 DP_DSC_MAJOR_MASK) >> DP_DSC_MAJOR_SHIFT;
	vdsc_cfg->dsc_version_minor =
		min(intel_dp_source_dsc_version_minor(intel_dp),
		    intel_dp_sink_dsc_version_minor(intel_dp));
	if (vdsc_cfg->convert_rgb)
		vdsc_cfg->convert_rgb =
			intel_dp->dsc_dpcd[DP_DSC_DEC_COLOR_FORMAT_CAP - DP_DSC_SUPPORT] &
			DP_DSC_RGB;

	line_buf_depth = drm_dp_dsc_sink_line_buf_depth(intel_dp->dsc_dpcd);
	if (!line_buf_depth) {
		drm_dbg_kms(&i915->drm,
			    "DSC Sink Line Buffer Depth invalid\n");
		return -EINVAL;
	}

	if (vdsc_cfg->dsc_version_minor == 2)
		vdsc_cfg->line_buf_depth = (line_buf_depth == DSC_1_2_MAX_LINEBUF_DEPTH_BITS) ?
			DSC_1_2_MAX_LINEBUF_DEPTH_VAL : line_buf_depth;
	else
		vdsc_cfg->line_buf_depth = (line_buf_depth > DSC_1_1_MAX_LINEBUF_DEPTH_BITS) ?
			DSC_1_1_MAX_LINEBUF_DEPTH_BITS : line_buf_depth;

	vdsc_cfg->block_pred_enable =
		intel_dp->dsc_dpcd[DP_DSC_BLK_PREDICTION_SUPPORT - DP_DSC_SUPPORT] &
		DP_DSC_BLK_PREDICTION_IS_SUPPORTED;

	return drm_dsc_compute_rc_parameters(vdsc_cfg);
}

static bool intel_dp_dsc_supports_format(struct intel_dp *intel_dp,
					 enum intel_output_format output_format)
{
	u8 sink_dsc_format;

	switch (output_format) {
	case INTEL_OUTPUT_FORMAT_RGB:
		sink_dsc_format = DP_DSC_RGB;
		break;
	case INTEL_OUTPUT_FORMAT_YCBCR444:
		sink_dsc_format = DP_DSC_YCbCr444;
		break;
	case INTEL_OUTPUT_FORMAT_YCBCR420:
		if (min(intel_dp_source_dsc_version_minor(intel_dp),
			intel_dp_sink_dsc_version_minor(intel_dp)) < 2)
			return false;
		sink_dsc_format = DP_DSC_YCbCr420_Native;
		break;
	default:
		return false;
	}

	return drm_dp_dsc_sink_supports_format(intel_dp->dsc_dpcd, sink_dsc_format);
}

int intel_dp_dsc_compute_config(struct intel_dp *intel_dp,
				struct intel_crtc_state *pipe_config,
				struct drm_connector_state *conn_state,
				struct link_config_limits *limits,
				int timeslots,
				bool compute_pipe_bpp)
{
	struct intel_digital_port *dig_port = dp_to_dig_port(intel_dp);
	struct drm_i915_private *dev_priv = to_i915(dig_port->base.base.dev);
	const struct drm_display_mode *adjusted_mode =
		&pipe_config->hw.adjusted_mode;
	int pipe_bpp;
	int ret;

	pipe_config->fec_enable = !intel_dp_is_edp(intel_dp) &&
		intel_dp_supports_fec(intel_dp, pipe_config);

	if (!intel_dp_supports_dsc(intel_dp, pipe_config))
		return -EINVAL;

	if (!intel_dp_dsc_supports_format(intel_dp, pipe_config->output_format))
		return -EINVAL;

	if (compute_pipe_bpp)
		pipe_bpp = intel_dp_dsc_compute_bpp(intel_dp, conn_state->max_requested_bpc);
	else
		pipe_bpp = pipe_config->pipe_bpp;

	if (intel_dp->force_dsc_bpc) {
		pipe_bpp = intel_dp->force_dsc_bpc * 3;
		drm_dbg_kms(&dev_priv->drm, "Input DSC BPP forced to %d", pipe_bpp);
	}

	 
	if (pipe_bpp < 8 * 3) {
		drm_dbg_kms(&dev_priv->drm,
			    "No DSC support for less than 8bpc\n");
		return -EINVAL;
	}

	 
	pipe_config->pipe_bpp = pipe_bpp;
	pipe_config->port_clock = limits->max_rate;
	pipe_config->lane_count = limits->max_lane_count;

	if (intel_dp_is_edp(intel_dp)) {
		pipe_config->dsc.compressed_bpp =
			min_t(u16, drm_edp_dsc_sink_output_bpp(intel_dp->dsc_dpcd) >> 4,
			      pipe_config->pipe_bpp);
		pipe_config->dsc.slice_count =
			drm_dp_dsc_sink_max_slice_count(intel_dp->dsc_dpcd,
							true);
		if (!pipe_config->dsc.slice_count) {
			drm_dbg_kms(&dev_priv->drm, "Unsupported Slice Count %d\n",
				    pipe_config->dsc.slice_count);
			return -EINVAL;
		}
	} else {
		u16 dsc_max_output_bpp = 0;
		u8 dsc_dp_slice_count;

		if (compute_pipe_bpp) {
			dsc_max_output_bpp =
				intel_dp_dsc_get_output_bpp(dev_priv,
							    pipe_config->port_clock,
							    pipe_config->lane_count,
							    adjusted_mode->crtc_clock,
							    adjusted_mode->crtc_hdisplay,
							    pipe_config->bigjoiner_pipes,
							    pipe_bpp,
							    timeslots);
			 
			if (pipe_config->output_format == INTEL_OUTPUT_FORMAT_YCBCR420)
				dsc_max_output_bpp = min_t(u16, dsc_max_output_bpp, 31 << 4);

			if (!dsc_max_output_bpp) {
				drm_dbg_kms(&dev_priv->drm,
					    "Compressed BPP not supported\n");
				return -EINVAL;
			}
		}
		dsc_dp_slice_count =
			intel_dp_dsc_get_slice_count(intel_dp,
						     adjusted_mode->crtc_clock,
						     adjusted_mode->crtc_hdisplay,
						     pipe_config->bigjoiner_pipes);
		if (!dsc_dp_slice_count) {
			drm_dbg_kms(&dev_priv->drm,
				    "Compressed Slice Count not supported\n");
			return -EINVAL;
		}

		 
		if (compute_pipe_bpp) {
			pipe_config->dsc.compressed_bpp = min_t(u16,
								dsc_max_output_bpp >> 4,
								pipe_config->pipe_bpp);
		}
		pipe_config->dsc.slice_count = dsc_dp_slice_count;
		drm_dbg_kms(&dev_priv->drm, "DSC: compressed bpp %d slice count %d\n",
			    pipe_config->dsc.compressed_bpp,
			    pipe_config->dsc.slice_count);
	}
	 
	if (pipe_config->bigjoiner_pipes || pipe_config->dsc.slice_count > 1)
		pipe_config->dsc.dsc_split = true;

	ret = intel_dp_dsc_compute_params(&dig_port->base, pipe_config);
	if (ret < 0) {
		drm_dbg_kms(&dev_priv->drm,
			    "Cannot compute valid DSC parameters for Input Bpp = %d "
			    "Compressed BPP = %d\n",
			    pipe_config->pipe_bpp,
			    pipe_config->dsc.compressed_bpp);
		return ret;
	}

	pipe_config->dsc.compression_enable = true;
	drm_dbg_kms(&dev_priv->drm, "DP DSC computed with Input Bpp = %d "
		    "Compressed Bpp = %d Slice Count = %d\n",
		    pipe_config->pipe_bpp,
		    pipe_config->dsc.compressed_bpp,
		    pipe_config->dsc.slice_count);

	return 0;
}

static int
intel_dp_compute_link_config(struct intel_encoder *encoder,
			     struct intel_crtc_state *pipe_config,
			     struct drm_connector_state *conn_state,
			     bool respect_downstream_limits)
{
	struct drm_i915_private *i915 = to_i915(encoder->base.dev);
	struct intel_crtc *crtc = to_intel_crtc(pipe_config->uapi.crtc);
	const struct drm_display_mode *adjusted_mode =
		&pipe_config->hw.adjusted_mode;
	struct intel_dp *intel_dp = enc_to_intel_dp(encoder);
	struct link_config_limits limits;
	bool joiner_needs_dsc = false;
	int ret;

	limits.min_rate = intel_dp_common_rate(intel_dp, 0);
	limits.max_rate = intel_dp_max_link_rate(intel_dp);

	limits.min_lane_count = 1;
	limits.max_lane_count = intel_dp_max_lane_count(intel_dp);

	limits.min_bpp = intel_dp_min_bpp(pipe_config->output_format);
	limits.max_bpp = intel_dp_max_bpp(intel_dp, pipe_config, respect_downstream_limits);

	if (intel_dp->use_max_params) {
		 
		limits.min_lane_count = limits.max_lane_count;
		limits.min_rate = limits.max_rate;
	}

	intel_dp_adjust_compliance_config(intel_dp, pipe_config, &limits);

	drm_dbg_kms(&i915->drm, "DP link computation with max lane count %i "
		    "max rate %d max bpp %d pixel clock %iKHz\n",
		    limits.max_lane_count, limits.max_rate,
		    limits.max_bpp, adjusted_mode->crtc_clock);

	if (intel_dp_need_bigjoiner(intel_dp, adjusted_mode->crtc_hdisplay,
				    adjusted_mode->crtc_clock))
		pipe_config->bigjoiner_pipes = GENMASK(crtc->pipe + 1, crtc->pipe);

	 
	joiner_needs_dsc = DISPLAY_VER(i915) < 13 && pipe_config->bigjoiner_pipes;

	 
	ret = intel_dp_compute_link_config_wide(intel_dp, pipe_config, conn_state, &limits);

	if (ret || joiner_needs_dsc || intel_dp->force_dsc_en) {
		drm_dbg_kms(&i915->drm, "Try DSC (fallback=%s, joiner=%s, force=%s)\n",
			    str_yes_no(ret), str_yes_no(joiner_needs_dsc),
			    str_yes_no(intel_dp->force_dsc_en));
		ret = intel_dp_dsc_compute_config(intel_dp, pipe_config,
						  conn_state, &limits, 64, true);
		if (ret < 0)
			return ret;
	}

	if (pipe_config->dsc.compression_enable) {
		drm_dbg_kms(&i915->drm,
			    "DP lane count %d clock %d Input bpp %d Compressed bpp %d\n",
			    pipe_config->lane_count, pipe_config->port_clock,
			    pipe_config->pipe_bpp,
			    pipe_config->dsc.compressed_bpp);

		drm_dbg_kms(&i915->drm,
			    "DP link rate required %i available %i\n",
			    intel_dp_link_required(adjusted_mode->crtc_clock,
						   pipe_config->dsc.compressed_bpp),
			    intel_dp_max_data_rate(pipe_config->port_clock,
						   pipe_config->lane_count));
	} else {
		drm_dbg_kms(&i915->drm, "DP lane count %d clock %d bpp %d\n",
			    pipe_config->lane_count, pipe_config->port_clock,
			    pipe_config->pipe_bpp);

		drm_dbg_kms(&i915->drm,
			    "DP link rate required %i available %i\n",
			    intel_dp_link_required(adjusted_mode->crtc_clock,
						   pipe_config->pipe_bpp),
			    intel_dp_max_data_rate(pipe_config->port_clock,
						   pipe_config->lane_count));
	}
	return 0;
}

bool intel_dp_limited_color_range(const struct intel_crtc_state *crtc_state,
				  const struct drm_connector_state *conn_state)
{
	const struct intel_digital_connector_state *intel_conn_state =
		to_intel_digital_connector_state(conn_state);
	const struct drm_display_mode *adjusted_mode =
		&crtc_state->hw.adjusted_mode;

	 
	if (crtc_state->output_format != INTEL_OUTPUT_FORMAT_RGB)
		return false;

	if (intel_conn_state->broadcast_rgb == INTEL_BROADCAST_RGB_AUTO) {
		 
		return crtc_state->pipe_bpp != 18 &&
			drm_default_rgb_quant_range(adjusted_mode) ==
			HDMI_QUANTIZATION_RANGE_LIMITED;
	} else {
		return intel_conn_state->broadcast_rgb ==
			INTEL_BROADCAST_RGB_LIMITED;
	}
}

static bool intel_dp_port_has_audio(struct drm_i915_private *dev_priv,
				    enum port port)
{
	if (IS_G4X(dev_priv))
		return false;
	if (DISPLAY_VER(dev_priv) < 12 && port == PORT_A)
		return false;

	return true;
}

static void intel_dp_compute_vsc_colorimetry(const struct intel_crtc_state *crtc_state,
					     const struct drm_connector_state *conn_state,
					     struct drm_dp_vsc_sdp *vsc)
{
	struct intel_crtc *crtc = to_intel_crtc(crtc_state->uapi.crtc);
	struct drm_i915_private *dev_priv = to_i915(crtc->base.dev);

	 
	vsc->revision = 0x5;
	vsc->length = 0x13;

	 
	switch (crtc_state->output_format) {
	case INTEL_OUTPUT_FORMAT_YCBCR444:
		vsc->pixelformat = DP_PIXELFORMAT_YUV444;
		break;
	case INTEL_OUTPUT_FORMAT_YCBCR420:
		vsc->pixelformat = DP_PIXELFORMAT_YUV420;
		break;
	case INTEL_OUTPUT_FORMAT_RGB:
	default:
		vsc->pixelformat = DP_PIXELFORMAT_RGB;
	}

	switch (conn_state->colorspace) {
	case DRM_MODE_COLORIMETRY_BT709_YCC:
		vsc->colorimetry = DP_COLORIMETRY_BT709_YCC;
		break;
	case DRM_MODE_COLORIMETRY_XVYCC_601:
		vsc->colorimetry = DP_COLORIMETRY_XVYCC_601;
		break;
	case DRM_MODE_COLORIMETRY_XVYCC_709:
		vsc->colorimetry = DP_COLORIMETRY_XVYCC_709;
		break;
	case DRM_MODE_COLORIMETRY_SYCC_601:
		vsc->colorimetry = DP_COLORIMETRY_SYCC_601;
		break;
	case DRM_MODE_COLORIMETRY_OPYCC_601:
		vsc->colorimetry = DP_COLORIMETRY_OPYCC_601;
		break;
	case DRM_MODE_COLORIMETRY_BT2020_CYCC:
		vsc->colorimetry = DP_COLORIMETRY_BT2020_CYCC;
		break;
	case DRM_MODE_COLORIMETRY_BT2020_RGB:
		vsc->colorimetry = DP_COLORIMETRY_BT2020_RGB;
		break;
	case DRM_MODE_COLORIMETRY_BT2020_YCC:
		vsc->colorimetry = DP_COLORIMETRY_BT2020_YCC;
		break;
	case DRM_MODE_COLORIMETRY_DCI_P3_RGB_D65:
	case DRM_MODE_COLORIMETRY_DCI_P3_RGB_THEATER:
		vsc->colorimetry = DP_COLORIMETRY_DCI_P3_RGB;
		break;
	default:
		 
		if (crtc_state->output_format == INTEL_OUTPUT_FORMAT_YCBCR420)
			vsc->colorimetry = DP_COLORIMETRY_BT709_YCC;
		else
			vsc->colorimetry = DP_COLORIMETRY_DEFAULT;
		break;
	}

	vsc->bpc = crtc_state->pipe_bpp / 3;

	 
	drm_WARN_ON(&dev_priv->drm,
		    vsc->bpc == 6 && vsc->pixelformat != DP_PIXELFORMAT_RGB);

	 
	vsc->dynamic_range = DP_DYNAMIC_RANGE_CTA;
	vsc->content_type = DP_CONTENT_TYPE_NOT_DEFINED;
}

static void intel_dp_compute_vsc_sdp(struct intel_dp *intel_dp,
				     struct intel_crtc_state *crtc_state,
				     const struct drm_connector_state *conn_state)
{
	struct drm_dp_vsc_sdp *vsc = &crtc_state->infoframes.vsc;

	 
	if (crtc_state->has_psr)
		return;

	if (!intel_dp_needs_vsc_sdp(crtc_state, conn_state))
		return;

	crtc_state->infoframes.enable |= intel_hdmi_infoframe_enable(DP_SDP_VSC);
	vsc->sdp_type = DP_SDP_VSC;
	intel_dp_compute_vsc_colorimetry(crtc_state, conn_state,
					 &crtc_state->infoframes.vsc);
}

void intel_dp_compute_psr_vsc_sdp(struct intel_dp *intel_dp,
				  const struct intel_crtc_state *crtc_state,
				  const struct drm_connector_state *conn_state,
				  struct drm_dp_vsc_sdp *vsc)
{
	vsc->sdp_type = DP_SDP_VSC;

	if (crtc_state->has_psr2) {
		if (intel_dp->psr.colorimetry_support &&
		    intel_dp_needs_vsc_sdp(crtc_state, conn_state)) {
			 
			intel_dp_compute_vsc_colorimetry(crtc_state, conn_state,
							 vsc);
		} else {
			 
			vsc->revision = 0x4;
			vsc->length = 0xe;
		}
	} else {
		 
		vsc->revision = 0x2;
		vsc->length = 0x8;
	}
}

static void
intel_dp_compute_hdr_metadata_infoframe_sdp(struct intel_dp *intel_dp,
					    struct intel_crtc_state *crtc_state,
					    const struct drm_connector_state *conn_state)
{
	int ret;
	struct drm_i915_private *dev_priv = dp_to_i915(intel_dp);
	struct hdmi_drm_infoframe *drm_infoframe = &crtc_state->infoframes.drm.drm;

	if (!conn_state->hdr_output_metadata)
		return;

	ret = drm_hdmi_infoframe_set_hdr_metadata(drm_infoframe, conn_state);

	if (ret) {
		drm_dbg_kms(&dev_priv->drm, "couldn't set HDR metadata in infoframe\n");
		return;
	}

	crtc_state->infoframes.enable |=
		intel_hdmi_infoframe_enable(HDMI_PACKET_TYPE_GAMUT_METADATA);
}

static bool cpu_transcoder_has_drrs(struct drm_i915_private *i915,
				    enum transcoder cpu_transcoder)
{
	if (HAS_DOUBLE_BUFFERED_M_N(i915))
		return true;

	return intel_cpu_transcoder_has_m2_n2(i915, cpu_transcoder);
}

static bool can_enable_drrs(struct intel_connector *connector,
			    const struct intel_crtc_state *pipe_config,
			    const struct drm_display_mode *downclock_mode)
{
	struct drm_i915_private *i915 = to_i915(connector->base.dev);

	if (pipe_config->vrr.enable)
		return false;

	 
	if (pipe_config->has_psr)
		return false;

	 
	if (pipe_config->has_pch_encoder)
		return false;

	if (!cpu_transcoder_has_drrs(i915, pipe_config->cpu_transcoder))
		return false;

	return downclock_mode &&
		intel_panel_drrs_type(connector) == DRRS_TYPE_SEAMLESS;
}

static void
intel_dp_drrs_compute_config(struct intel_connector *connector,
			     struct intel_crtc_state *pipe_config,
			     int output_bpp)
{
	struct drm_i915_private *i915 = to_i915(connector->base.dev);
	const struct drm_display_mode *downclock_mode =
		intel_panel_downclock_mode(connector, &pipe_config->hw.adjusted_mode);
	int pixel_clock;

	if (has_seamless_m_n(connector))
		pipe_config->seamless_m_n = true;

	if (!can_enable_drrs(connector, pipe_config, downclock_mode)) {
		if (intel_cpu_transcoder_has_m2_n2(i915, pipe_config->cpu_transcoder))
			intel_zero_m_n(&pipe_config->dp_m2_n2);
		return;
	}

	if (IS_IRONLAKE(i915) || IS_SANDYBRIDGE(i915) || IS_IVYBRIDGE(i915))
		pipe_config->msa_timing_delay = connector->panel.vbt.edp.drrs_msa_timing_delay;

	pipe_config->has_drrs = true;

	pixel_clock = downclock_mode->clock;
	if (pipe_config->splitter.enable)
		pixel_clock /= pipe_config->splitter.link_count;

	intel_link_compute_m_n(output_bpp, pipe_config->lane_count, pixel_clock,
			       pipe_config->port_clock, &pipe_config->dp_m2_n2,
			       pipe_config->fec_enable);

	 
	if (pipe_config->splitter.enable)
		pipe_config->dp_m2_n2.data_m *= pipe_config->splitter.link_count;
}

static bool intel_dp_has_audio(struct intel_encoder *encoder,
			       const struct drm_connector_state *conn_state)
{
	struct drm_i915_private *i915 = to_i915(encoder->base.dev);
	struct intel_dp *intel_dp = enc_to_intel_dp(encoder);
	struct intel_connector *connector = intel_dp->attached_connector;
	const struct intel_digital_connector_state *intel_conn_state =
		to_intel_digital_connector_state(conn_state);

	if (!intel_dp_port_has_audio(i915, encoder->port))
		return false;

	if (intel_conn_state->force_audio == HDMI_AUDIO_AUTO)
		return connector->base.display_info.has_audio;
	else
		return intel_conn_state->force_audio == HDMI_AUDIO_ON;
}

static int
intel_dp_compute_output_format(struct intel_encoder *encoder,
			       struct intel_crtc_state *crtc_state,
			       struct drm_connector_state *conn_state,
			       bool respect_downstream_limits)
{
	struct drm_i915_private *i915 = to_i915(encoder->base.dev);
	struct intel_dp *intel_dp = enc_to_intel_dp(encoder);
	struct intel_connector *connector = intel_dp->attached_connector;
	const struct drm_display_info *info = &connector->base.display_info;
	const struct drm_display_mode *adjusted_mode = &crtc_state->hw.adjusted_mode;
	bool ycbcr_420_only;
	int ret;

	ycbcr_420_only = drm_mode_is_420_only(info, adjusted_mode);

	if (ycbcr_420_only && !connector->base.ycbcr_420_allowed) {
		drm_dbg_kms(&i915->drm,
			    "YCbCr 4:2:0 mode but YCbCr 4:2:0 output not possible. Falling back to RGB.\n");
		crtc_state->sink_format = INTEL_OUTPUT_FORMAT_RGB;
	} else {
		crtc_state->sink_format = intel_dp_sink_format(connector, adjusted_mode);
	}

	crtc_state->output_format = intel_dp_output_format(connector, crtc_state->sink_format);

	ret = intel_dp_compute_link_config(encoder, crtc_state, conn_state,
					   respect_downstream_limits);
	if (ret) {
		if (crtc_state->sink_format == INTEL_OUTPUT_FORMAT_YCBCR420 ||
		    !connector->base.ycbcr_420_allowed ||
		    !drm_mode_is_420_also(info, adjusted_mode))
			return ret;

		crtc_state->sink_format = INTEL_OUTPUT_FORMAT_YCBCR420;
		crtc_state->output_format = intel_dp_output_format(connector,
								   crtc_state->sink_format);
		ret = intel_dp_compute_link_config(encoder, crtc_state, conn_state,
						   respect_downstream_limits);
	}

	return ret;
}

static void
intel_dp_audio_compute_config(struct intel_encoder *encoder,
			      struct intel_crtc_state *pipe_config,
			      struct drm_connector_state *conn_state)
{
	struct drm_i915_private *i915 = to_i915(encoder->base.dev);
	struct drm_connector *connector = conn_state->connector;

	pipe_config->sdp_split_enable =
		intel_dp_has_audio(encoder, conn_state) &&
		intel_dp_is_uhbr(pipe_config);

	drm_dbg_kms(&i915->drm, "[CONNECTOR:%d:%s] SDP split enable: %s\n",
		    connector->base.id, connector->name,
		    str_yes_no(pipe_config->sdp_split_enable));
}

int
intel_dp_compute_config(struct intel_encoder *encoder,
			struct intel_crtc_state *pipe_config,
			struct drm_connector_state *conn_state)
{
	struct drm_i915_private *dev_priv = to_i915(encoder->base.dev);
	struct drm_display_mode *adjusted_mode = &pipe_config->hw.adjusted_mode;
	struct intel_dp *intel_dp = enc_to_intel_dp(encoder);
	const struct drm_display_mode *fixed_mode;
	struct intel_connector *connector = intel_dp->attached_connector;
	int ret = 0, output_bpp;

	if (HAS_PCH_SPLIT(dev_priv) && !HAS_DDI(dev_priv) && encoder->port != PORT_A)
		pipe_config->has_pch_encoder = true;

	pipe_config->has_audio =
		intel_dp_has_audio(encoder, conn_state) &&
		intel_audio_compute_config(encoder, pipe_config, conn_state);

	fixed_mode = intel_panel_fixed_mode(connector, adjusted_mode);
	if (intel_dp_is_edp(intel_dp) && fixed_mode) {
		ret = intel_panel_compute_config(connector, adjusted_mode);
		if (ret)
			return ret;
	}

	if (adjusted_mode->flags & DRM_MODE_FLAG_DBLSCAN)
		return -EINVAL;

	if (!connector->base.interlace_allowed &&
	    adjusted_mode->flags & DRM_MODE_FLAG_INTERLACE)
		return -EINVAL;

	if (adjusted_mode->flags & DRM_MODE_FLAG_DBLCLK)
		return -EINVAL;

	if (intel_dp_hdisplay_bad(dev_priv, adjusted_mode->crtc_hdisplay))
		return -EINVAL;

	 
	ret = intel_dp_compute_output_format(encoder, pipe_config, conn_state, true);
	if (ret)
		ret = intel_dp_compute_output_format(encoder, pipe_config, conn_state, false);
	if (ret)
		return ret;

	if ((intel_dp_is_edp(intel_dp) && fixed_mode) ||
	    pipe_config->output_format == INTEL_OUTPUT_FORMAT_YCBCR420) {
		ret = intel_panel_fitting(pipe_config, conn_state);
		if (ret)
			return ret;
	}

	pipe_config->limited_color_range =
		intel_dp_limited_color_range(pipe_config, conn_state);

	pipe_config->enhanced_framing =
		drm_dp_enhanced_frame_cap(intel_dp->dpcd);

	if (pipe_config->dsc.compression_enable)
		output_bpp = pipe_config->dsc.compressed_bpp;
	else
		output_bpp = intel_dp_output_bpp(pipe_config->output_format,
						 pipe_config->pipe_bpp);

	if (intel_dp->mso_link_count) {
		int n = intel_dp->mso_link_count;
		int overlap = intel_dp->mso_pixel_overlap;

		pipe_config->splitter.enable = true;
		pipe_config->splitter.link_count = n;
		pipe_config->splitter.pixel_overlap = overlap;

		drm_dbg_kms(&dev_priv->drm, "MSO link count %d, pixel overlap %d\n",
			    n, overlap);

		adjusted_mode->crtc_hdisplay = adjusted_mode->crtc_hdisplay / n + overlap;
		adjusted_mode->crtc_hblank_start = adjusted_mode->crtc_hblank_start / n + overlap;
		adjusted_mode->crtc_hblank_end = adjusted_mode->crtc_hblank_end / n + overlap;
		adjusted_mode->crtc_hsync_start = adjusted_mode->crtc_hsync_start / n + overlap;
		adjusted_mode->crtc_hsync_end = adjusted_mode->crtc_hsync_end / n + overlap;
		adjusted_mode->crtc_htotal = adjusted_mode->crtc_htotal / n + overlap;
		adjusted_mode->crtc_clock /= n;
	}

	intel_dp_audio_compute_config(encoder, pipe_config, conn_state);

	intel_link_compute_m_n(output_bpp,
			       pipe_config->lane_count,
			       adjusted_mode->crtc_clock,
			       pipe_config->port_clock,
			       &pipe_config->dp_m_n,
			       pipe_config->fec_enable);

	 
	if (pipe_config->splitter.enable)
		pipe_config->dp_m_n.data_m *= pipe_config->splitter.link_count;

	if (!HAS_DDI(dev_priv))
		g4x_dp_set_clock(encoder, pipe_config);

	intel_vrr_compute_config(pipe_config, conn_state);
	intel_psr_compute_config(intel_dp, pipe_config, conn_state);
	intel_dp_drrs_compute_config(connector, pipe_config, output_bpp);
	intel_dp_compute_vsc_sdp(intel_dp, pipe_config, conn_state);
	intel_dp_compute_hdr_metadata_infoframe_sdp(intel_dp, pipe_config, conn_state);

	return 0;
}

void intel_dp_set_link_params(struct intel_dp *intel_dp,
			      int link_rate, int lane_count)
{
	memset(intel_dp->train_set, 0, sizeof(intel_dp->train_set));
	intel_dp->link_trained = false;
	intel_dp->link_rate = link_rate;
	intel_dp->lane_count = lane_count;
}

static void intel_dp_reset_max_link_params(struct intel_dp *intel_dp)
{
	intel_dp->max_link_lane_count = intel_dp_max_common_lane_count(intel_dp);
	intel_dp->max_link_rate = intel_dp_max_common_rate(intel_dp);
}

 
void intel_edp_backlight_on(const struct intel_crtc_state *crtc_state,
			    const struct drm_connector_state *conn_state)
{
	struct intel_dp *intel_dp = enc_to_intel_dp(to_intel_encoder(conn_state->best_encoder));
	struct drm_i915_private *i915 = dp_to_i915(intel_dp);

	if (!intel_dp_is_edp(intel_dp))
		return;

	drm_dbg_kms(&i915->drm, "\n");

	intel_backlight_enable(crtc_state, conn_state);
	intel_pps_backlight_on(intel_dp);
}

 
void intel_edp_backlight_off(const struct drm_connector_state *old_conn_state)
{
	struct intel_dp *intel_dp = enc_to_intel_dp(to_intel_encoder(old_conn_state->best_encoder));
	struct drm_i915_private *i915 = dp_to_i915(intel_dp);

	if (!intel_dp_is_edp(intel_dp))
		return;

	drm_dbg_kms(&i915->drm, "\n");

	intel_pps_backlight_off(intel_dp);
	intel_backlight_disable(old_conn_state);
}

static bool downstream_hpd_needs_d0(struct intel_dp *intel_dp)
{
	 
	return intel_dp->dpcd[DP_DPCD_REV] == 0x11 &&
		drm_dp_is_branch(intel_dp->dpcd) &&
		intel_dp->downstream_ports[0] & DP_DS_PORT_HPD;
}

void intel_dp_sink_set_decompression_state(struct intel_dp *intel_dp,
					   const struct intel_crtc_state *crtc_state,
					   bool enable)
{
	struct drm_i915_private *i915 = dp_to_i915(intel_dp);
	int ret;

	if (!crtc_state->dsc.compression_enable)
		return;

	ret = drm_dp_dpcd_writeb(&intel_dp->aux, DP_DSC_ENABLE,
				 enable ? DP_DECOMPRESSION_EN : 0);
	if (ret < 0)
		drm_dbg_kms(&i915->drm,
			    "Failed to %s sink decompression state\n",
			    str_enable_disable(enable));
}

static void
intel_edp_init_source_oui(struct intel_dp *intel_dp, bool careful)
{
	struct drm_i915_private *i915 = dp_to_i915(intel_dp);
	u8 oui[] = { 0x00, 0xaa, 0x01 };
	u8 buf[3] = { 0 };

	 
	if (careful) {
		if (drm_dp_dpcd_read(&intel_dp->aux, DP_SOURCE_OUI, buf, sizeof(buf)) < 0)
			drm_err(&i915->drm, "Failed to read source OUI\n");

		if (memcmp(oui, buf, sizeof(oui)) == 0)
			return;
	}

	if (drm_dp_dpcd_write(&intel_dp->aux, DP_SOURCE_OUI, oui, sizeof(oui)) < 0)
		drm_err(&i915->drm, "Failed to write source OUI\n");

	intel_dp->last_oui_write = jiffies;
}

void intel_dp_wait_source_oui(struct intel_dp *intel_dp)
{
	struct intel_connector *connector = intel_dp->attached_connector;
	struct drm_i915_private *i915 = dp_to_i915(intel_dp);

	drm_dbg_kms(&i915->drm, "[CONNECTOR:%d:%s] Performing OUI wait (%u ms)\n",
		    connector->base.base.id, connector->base.name,
		    connector->panel.vbt.backlight.hdr_dpcd_refresh_timeout);

	wait_remaining_ms_from_jiffies(intel_dp->last_oui_write,
				       connector->panel.vbt.backlight.hdr_dpcd_refresh_timeout);
}

 
void intel_dp_set_power(struct intel_dp *intel_dp, u8 mode)
{
	struct intel_encoder *encoder = &dp_to_dig_port(intel_dp)->base;
	struct drm_i915_private *i915 = to_i915(encoder->base.dev);
	int ret, i;

	 
	if (intel_dp->dpcd[DP_DPCD_REV] < 0x11)
		return;

	if (mode != DP_SET_POWER_D0) {
		if (downstream_hpd_needs_d0(intel_dp))
			return;

		ret = drm_dp_dpcd_writeb(&intel_dp->aux, DP_SET_POWER, mode);
	} else {
		struct intel_lspcon *lspcon = dp_to_lspcon(intel_dp);

		lspcon_resume(dp_to_dig_port(intel_dp));

		 
		if (intel_dp_is_edp(intel_dp))
			intel_edp_init_source_oui(intel_dp, false);

		 
		for (i = 0; i < 3; i++) {
			ret = drm_dp_dpcd_writeb(&intel_dp->aux, DP_SET_POWER, mode);
			if (ret == 1)
				break;
			msleep(1);
		}

		if (ret == 1 && lspcon->active)
			lspcon_wait_pcon_mode(lspcon);
	}

	if (ret != 1)
		drm_dbg_kms(&i915->drm, "[ENCODER:%d:%s] Set power to %s failed\n",
			    encoder->base.base.id, encoder->base.name,
			    mode == DP_SET_POWER_D0 ? "D0" : "D3");
}

static bool
intel_dp_get_dpcd(struct intel_dp *intel_dp);

 
void intel_dp_sync_state(struct intel_encoder *encoder,
			 const struct intel_crtc_state *crtc_state)
{
	struct intel_dp *intel_dp = enc_to_intel_dp(encoder);

	if (!crtc_state)
		return;

	 
	if (intel_dp->dpcd[DP_DPCD_REV] == 0)
		intel_dp_get_dpcd(intel_dp);

	intel_dp_reset_max_link_params(intel_dp);
}

bool intel_dp_initial_fastset_check(struct intel_encoder *encoder,
				    struct intel_crtc_state *crtc_state)
{
	struct drm_i915_private *i915 = to_i915(encoder->base.dev);
	struct intel_dp *intel_dp = enc_to_intel_dp(encoder);
	bool fastset = true;

	 
	if (intel_dp_rate_index(intel_dp->source_rates, intel_dp->num_source_rates,
				crtc_state->port_clock) < 0) {
		drm_dbg_kms(&i915->drm, "[ENCODER:%d:%s] Forcing full modeset due to unsupported link rate\n",
			    encoder->base.base.id, encoder->base.name);
		crtc_state->uapi.connectors_changed = true;
		fastset = false;
	}

	 
	if (crtc_state->dsc.compression_enable) {
		drm_dbg_kms(&i915->drm, "[ENCODER:%d:%s] Forcing full modeset due to DSC being enabled\n",
			    encoder->base.base.id, encoder->base.name);
		crtc_state->uapi.mode_changed = true;
		fastset = false;
	}

	if (CAN_PSR(intel_dp)) {
		drm_dbg_kms(&i915->drm, "[ENCODER:%d:%s] Forcing full modeset to compute PSR state\n",
			    encoder->base.base.id, encoder->base.name);
		crtc_state->uapi.mode_changed = true;
		fastset = false;
	}

	return fastset;
}

static void intel_dp_get_pcon_dsc_cap(struct intel_dp *intel_dp)
{
	struct drm_i915_private *i915 = dp_to_i915(intel_dp);

	 

	memset(intel_dp->pcon_dsc_dpcd, 0, sizeof(intel_dp->pcon_dsc_dpcd));

	if (drm_dp_dpcd_read(&intel_dp->aux, DP_PCON_DSC_ENCODER,
			     intel_dp->pcon_dsc_dpcd,
			     sizeof(intel_dp->pcon_dsc_dpcd)) < 0)
		drm_err(&i915->drm, "Failed to read DPCD register 0x%x\n",
			DP_PCON_DSC_ENCODER);

	drm_dbg_kms(&i915->drm, "PCON ENCODER DSC DPCD: %*ph\n",
		    (int)sizeof(intel_dp->pcon_dsc_dpcd), intel_dp->pcon_dsc_dpcd);
}

static int intel_dp_pcon_get_frl_mask(u8 frl_bw_mask)
{
	int bw_gbps[] = {9, 18, 24, 32, 40, 48};
	int i;

	for (i = ARRAY_SIZE(bw_gbps) - 1; i >= 0; i--) {
		if (frl_bw_mask & (1 << i))
			return bw_gbps[i];
	}
	return 0;
}

static int intel_dp_pcon_set_frl_mask(int max_frl)
{
	switch (max_frl) {
	case 48:
		return DP_PCON_FRL_BW_MASK_48GBPS;
	case 40:
		return DP_PCON_FRL_BW_MASK_40GBPS;
	case 32:
		return DP_PCON_FRL_BW_MASK_32GBPS;
	case 24:
		return DP_PCON_FRL_BW_MASK_24GBPS;
	case 18:
		return DP_PCON_FRL_BW_MASK_18GBPS;
	case 9:
		return DP_PCON_FRL_BW_MASK_9GBPS;
	}

	return 0;
}

static int intel_dp_hdmi_sink_max_frl(struct intel_dp *intel_dp)
{
	struct intel_connector *intel_connector = intel_dp->attached_connector;
	struct drm_connector *connector = &intel_connector->base;
	int max_frl_rate;
	int max_lanes, rate_per_lane;
	int max_dsc_lanes, dsc_rate_per_lane;

	max_lanes = connector->display_info.hdmi.max_lanes;
	rate_per_lane = connector->display_info.hdmi.max_frl_rate_per_lane;
	max_frl_rate = max_lanes * rate_per_lane;

	if (connector->display_info.hdmi.dsc_cap.v_1p2) {
		max_dsc_lanes = connector->display_info.hdmi.dsc_cap.max_lanes;
		dsc_rate_per_lane = connector->display_info.hdmi.dsc_cap.max_frl_rate_per_lane;
		if (max_dsc_lanes && dsc_rate_per_lane)
			max_frl_rate = min(max_frl_rate, max_dsc_lanes * dsc_rate_per_lane);
	}

	return max_frl_rate;
}

static bool
intel_dp_pcon_is_frl_trained(struct intel_dp *intel_dp,
			     u8 max_frl_bw_mask, u8 *frl_trained_mask)
{
	if (drm_dp_pcon_hdmi_link_active(&intel_dp->aux) &&
	    drm_dp_pcon_hdmi_link_mode(&intel_dp->aux, frl_trained_mask) == DP_PCON_HDMI_MODE_FRL &&
	    *frl_trained_mask >= max_frl_bw_mask)
		return true;

	return false;
}

static int intel_dp_pcon_start_frl_training(struct intel_dp *intel_dp)
{
#define TIMEOUT_FRL_READY_MS 500
#define TIMEOUT_HDMI_LINK_ACTIVE_MS 1000

	struct drm_i915_private *i915 = dp_to_i915(intel_dp);
	int max_frl_bw, max_pcon_frl_bw, max_edid_frl_bw, ret;
	u8 max_frl_bw_mask = 0, frl_trained_mask;
	bool is_active;

	max_pcon_frl_bw = intel_dp->dfp.pcon_max_frl_bw;
	drm_dbg(&i915->drm, "PCON max rate = %d Gbps\n", max_pcon_frl_bw);

	max_edid_frl_bw = intel_dp_hdmi_sink_max_frl(intel_dp);
	drm_dbg(&i915->drm, "Sink max rate from EDID = %d Gbps\n", max_edid_frl_bw);

	max_frl_bw = min(max_edid_frl_bw, max_pcon_frl_bw);

	if (max_frl_bw <= 0)
		return -EINVAL;

	max_frl_bw_mask = intel_dp_pcon_set_frl_mask(max_frl_bw);
	drm_dbg(&i915->drm, "MAX_FRL_BW_MASK = %u\n", max_frl_bw_mask);

	if (intel_dp_pcon_is_frl_trained(intel_dp, max_frl_bw_mask, &frl_trained_mask))
		goto frl_trained;

	ret = drm_dp_pcon_frl_prepare(&intel_dp->aux, false);
	if (ret < 0)
		return ret;
	 
	wait_for(is_active = drm_dp_pcon_is_frl_ready(&intel_dp->aux) == true, TIMEOUT_FRL_READY_MS);

	if (!is_active)
		return -ETIMEDOUT;

	ret = drm_dp_pcon_frl_configure_1(&intel_dp->aux, max_frl_bw,
					  DP_PCON_ENABLE_SEQUENTIAL_LINK);
	if (ret < 0)
		return ret;
	ret = drm_dp_pcon_frl_configure_2(&intel_dp->aux, max_frl_bw_mask,
					  DP_PCON_FRL_LINK_TRAIN_NORMAL);
	if (ret < 0)
		return ret;
	ret = drm_dp_pcon_frl_enable(&intel_dp->aux);
	if (ret < 0)
		return ret;
	 
	wait_for(is_active =
		 intel_dp_pcon_is_frl_trained(intel_dp, max_frl_bw_mask, &frl_trained_mask),
		 TIMEOUT_HDMI_LINK_ACTIVE_MS);

	if (!is_active)
		return -ETIMEDOUT;

frl_trained:
	drm_dbg(&i915->drm, "FRL_TRAINED_MASK = %u\n", frl_trained_mask);
	intel_dp->frl.trained_rate_gbps = intel_dp_pcon_get_frl_mask(frl_trained_mask);
	intel_dp->frl.is_trained = true;
	drm_dbg(&i915->drm, "FRL trained with : %d Gbps\n", intel_dp->frl.trained_rate_gbps);

	return 0;
}

static bool intel_dp_is_hdmi_2_1_sink(struct intel_dp *intel_dp)
{
	if (drm_dp_is_branch(intel_dp->dpcd) &&
	    intel_dp_has_hdmi_sink(intel_dp) &&
	    intel_dp_hdmi_sink_max_frl(intel_dp) > 0)
		return true;

	return false;
}

static
int intel_dp_pcon_set_tmds_mode(struct intel_dp *intel_dp)
{
	int ret;
	u8 buf = 0;

	 
	buf |= DP_PCON_ENABLE_SOURCE_CTL_MODE;

	ret = drm_dp_dpcd_writeb(&intel_dp->aux, DP_PCON_HDMI_LINK_CONFIG_1, buf);
	if (ret < 0)
		return ret;

	 
	buf |= DP_PCON_ENABLE_HDMI_LINK;
	ret = drm_dp_dpcd_writeb(&intel_dp->aux, DP_PCON_HDMI_LINK_CONFIG_1, buf);
	if (ret < 0)
		return ret;

	return 0;
}

void intel_dp_check_frl_training(struct intel_dp *intel_dp)
{
	struct drm_i915_private *dev_priv = dp_to_i915(intel_dp);

	 
	if (!(intel_dp->downstream_ports[2] & DP_PCON_SOURCE_CTL_MODE) ||
	    !intel_dp_is_hdmi_2_1_sink(intel_dp) ||
	    intel_dp->frl.is_trained)
		return;

	if (intel_dp_pcon_start_frl_training(intel_dp) < 0) {
		int ret, mode;

		drm_dbg(&dev_priv->drm, "Couldn't set FRL mode, continuing with TMDS mode\n");
		ret = intel_dp_pcon_set_tmds_mode(intel_dp);
		mode = drm_dp_pcon_hdmi_link_mode(&intel_dp->aux, NULL);

		if (ret < 0 || mode != DP_PCON_HDMI_MODE_TMDS)
			drm_dbg(&dev_priv->drm, "Issue with PCON, cannot set TMDS mode\n");
	} else {
		drm_dbg(&dev_priv->drm, "FRL training Completed\n");
	}
}

static int
intel_dp_pcon_dsc_enc_slice_height(const struct intel_crtc_state *crtc_state)
{
	int vactive = crtc_state->hw.adjusted_mode.vdisplay;

	return intel_hdmi_dsc_get_slice_height(vactive);
}

static int
intel_dp_pcon_dsc_enc_slices(struct intel_dp *intel_dp,
			     const struct intel_crtc_state *crtc_state)
{
	struct intel_connector *intel_connector = intel_dp->attached_connector;
	struct drm_connector *connector = &intel_connector->base;
	int hdmi_throughput = connector->display_info.hdmi.dsc_cap.clk_per_slice;
	int hdmi_max_slices = connector->display_info.hdmi.dsc_cap.max_slices;
	int pcon_max_slices = drm_dp_pcon_dsc_max_slices(intel_dp->pcon_dsc_dpcd);
	int pcon_max_slice_width = drm_dp_pcon_dsc_max_slice_width(intel_dp->pcon_dsc_dpcd);

	return intel_hdmi_dsc_get_num_slices(crtc_state, pcon_max_slices,
					     pcon_max_slice_width,
					     hdmi_max_slices, hdmi_throughput);
}

static int
intel_dp_pcon_dsc_enc_bpp(struct intel_dp *intel_dp,
			  const struct intel_crtc_state *crtc_state,
			  int num_slices, int slice_width)
{
	struct intel_connector *intel_connector = intel_dp->attached_connector;
	struct drm_connector *connector = &intel_connector->base;
	int output_format = crtc_state->output_format;
	bool hdmi_all_bpp = connector->display_info.hdmi.dsc_cap.all_bpp;
	int pcon_fractional_bpp = drm_dp_pcon_dsc_bpp_incr(intel_dp->pcon_dsc_dpcd);
	int hdmi_max_chunk_bytes =
		connector->display_info.hdmi.dsc_cap.total_chunk_kbytes * 1024;

	return intel_hdmi_dsc_get_bpp(pcon_fractional_bpp, slice_width,
				      num_slices, output_format, hdmi_all_bpp,
				      hdmi_max_chunk_bytes);
}

void
intel_dp_pcon_dsc_configure(struct intel_dp *intel_dp,
			    const struct intel_crtc_state *crtc_state)
{
	u8 pps_param[6];
	int slice_height;
	int slice_width;
	int num_slices;
	int bits_per_pixel;
	int ret;
	struct intel_connector *intel_connector = intel_dp->attached_connector;
	struct drm_i915_private *i915 = dp_to_i915(intel_dp);
	struct drm_connector *connector;
	bool hdmi_is_dsc_1_2;

	if (!intel_dp_is_hdmi_2_1_sink(intel_dp))
		return;

	if (!intel_connector)
		return;
	connector = &intel_connector->base;
	hdmi_is_dsc_1_2 = connector->display_info.hdmi.dsc_cap.v_1p2;

	if (!drm_dp_pcon_enc_is_dsc_1_2(intel_dp->pcon_dsc_dpcd) ||
	    !hdmi_is_dsc_1_2)
		return;

	slice_height = intel_dp_pcon_dsc_enc_slice_height(crtc_state);
	if (!slice_height)
		return;

	num_slices = intel_dp_pcon_dsc_enc_slices(intel_dp, crtc_state);
	if (!num_slices)
		return;

	slice_width = DIV_ROUND_UP(crtc_state->hw.adjusted_mode.hdisplay,
				   num_slices);

	bits_per_pixel = intel_dp_pcon_dsc_enc_bpp(intel_dp, crtc_state,
						   num_slices, slice_width);
	if (!bits_per_pixel)
		return;

	pps_param[0] = slice_height & 0xFF;
	pps_param[1] = slice_height >> 8;
	pps_param[2] = slice_width & 0xFF;
	pps_param[3] = slice_width >> 8;
	pps_param[4] = bits_per_pixel & 0xFF;
	pps_param[5] = (bits_per_pixel >> 8) & 0x3;

	ret = drm_dp_pcon_pps_override_param(&intel_dp->aux, pps_param);
	if (ret < 0)
		drm_dbg_kms(&i915->drm, "Failed to set pcon DSC\n");
}

void intel_dp_configure_protocol_converter(struct intel_dp *intel_dp,
					   const struct intel_crtc_state *crtc_state)
{
	struct drm_i915_private *i915 = dp_to_i915(intel_dp);
	bool ycbcr444_to_420 = false;
	bool rgb_to_ycbcr = false;
	u8 tmp;

	if (intel_dp->dpcd[DP_DPCD_REV] < 0x13)
		return;

	if (!drm_dp_is_branch(intel_dp->dpcd))
		return;

	tmp = intel_dp_has_hdmi_sink(intel_dp) ? DP_HDMI_DVI_OUTPUT_CONFIG : 0;

	if (drm_dp_dpcd_writeb(&intel_dp->aux,
			       DP_PROTOCOL_CONVERTER_CONTROL_0, tmp) != 1)
		drm_dbg_kms(&i915->drm, "Failed to %s protocol converter HDMI mode\n",
			    str_enable_disable(intel_dp_has_hdmi_sink(intel_dp)));

	if (crtc_state->sink_format == INTEL_OUTPUT_FORMAT_YCBCR420) {
		switch (crtc_state->output_format) {
		case INTEL_OUTPUT_FORMAT_YCBCR420:
			break;
		case INTEL_OUTPUT_FORMAT_YCBCR444:
			ycbcr444_to_420 = true;
			break;
		case INTEL_OUTPUT_FORMAT_RGB:
			rgb_to_ycbcr = true;
			ycbcr444_to_420 = true;
			break;
		default:
			MISSING_CASE(crtc_state->output_format);
			break;
		}
	} else if (crtc_state->sink_format == INTEL_OUTPUT_FORMAT_YCBCR444) {
		switch (crtc_state->output_format) {
		case INTEL_OUTPUT_FORMAT_YCBCR444:
			break;
		case INTEL_OUTPUT_FORMAT_RGB:
			rgb_to_ycbcr = true;
			break;
		default:
			MISSING_CASE(crtc_state->output_format);
			break;
		}
	}

	tmp = ycbcr444_to_420 ? DP_CONVERSION_TO_YCBCR420_ENABLE : 0;

	if (drm_dp_dpcd_writeb(&intel_dp->aux,
			       DP_PROTOCOL_CONVERTER_CONTROL_1, tmp) != 1)
		drm_dbg_kms(&i915->drm,
			    "Failed to %s protocol converter YCbCr 4:2:0 conversion mode\n",
			    str_enable_disable(intel_dp->dfp.ycbcr_444_to_420));

	tmp = rgb_to_ycbcr ? DP_CONVERSION_BT709_RGB_YCBCR_ENABLE : 0;

	if (drm_dp_pcon_convert_rgb_to_ycbcr(&intel_dp->aux, tmp) < 0)
		drm_dbg_kms(&i915->drm,
			    "Failed to %s protocol converter RGB->YCbCr conversion mode\n",
			    str_enable_disable(tmp));
}

bool intel_dp_get_colorimetry_status(struct intel_dp *intel_dp)
{
	u8 dprx = 0;

	if (drm_dp_dpcd_readb(&intel_dp->aux, DP_DPRX_FEATURE_ENUMERATION_LIST,
			      &dprx) != 1)
		return false;
	return dprx & DP_VSC_SDP_EXT_FOR_COLORIMETRY_SUPPORTED;
}

static void intel_dp_get_dsc_sink_cap(struct intel_dp *intel_dp)
{
	struct drm_i915_private *i915 = dp_to_i915(intel_dp);

	 
	memset(intel_dp->dsc_dpcd, 0, sizeof(intel_dp->dsc_dpcd));

	 
	intel_dp->fec_capable = 0;

	 
	if (intel_dp->dpcd[DP_DPCD_REV] >= 0x14 ||
	    intel_dp->edp_dpcd[0] >= DP_EDP_14) {
		if (drm_dp_dpcd_read(&intel_dp->aux, DP_DSC_SUPPORT,
				     intel_dp->dsc_dpcd,
				     sizeof(intel_dp->dsc_dpcd)) < 0)
			drm_err(&i915->drm,
				"Failed to read DPCD register 0x%x\n",
				DP_DSC_SUPPORT);

		drm_dbg_kms(&i915->drm, "DSC DPCD: %*ph\n",
			    (int)sizeof(intel_dp->dsc_dpcd),
			    intel_dp->dsc_dpcd);

		 
		if (!intel_dp_is_edp(intel_dp) &&
		    drm_dp_dpcd_readb(&intel_dp->aux, DP_FEC_CAPABILITY,
				      &intel_dp->fec_capable) < 0)
			drm_err(&i915->drm,
				"Failed to read FEC DPCD register\n");

		drm_dbg_kms(&i915->drm, "FEC CAPABILITY: %x\n",
			    intel_dp->fec_capable);
	}
}

static void intel_edp_mso_mode_fixup(struct intel_connector *connector,
				     struct drm_display_mode *mode)
{
	struct intel_dp *intel_dp = intel_attached_dp(connector);
	struct drm_i915_private *i915 = to_i915(connector->base.dev);
	int n = intel_dp->mso_link_count;
	int overlap = intel_dp->mso_pixel_overlap;

	if (!mode || !n)
		return;

	mode->hdisplay = (mode->hdisplay - overlap) * n;
	mode->hsync_start = (mode->hsync_start - overlap) * n;
	mode->hsync_end = (mode->hsync_end - overlap) * n;
	mode->htotal = (mode->htotal - overlap) * n;
	mode->clock *= n;

	drm_mode_set_name(mode);

	drm_dbg_kms(&i915->drm,
		    "[CONNECTOR:%d:%s] using generated MSO mode: " DRM_MODE_FMT "\n",
		    connector->base.base.id, connector->base.name,
		    DRM_MODE_ARG(mode));
}

void intel_edp_fixup_vbt_bpp(struct intel_encoder *encoder, int pipe_bpp)
{
	struct drm_i915_private *dev_priv = to_i915(encoder->base.dev);
	struct intel_dp *intel_dp = enc_to_intel_dp(encoder);
	struct intel_connector *connector = intel_dp->attached_connector;

	if (connector->panel.vbt.edp.bpp && pipe_bpp > connector->panel.vbt.edp.bpp) {
		 
		drm_dbg_kms(&dev_priv->drm,
			    "pipe has %d bpp for eDP panel, overriding BIOS-provided max %d bpp\n",
			    pipe_bpp, connector->panel.vbt.edp.bpp);
		connector->panel.vbt.edp.bpp = pipe_bpp;
	}
}

static void intel_edp_mso_init(struct intel_dp *intel_dp)
{
	struct drm_i915_private *i915 = dp_to_i915(intel_dp);
	struct intel_connector *connector = intel_dp->attached_connector;
	struct drm_display_info *info = &connector->base.display_info;
	u8 mso;

	if (intel_dp->edp_dpcd[0] < DP_EDP_14)
		return;

	if (drm_dp_dpcd_readb(&intel_dp->aux, DP_EDP_MSO_LINK_CAPABILITIES, &mso) != 1) {
		drm_err(&i915->drm, "Failed to read MSO cap\n");
		return;
	}

	 
	mso &= DP_EDP_MSO_NUMBER_OF_LINKS_MASK;
	if (mso % 2 || mso > drm_dp_max_lane_count(intel_dp->dpcd)) {
		drm_err(&i915->drm, "Invalid MSO link count cap %u\n", mso);
		mso = 0;
	}

	if (mso) {
		drm_dbg_kms(&i915->drm, "Sink MSO %ux%u configuration, pixel overlap %u\n",
			    mso, drm_dp_max_lane_count(intel_dp->dpcd) / mso,
			    info->mso_pixel_overlap);
		if (!HAS_MSO(i915)) {
			drm_err(&i915->drm, "No source MSO support, disabling\n");
			mso = 0;
		}
	}

	intel_dp->mso_link_count = mso;
	intel_dp->mso_pixel_overlap = mso ? info->mso_pixel_overlap : 0;
}

static bool
intel_edp_init_dpcd(struct intel_dp *intel_dp)
{
	struct drm_i915_private *dev_priv =
		to_i915(dp_to_dig_port(intel_dp)->base.base.dev);

	 
	drm_WARN_ON(&dev_priv->drm, intel_dp->dpcd[DP_DPCD_REV] != 0);

	if (drm_dp_read_dpcd_caps(&intel_dp->aux, intel_dp->dpcd) != 0)
		return false;

	drm_dp_read_desc(&intel_dp->aux, &intel_dp->desc,
			 drm_dp_is_branch(intel_dp->dpcd));

	 
	if (drm_dp_dpcd_read(&intel_dp->aux, DP_EDP_DPCD_REV,
			     intel_dp->edp_dpcd, sizeof(intel_dp->edp_dpcd)) ==
			     sizeof(intel_dp->edp_dpcd)) {
		drm_dbg_kms(&dev_priv->drm, "eDP DPCD: %*ph\n",
			    (int)sizeof(intel_dp->edp_dpcd),
			    intel_dp->edp_dpcd);

		intel_dp->use_max_params = intel_dp->edp_dpcd[0] < DP_EDP_14;
	}

	 
	intel_psr_init_dpcd(intel_dp);

	 
	intel_dp->num_sink_rates = 0;

	 
	if (intel_dp->edp_dpcd[0] >= DP_EDP_14) {
		__le16 sink_rates[DP_MAX_SUPPORTED_RATES];
		int i;

		drm_dp_dpcd_read(&intel_dp->aux, DP_SUPPORTED_LINK_RATES,
				sink_rates, sizeof(sink_rates));

		for (i = 0; i < ARRAY_SIZE(sink_rates); i++) {
			int val = le16_to_cpu(sink_rates[i]);

			if (val == 0)
				break;

			 
			intel_dp->sink_rates[i] = (val * 200) / 10;
		}
		intel_dp->num_sink_rates = i;
	}

	 
	if (intel_dp->num_sink_rates)
		intel_dp->use_rate_select = true;
	else
		intel_dp_set_sink_rates(intel_dp);
	intel_dp_set_max_sink_lane_count(intel_dp);

	 
	if (HAS_DSC(dev_priv))
		intel_dp_get_dsc_sink_cap(intel_dp);

	 
	intel_edp_init_source_oui(intel_dp, true);

	return true;
}

static bool
intel_dp_has_sink_count(struct intel_dp *intel_dp)
{
	if (!intel_dp->attached_connector)
		return false;

	return drm_dp_read_sink_count_cap(&intel_dp->attached_connector->base,
					  intel_dp->dpcd,
					  &intel_dp->desc);
}

static bool
intel_dp_get_dpcd(struct intel_dp *intel_dp)
{
	int ret;

	if (intel_dp_init_lttpr_and_dprx_caps(intel_dp) < 0)
		return false;

	 
	if (!intel_dp_is_edp(intel_dp)) {
		drm_dp_read_desc(&intel_dp->aux, &intel_dp->desc,
				 drm_dp_is_branch(intel_dp->dpcd));

		intel_dp_set_sink_rates(intel_dp);
		intel_dp_set_max_sink_lane_count(intel_dp);
		intel_dp_set_common_rates(intel_dp);
	}

	if (intel_dp_has_sink_count(intel_dp)) {
		ret = drm_dp_read_sink_count(&intel_dp->aux);
		if (ret < 0)
			return false;

		 
		intel_dp->sink_count = ret;

		 
		if (!intel_dp->sink_count)
			return false;
	}

	return drm_dp_read_downstream_info(&intel_dp->aux, intel_dp->dpcd,
					   intel_dp->downstream_ports) == 0;
}

static bool
intel_dp_can_mst(struct intel_dp *intel_dp)
{
	struct drm_i915_private *i915 = dp_to_i915(intel_dp);

	return i915->params.enable_dp_mst &&
		intel_dp_mst_source_support(intel_dp) &&
		drm_dp_read_mst_cap(&intel_dp->aux, intel_dp->dpcd);
}

static void
intel_dp_configure_mst(struct intel_dp *intel_dp)
{
	struct drm_i915_private *i915 = dp_to_i915(intel_dp);
	struct intel_encoder *encoder =
		&dp_to_dig_port(intel_dp)->base;
	bool sink_can_mst = drm_dp_read_mst_cap(&intel_dp->aux, intel_dp->dpcd);

	drm_dbg_kms(&i915->drm,
		    "[ENCODER:%d:%s] MST support: port: %s, sink: %s, modparam: %s\n",
		    encoder->base.base.id, encoder->base.name,
		    str_yes_no(intel_dp_mst_source_support(intel_dp)),
		    str_yes_no(sink_can_mst),
		    str_yes_no(i915->params.enable_dp_mst));

	if (!intel_dp_mst_source_support(intel_dp))
		return;

	intel_dp->is_mst = sink_can_mst &&
		i915->params.enable_dp_mst;

	drm_dp_mst_topology_mgr_set_mst(&intel_dp->mst_mgr,
					intel_dp->is_mst);
}

static bool
intel_dp_get_sink_irq_esi(struct intel_dp *intel_dp, u8 *esi)
{
	return drm_dp_dpcd_read(&intel_dp->aux, DP_SINK_COUNT_ESI, esi, 4) == 4;
}

static bool intel_dp_ack_sink_irq_esi(struct intel_dp *intel_dp, u8 esi[4])
{
	int retry;

	for (retry = 0; retry < 3; retry++) {
		if (drm_dp_dpcd_write(&intel_dp->aux, DP_SINK_COUNT_ESI + 1,
				      &esi[1], 3) == 3)
			return true;
	}

	return false;
}

bool
intel_dp_needs_vsc_sdp(const struct intel_crtc_state *crtc_state,
		       const struct drm_connector_state *conn_state)
{
	 
	if (crtc_state->output_format == INTEL_OUTPUT_FORMAT_YCBCR420)
		return true;

	switch (conn_state->colorspace) {
	case DRM_MODE_COLORIMETRY_SYCC_601:
	case DRM_MODE_COLORIMETRY_OPYCC_601:
	case DRM_MODE_COLORIMETRY_BT2020_YCC:
	case DRM_MODE_COLORIMETRY_BT2020_RGB:
	case DRM_MODE_COLORIMETRY_BT2020_CYCC:
		return true;
	default:
		break;
	}

	return false;
}

static ssize_t intel_dp_vsc_sdp_pack(const struct drm_dp_vsc_sdp *vsc,
				     struct dp_sdp *sdp, size_t size)
{
	size_t length = sizeof(struct dp_sdp);

	if (size < length)
		return -ENOSPC;

	memset(sdp, 0, size);

	 
	sdp->sdp_header.HB0 = 0;  
	sdp->sdp_header.HB1 = vsc->sdp_type;  
	sdp->sdp_header.HB2 = vsc->revision;  
	sdp->sdp_header.HB3 = vsc->length;  

	 
	if (vsc->revision != 0x5)
		goto out;

	 
	 
	sdp->db[16] = (vsc->pixelformat & 0xf) << 4;  
	sdp->db[16] |= vsc->colorimetry & 0xf;  

	switch (vsc->bpc) {
	case 6:
		 
		break;
	case 8:
		sdp->db[17] = 0x1;  
		break;
	case 10:
		sdp->db[17] = 0x2;
		break;
	case 12:
		sdp->db[17] = 0x3;
		break;
	case 16:
		sdp->db[17] = 0x4;
		break;
	default:
		MISSING_CASE(vsc->bpc);
		break;
	}
	 
	if (vsc->dynamic_range == DP_DYNAMIC_RANGE_CTA)
		sdp->db[17] |= 0x80;   

	 
	sdp->db[18] = vsc->content_type & 0x7;

out:
	return length;
}

static ssize_t
intel_dp_hdr_metadata_infoframe_sdp_pack(struct drm_i915_private *i915,
					 const struct hdmi_drm_infoframe *drm_infoframe,
					 struct dp_sdp *sdp,
					 size_t size)
{
	size_t length = sizeof(struct dp_sdp);
	const int infoframe_size = HDMI_INFOFRAME_HEADER_SIZE + HDMI_DRM_INFOFRAME_SIZE;
	unsigned char buf[HDMI_INFOFRAME_HEADER_SIZE + HDMI_DRM_INFOFRAME_SIZE];
	ssize_t len;

	if (size < length)
		return -ENOSPC;

	memset(sdp, 0, size);

	len = hdmi_drm_infoframe_pack_only(drm_infoframe, buf, sizeof(buf));
	if (len < 0) {
		drm_dbg_kms(&i915->drm, "buffer size is smaller than hdr metadata infoframe\n");
		return -ENOSPC;
	}

	if (len != infoframe_size) {
		drm_dbg_kms(&i915->drm, "wrong static hdr metadata size\n");
		return -ENOSPC;
	}

	 

	 
	sdp->sdp_header.HB0 = 0;
	 
	sdp->sdp_header.HB1 = drm_infoframe->type;
	 
	sdp->sdp_header.HB2 = 0x1D;
	 
	sdp->sdp_header.HB3 = (0x13 << 2);
	 
	sdp->db[0] = drm_infoframe->version;
	 
	sdp->db[1] = drm_infoframe->length;
	 
	BUILD_BUG_ON(sizeof(sdp->db) < HDMI_DRM_INFOFRAME_SIZE + 2);
	memcpy(&sdp->db[2], &buf[HDMI_INFOFRAME_HEADER_SIZE],
	       HDMI_DRM_INFOFRAME_SIZE);

	 
	return sizeof(struct dp_sdp_header) + 2 + HDMI_DRM_INFOFRAME_SIZE;
}

static void intel_write_dp_sdp(struct intel_encoder *encoder,
			       const struct intel_crtc_state *crtc_state,
			       unsigned int type)
{
	struct intel_digital_port *dig_port = enc_to_dig_port(encoder);
	struct drm_i915_private *dev_priv = to_i915(encoder->base.dev);
	struct dp_sdp sdp = {};
	ssize_t len;

	if ((crtc_state->infoframes.enable &
	     intel_hdmi_infoframe_enable(type)) == 0)
		return;

	switch (type) {
	case DP_SDP_VSC:
		len = intel_dp_vsc_sdp_pack(&crtc_state->infoframes.vsc, &sdp,
					    sizeof(sdp));
		break;
	case HDMI_PACKET_TYPE_GAMUT_METADATA:
		len = intel_dp_hdr_metadata_infoframe_sdp_pack(dev_priv,
							       &crtc_state->infoframes.drm.drm,
							       &sdp, sizeof(sdp));
		break;
	default:
		MISSING_CASE(type);
		return;
	}

	if (drm_WARN_ON(&dev_priv->drm, len < 0))
		return;

	dig_port->write_infoframe(encoder, crtc_state, type, &sdp, len);
}

void intel_write_dp_vsc_sdp(struct intel_encoder *encoder,
			    const struct intel_crtc_state *crtc_state,
			    const struct drm_dp_vsc_sdp *vsc)
{
	struct intel_digital_port *dig_port = enc_to_dig_port(encoder);
	struct drm_i915_private *dev_priv = to_i915(encoder->base.dev);
	struct dp_sdp sdp = {};
	ssize_t len;

	len = intel_dp_vsc_sdp_pack(vsc, &sdp, sizeof(sdp));

	if (drm_WARN_ON(&dev_priv->drm, len < 0))
		return;

	dig_port->write_infoframe(encoder, crtc_state, DP_SDP_VSC,
					&sdp, len);
}

void intel_dp_set_infoframes(struct intel_encoder *encoder,
			     bool enable,
			     const struct intel_crtc_state *crtc_state,
			     const struct drm_connector_state *conn_state)
{
	struct drm_i915_private *dev_priv = to_i915(encoder->base.dev);
	i915_reg_t reg = HSW_TVIDEO_DIP_CTL(crtc_state->cpu_transcoder);
	u32 dip_enable = VIDEO_DIP_ENABLE_AVI_HSW | VIDEO_DIP_ENABLE_GCP_HSW |
			 VIDEO_DIP_ENABLE_VS_HSW | VIDEO_DIP_ENABLE_GMP_HSW |
			 VIDEO_DIP_ENABLE_SPD_HSW | VIDEO_DIP_ENABLE_DRM_GLK;
	u32 val = intel_de_read(dev_priv, reg) & ~dip_enable;

	 
	 
	if (!crtc_state->has_psr)
		val &= ~VIDEO_DIP_ENABLE_VSC_HSW;

	intel_de_write(dev_priv, reg, val);
	intel_de_posting_read(dev_priv, reg);

	if (!enable)
		return;

	 
	if (!crtc_state->has_psr)
		intel_write_dp_sdp(encoder, crtc_state, DP_SDP_VSC);

	intel_write_dp_sdp(encoder, crtc_state, HDMI_PACKET_TYPE_GAMUT_METADATA);
}

static int intel_dp_vsc_sdp_unpack(struct drm_dp_vsc_sdp *vsc,
				   const void *buffer, size_t size)
{
	const struct dp_sdp *sdp = buffer;

	if (size < sizeof(struct dp_sdp))
		return -EINVAL;

	memset(vsc, 0, sizeof(*vsc));

	if (sdp->sdp_header.HB0 != 0)
		return -EINVAL;

	if (sdp->sdp_header.HB1 != DP_SDP_VSC)
		return -EINVAL;

	vsc->sdp_type = sdp->sdp_header.HB1;
	vsc->revision = sdp->sdp_header.HB2;
	vsc->length = sdp->sdp_header.HB3;

	if ((sdp->sdp_header.HB2 == 0x2 && sdp->sdp_header.HB3 == 0x8) ||
	    (sdp->sdp_header.HB2 == 0x4 && sdp->sdp_header.HB3 == 0xe)) {
		 
		return 0;
	} else if (sdp->sdp_header.HB2 == 0x5 && sdp->sdp_header.HB3 == 0x13) {
		 
		vsc->pixelformat = (sdp->db[16] >> 4) & 0xf;
		vsc->colorimetry = sdp->db[16] & 0xf;
		vsc->dynamic_range = (sdp->db[17] >> 7) & 0x1;

		switch (sdp->db[17] & 0x7) {
		case 0x0:
			vsc->bpc = 6;
			break;
		case 0x1:
			vsc->bpc = 8;
			break;
		case 0x2:
			vsc->bpc = 10;
			break;
		case 0x3:
			vsc->bpc = 12;
			break;
		case 0x4:
			vsc->bpc = 16;
			break;
		default:
			MISSING_CASE(sdp->db[17] & 0x7);
			return -EINVAL;
		}

		vsc->content_type = sdp->db[18] & 0x7;
	} else {
		return -EINVAL;
	}

	return 0;
}

static int
intel_dp_hdr_metadata_infoframe_sdp_unpack(struct hdmi_drm_infoframe *drm_infoframe,
					   const void *buffer, size_t size)
{
	int ret;

	const struct dp_sdp *sdp = buffer;

	if (size < sizeof(struct dp_sdp))
		return -EINVAL;

	if (sdp->sdp_header.HB0 != 0)
		return -EINVAL;

	if (sdp->sdp_header.HB1 != HDMI_INFOFRAME_TYPE_DRM)
		return -EINVAL;

	 
	if (sdp->sdp_header.HB2 != 0x1D)
		return -EINVAL;

	 
	if ((sdp->sdp_header.HB3 & 0x3) != 0)
		return -EINVAL;

	 
	if (((sdp->sdp_header.HB3 >> 2) & 0x3f) != 0x13)
		return -EINVAL;

	 
	if (sdp->db[0] != 1)
		return -EINVAL;

	 
	if (sdp->db[1] != HDMI_DRM_INFOFRAME_SIZE)
		return -EINVAL;

	ret = hdmi_drm_infoframe_unpack_only(drm_infoframe, &sdp->db[2],
					     HDMI_DRM_INFOFRAME_SIZE);

	return ret;
}

static void intel_read_dp_vsc_sdp(struct intel_encoder *encoder,
				  struct intel_crtc_state *crtc_state,
				  struct drm_dp_vsc_sdp *vsc)
{
	struct intel_digital_port *dig_port = enc_to_dig_port(encoder);
	struct drm_i915_private *dev_priv = to_i915(encoder->base.dev);
	unsigned int type = DP_SDP_VSC;
	struct dp_sdp sdp = {};
	int ret;

	 
	if (crtc_state->has_psr)
		return;

	if ((crtc_state->infoframes.enable &
	     intel_hdmi_infoframe_enable(type)) == 0)
		return;

	dig_port->read_infoframe(encoder, crtc_state, type, &sdp, sizeof(sdp));

	ret = intel_dp_vsc_sdp_unpack(vsc, &sdp, sizeof(sdp));

	if (ret)
		drm_dbg_kms(&dev_priv->drm, "Failed to unpack DP VSC SDP\n");
}

static void intel_read_dp_hdr_metadata_infoframe_sdp(struct intel_encoder *encoder,
						     struct intel_crtc_state *crtc_state,
						     struct hdmi_drm_infoframe *drm_infoframe)
{
	struct intel_digital_port *dig_port = enc_to_dig_port(encoder);
	struct drm_i915_private *dev_priv = to_i915(encoder->base.dev);
	unsigned int type = HDMI_PACKET_TYPE_GAMUT_METADATA;
	struct dp_sdp sdp = {};
	int ret;

	if ((crtc_state->infoframes.enable &
	    intel_hdmi_infoframe_enable(type)) == 0)
		return;

	dig_port->read_infoframe(encoder, crtc_state, type, &sdp,
				 sizeof(sdp));

	ret = intel_dp_hdr_metadata_infoframe_sdp_unpack(drm_infoframe, &sdp,
							 sizeof(sdp));

	if (ret)
		drm_dbg_kms(&dev_priv->drm,
			    "Failed to unpack DP HDR Metadata Infoframe SDP\n");
}

void intel_read_dp_sdp(struct intel_encoder *encoder,
		       struct intel_crtc_state *crtc_state,
		       unsigned int type)
{
	switch (type) {
	case DP_SDP_VSC:
		intel_read_dp_vsc_sdp(encoder, crtc_state,
				      &crtc_state->infoframes.vsc);
		break;
	case HDMI_PACKET_TYPE_GAMUT_METADATA:
		intel_read_dp_hdr_metadata_infoframe_sdp(encoder, crtc_state,
							 &crtc_state->infoframes.drm.drm);
		break;
	default:
		MISSING_CASE(type);
		break;
	}
}

static u8 intel_dp_autotest_link_training(struct intel_dp *intel_dp)
{
	struct drm_i915_private *i915 = dp_to_i915(intel_dp);
	int status = 0;
	int test_link_rate;
	u8 test_lane_count, test_link_bw;
	 
	 
	status = drm_dp_dpcd_readb(&intel_dp->aux, DP_TEST_LANE_COUNT,
				   &test_lane_count);

	if (status <= 0) {
		drm_dbg_kms(&i915->drm, "Lane count read failed\n");
		return DP_TEST_NAK;
	}
	test_lane_count &= DP_MAX_LANE_COUNT_MASK;

	status = drm_dp_dpcd_readb(&intel_dp->aux, DP_TEST_LINK_RATE,
				   &test_link_bw);
	if (status <= 0) {
		drm_dbg_kms(&i915->drm, "Link Rate read failed\n");
		return DP_TEST_NAK;
	}
	test_link_rate = drm_dp_bw_code_to_link_rate(test_link_bw);

	 
	if (!intel_dp_link_params_valid(intel_dp, test_link_rate,
					test_lane_count))
		return DP_TEST_NAK;

	intel_dp->compliance.test_lane_count = test_lane_count;
	intel_dp->compliance.test_link_rate = test_link_rate;

	return DP_TEST_ACK;
}

static u8 intel_dp_autotest_video_pattern(struct intel_dp *intel_dp)
{
	struct drm_i915_private *i915 = dp_to_i915(intel_dp);
	u8 test_pattern;
	u8 test_misc;
	__be16 h_width, v_height;
	int status = 0;

	 
	status = drm_dp_dpcd_readb(&intel_dp->aux, DP_TEST_PATTERN,
				   &test_pattern);
	if (status <= 0) {
		drm_dbg_kms(&i915->drm, "Test pattern read failed\n");
		return DP_TEST_NAK;
	}
	if (test_pattern != DP_COLOR_RAMP)
		return DP_TEST_NAK;

	status = drm_dp_dpcd_read(&intel_dp->aux, DP_TEST_H_WIDTH_HI,
				  &h_width, 2);
	if (status <= 0) {
		drm_dbg_kms(&i915->drm, "H Width read failed\n");
		return DP_TEST_NAK;
	}

	status = drm_dp_dpcd_read(&intel_dp->aux, DP_TEST_V_HEIGHT_HI,
				  &v_height, 2);
	if (status <= 0) {
		drm_dbg_kms(&i915->drm, "V Height read failed\n");
		return DP_TEST_NAK;
	}

	status = drm_dp_dpcd_readb(&intel_dp->aux, DP_TEST_MISC0,
				   &test_misc);
	if (status <= 0) {
		drm_dbg_kms(&i915->drm, "TEST MISC read failed\n");
		return DP_TEST_NAK;
	}
	if ((test_misc & DP_TEST_COLOR_FORMAT_MASK) != DP_COLOR_FORMAT_RGB)
		return DP_TEST_NAK;
	if (test_misc & DP_TEST_DYNAMIC_RANGE_CEA)
		return DP_TEST_NAK;
	switch (test_misc & DP_TEST_BIT_DEPTH_MASK) {
	case DP_TEST_BIT_DEPTH_6:
		intel_dp->compliance.test_data.bpc = 6;
		break;
	case DP_TEST_BIT_DEPTH_8:
		intel_dp->compliance.test_data.bpc = 8;
		break;
	default:
		return DP_TEST_NAK;
	}

	intel_dp->compliance.test_data.video_pattern = test_pattern;
	intel_dp->compliance.test_data.hdisplay = be16_to_cpu(h_width);
	intel_dp->compliance.test_data.vdisplay = be16_to_cpu(v_height);
	 
	intel_dp->compliance.test_active = true;

	return DP_TEST_ACK;
}

static u8 intel_dp_autotest_edid(struct intel_dp *intel_dp)
{
	struct drm_i915_private *i915 = dp_to_i915(intel_dp);
	u8 test_result = DP_TEST_ACK;
	struct intel_connector *intel_connector = intel_dp->attached_connector;
	struct drm_connector *connector = &intel_connector->base;

	if (intel_connector->detect_edid == NULL ||
	    connector->edid_corrupt ||
	    intel_dp->aux.i2c_defer_count > 6) {
		 
		if (intel_dp->aux.i2c_nack_count > 0 ||
			intel_dp->aux.i2c_defer_count > 0)
			drm_dbg_kms(&i915->drm,
				    "EDID read had %d NACKs, %d DEFERs\n",
				    intel_dp->aux.i2c_nack_count,
				    intel_dp->aux.i2c_defer_count);
		intel_dp->compliance.test_data.edid = INTEL_DP_RESOLUTION_FAILSAFE;
	} else {
		 
		const struct edid *block = drm_edid_raw(intel_connector->detect_edid);

		 
		block += block->extensions;

		if (drm_dp_dpcd_writeb(&intel_dp->aux, DP_TEST_EDID_CHECKSUM,
				       block->checksum) <= 0)
			drm_dbg_kms(&i915->drm,
				    "Failed to write EDID checksum\n");

		test_result = DP_TEST_ACK | DP_TEST_EDID_CHECKSUM_WRITE;
		intel_dp->compliance.test_data.edid = INTEL_DP_RESOLUTION_PREFERRED;
	}

	 
	intel_dp->compliance.test_active = true;

	return test_result;
}

static void intel_dp_phy_pattern_update(struct intel_dp *intel_dp,
					const struct intel_crtc_state *crtc_state)
{
	struct drm_i915_private *dev_priv =
			to_i915(dp_to_dig_port(intel_dp)->base.base.dev);
	struct drm_dp_phy_test_params *data =
			&intel_dp->compliance.test_data.phytest;
	struct intel_crtc *crtc = to_intel_crtc(crtc_state->uapi.crtc);
	enum pipe pipe = crtc->pipe;
	u32 pattern_val;

	switch (data->phy_pattern) {
	case DP_PHY_TEST_PATTERN_NONE:
		drm_dbg_kms(&dev_priv->drm, "Disable Phy Test Pattern\n");
		intel_de_write(dev_priv, DDI_DP_COMP_CTL(pipe), 0x0);
		break;
	case DP_PHY_TEST_PATTERN_D10_2:
		drm_dbg_kms(&dev_priv->drm, "Set D10.2 Phy Test Pattern\n");
		intel_de_write(dev_priv, DDI_DP_COMP_CTL(pipe),
			       DDI_DP_COMP_CTL_ENABLE | DDI_DP_COMP_CTL_D10_2);
		break;
	case DP_PHY_TEST_PATTERN_ERROR_COUNT:
		drm_dbg_kms(&dev_priv->drm, "Set Error Count Phy Test Pattern\n");
		intel_de_write(dev_priv, DDI_DP_COMP_CTL(pipe),
			       DDI_DP_COMP_CTL_ENABLE |
			       DDI_DP_COMP_CTL_SCRAMBLED_0);
		break;
	case DP_PHY_TEST_PATTERN_PRBS7:
		drm_dbg_kms(&dev_priv->drm, "Set PRBS7 Phy Test Pattern\n");
		intel_de_write(dev_priv, DDI_DP_COMP_CTL(pipe),
			       DDI_DP_COMP_CTL_ENABLE | DDI_DP_COMP_CTL_PRBS7);
		break;
	case DP_PHY_TEST_PATTERN_80BIT_CUSTOM:
		 
		drm_dbg_kms(&dev_priv->drm,
			    "Set 80Bit Custom Phy Test Pattern 0x3e0f83e0 0x0f83e0f8 0x0000f83e\n");
		pattern_val = 0x3e0f83e0;
		intel_de_write(dev_priv, DDI_DP_COMP_PAT(pipe, 0), pattern_val);
		pattern_val = 0x0f83e0f8;
		intel_de_write(dev_priv, DDI_DP_COMP_PAT(pipe, 1), pattern_val);
		pattern_val = 0x0000f83e;
		intel_de_write(dev_priv, DDI_DP_COMP_PAT(pipe, 2), pattern_val);
		intel_de_write(dev_priv, DDI_DP_COMP_CTL(pipe),
			       DDI_DP_COMP_CTL_ENABLE |
			       DDI_DP_COMP_CTL_CUSTOM80);
		break;
	case DP_PHY_TEST_PATTERN_CP2520:
		 
		drm_dbg_kms(&dev_priv->drm, "Set HBR2 compliance Phy Test Pattern\n");
		pattern_val = 0xFB;
		intel_de_write(dev_priv, DDI_DP_COMP_CTL(pipe),
			       DDI_DP_COMP_CTL_ENABLE | DDI_DP_COMP_CTL_HBR2 |
			       pattern_val);
		break;
	default:
		WARN(1, "Invalid Phy Test Pattern\n");
	}
}

static void intel_dp_process_phy_request(struct intel_dp *intel_dp,
					 const struct intel_crtc_state *crtc_state)
{
	struct drm_i915_private *i915 = dp_to_i915(intel_dp);
	struct drm_dp_phy_test_params *data =
		&intel_dp->compliance.test_data.phytest;
	u8 link_status[DP_LINK_STATUS_SIZE];

	if (drm_dp_dpcd_read_phy_link_status(&intel_dp->aux, DP_PHY_DPRX,
					     link_status) < 0) {
		drm_dbg_kms(&i915->drm, "failed to get link status\n");
		return;
	}

	 
	intel_dp_get_adjust_train(intel_dp, crtc_state, DP_PHY_DPRX,
				  link_status);

	intel_dp_set_signal_levels(intel_dp, crtc_state, DP_PHY_DPRX);

	intel_dp_phy_pattern_update(intel_dp, crtc_state);

	drm_dp_dpcd_write(&intel_dp->aux, DP_TRAINING_LANE0_SET,
			  intel_dp->train_set, crtc_state->lane_count);

	drm_dp_set_phy_test_pattern(&intel_dp->aux, data,
				    intel_dp->dpcd[DP_DPCD_REV]);
}

static u8 intel_dp_autotest_phy_pattern(struct intel_dp *intel_dp)
{
	struct drm_i915_private *i915 = dp_to_i915(intel_dp);
	struct drm_dp_phy_test_params *data =
		&intel_dp->compliance.test_data.phytest;

	if (drm_dp_get_phy_test_pattern(&intel_dp->aux, data)) {
		drm_dbg_kms(&i915->drm, "DP Phy Test pattern AUX read failure\n");
		return DP_TEST_NAK;
	}

	 
	intel_dp->compliance.test_active = true;

	return DP_TEST_ACK;
}

static void intel_dp_handle_test_request(struct intel_dp *intel_dp)
{
	struct drm_i915_private *i915 = dp_to_i915(intel_dp);
	u8 response = DP_TEST_NAK;
	u8 request = 0;
	int status;

	status = drm_dp_dpcd_readb(&intel_dp->aux, DP_TEST_REQUEST, &request);
	if (status <= 0) {
		drm_dbg_kms(&i915->drm,
			    "Could not read test request from sink\n");
		goto update_status;
	}

	switch (request) {
	case DP_TEST_LINK_TRAINING:
		drm_dbg_kms(&i915->drm, "LINK_TRAINING test requested\n");
		response = intel_dp_autotest_link_training(intel_dp);
		break;
	case DP_TEST_LINK_VIDEO_PATTERN:
		drm_dbg_kms(&i915->drm, "TEST_PATTERN test requested\n");
		response = intel_dp_autotest_video_pattern(intel_dp);
		break;
	case DP_TEST_LINK_EDID_READ:
		drm_dbg_kms(&i915->drm, "EDID test requested\n");
		response = intel_dp_autotest_edid(intel_dp);
		break;
	case DP_TEST_LINK_PHY_TEST_PATTERN:
		drm_dbg_kms(&i915->drm, "PHY_PATTERN test requested\n");
		response = intel_dp_autotest_phy_pattern(intel_dp);
		break;
	default:
		drm_dbg_kms(&i915->drm, "Invalid test request '%02x'\n",
			    request);
		break;
	}

	if (response & DP_TEST_ACK)
		intel_dp->compliance.test_type = request;

update_status:
	status = drm_dp_dpcd_writeb(&intel_dp->aux, DP_TEST_RESPONSE, response);
	if (status <= 0)
		drm_dbg_kms(&i915->drm,
			    "Could not write test response to sink\n");
}

static bool intel_dp_link_ok(struct intel_dp *intel_dp,
			     u8 link_status[DP_LINK_STATUS_SIZE])
{
	struct intel_encoder *encoder = &dp_to_dig_port(intel_dp)->base;
	struct drm_i915_private *i915 = to_i915(encoder->base.dev);
	bool uhbr = intel_dp->link_rate >= 1000000;
	bool ok;

	if (uhbr)
		ok = drm_dp_128b132b_lane_channel_eq_done(link_status,
							  intel_dp->lane_count);
	else
		ok = drm_dp_channel_eq_ok(link_status, intel_dp->lane_count);

	if (ok)
		return true;

	intel_dp_dump_link_status(intel_dp, DP_PHY_DPRX, link_status);
	drm_dbg_kms(&i915->drm,
		    "[ENCODER:%d:%s] %s link not ok, retraining\n",
		    encoder->base.base.id, encoder->base.name,
		    uhbr ? "128b/132b" : "8b/10b");

	return false;
}

static void
intel_dp_mst_hpd_irq(struct intel_dp *intel_dp, u8 *esi, u8 *ack)
{
	bool handled = false;

	drm_dp_mst_hpd_irq_handle_event(&intel_dp->mst_mgr, esi, ack, &handled);

	if (esi[1] & DP_CP_IRQ) {
		intel_hdcp_handle_cp_irq(intel_dp->attached_connector);
		ack[1] |= DP_CP_IRQ;
	}
}

static bool intel_dp_mst_link_status(struct intel_dp *intel_dp)
{
	struct intel_encoder *encoder = &dp_to_dig_port(intel_dp)->base;
	struct drm_i915_private *i915 = to_i915(encoder->base.dev);
	u8 link_status[DP_LINK_STATUS_SIZE] = {};
	const size_t esi_link_status_size = DP_LINK_STATUS_SIZE - 2;

	if (drm_dp_dpcd_read(&intel_dp->aux, DP_LANE0_1_STATUS_ESI, link_status,
			     esi_link_status_size) != esi_link_status_size) {
		drm_err(&i915->drm,
			"[ENCODER:%d:%s] Failed to read link status\n",
			encoder->base.base.id, encoder->base.name);
		return false;
	}

	return intel_dp_link_ok(intel_dp, link_status);
}

 
static bool
intel_dp_check_mst_status(struct intel_dp *intel_dp)
{
	struct drm_i915_private *i915 = dp_to_i915(intel_dp);
	bool link_ok = true;

	drm_WARN_ON_ONCE(&i915->drm, intel_dp->active_mst_links < 0);

	for (;;) {
		u8 esi[4] = {};
		u8 ack[4] = {};

		if (!intel_dp_get_sink_irq_esi(intel_dp, esi)) {
			drm_dbg_kms(&i915->drm,
				    "failed to get ESI - device may have failed\n");
			link_ok = false;

			break;
		}

		drm_dbg_kms(&i915->drm, "DPRX ESI: %4ph\n", esi);

		if (intel_dp->active_mst_links > 0 && link_ok &&
		    esi[3] & LINK_STATUS_CHANGED) {
			if (!intel_dp_mst_link_status(intel_dp))
				link_ok = false;
			ack[3] |= LINK_STATUS_CHANGED;
		}

		intel_dp_mst_hpd_irq(intel_dp, esi, ack);

		if (!memchr_inv(ack, 0, sizeof(ack)))
			break;

		if (!intel_dp_ack_sink_irq_esi(intel_dp, ack))
			drm_dbg_kms(&i915->drm, "Failed to ack ESI\n");

		if (ack[1] & (DP_DOWN_REP_MSG_RDY | DP_UP_REQ_MSG_RDY))
			drm_dp_mst_hpd_irq_send_new_request(&intel_dp->mst_mgr);
	}

	return link_ok;
}

static void
intel_dp_handle_hdmi_link_status_change(struct intel_dp *intel_dp)
{
	bool is_active;
	u8 buf = 0;

	is_active = drm_dp_pcon_hdmi_link_active(&intel_dp->aux);
	if (intel_dp->frl.is_trained && !is_active) {
		if (drm_dp_dpcd_readb(&intel_dp->aux, DP_PCON_HDMI_LINK_CONFIG_1, &buf) < 0)
			return;

		buf &=  ~DP_PCON_ENABLE_HDMI_LINK;
		if (drm_dp_dpcd_writeb(&intel_dp->aux, DP_PCON_HDMI_LINK_CONFIG_1, buf) < 0)
			return;

		drm_dp_pcon_hdmi_frl_link_error_count(&intel_dp->aux, &intel_dp->attached_connector->base);

		intel_dp->frl.is_trained = false;

		 
		intel_dp_check_frl_training(intel_dp);
	}
}

static bool
intel_dp_needs_link_retrain(struct intel_dp *intel_dp)
{
	u8 link_status[DP_LINK_STATUS_SIZE];

	if (!intel_dp->link_trained)
		return false;

	 
	if (intel_psr_enabled(intel_dp))
		return false;

	if (drm_dp_dpcd_read_phy_link_status(&intel_dp->aux, DP_PHY_DPRX,
					     link_status) < 0)
		return false;

	 
	if (!intel_dp_link_params_valid(intel_dp, intel_dp->link_rate,
					intel_dp->lane_count))
		return false;

	 
	return !intel_dp_link_ok(intel_dp, link_status);
}

static bool intel_dp_has_connector(struct intel_dp *intel_dp,
				   const struct drm_connector_state *conn_state)
{
	struct drm_i915_private *i915 = dp_to_i915(intel_dp);
	struct intel_encoder *encoder;
	enum pipe pipe;

	if (!conn_state->best_encoder)
		return false;

	 
	encoder = &dp_to_dig_port(intel_dp)->base;
	if (conn_state->best_encoder == &encoder->base)
		return true;

	 
	for_each_pipe(i915, pipe) {
		encoder = &intel_dp->mst_encoders[pipe]->base;
		if (conn_state->best_encoder == &encoder->base)
			return true;
	}

	return false;
}

int intel_dp_get_active_pipes(struct intel_dp *intel_dp,
			      struct drm_modeset_acquire_ctx *ctx,
			      u8 *pipe_mask)
{
	struct drm_i915_private *i915 = dp_to_i915(intel_dp);
	struct drm_connector_list_iter conn_iter;
	struct intel_connector *connector;
	int ret = 0;

	*pipe_mask = 0;

	drm_connector_list_iter_begin(&i915->drm, &conn_iter);
	for_each_intel_connector_iter(connector, &conn_iter) {
		struct drm_connector_state *conn_state =
			connector->base.state;
		struct intel_crtc_state *crtc_state;
		struct intel_crtc *crtc;

		if (!intel_dp_has_connector(intel_dp, conn_state))
			continue;

		crtc = to_intel_crtc(conn_state->crtc);
		if (!crtc)
			continue;

		ret = drm_modeset_lock(&crtc->base.mutex, ctx);
		if (ret)
			break;

		crtc_state = to_intel_crtc_state(crtc->base.state);

		drm_WARN_ON(&i915->drm, !intel_crtc_has_dp_encoder(crtc_state));

		if (!crtc_state->hw.active)
			continue;

		if (conn_state->commit &&
		    !try_wait_for_completion(&conn_state->commit->hw_done))
			continue;

		*pipe_mask |= BIT(crtc->pipe);
	}
	drm_connector_list_iter_end(&conn_iter);

	return ret;
}

static bool intel_dp_is_connected(struct intel_dp *intel_dp)
{
	struct intel_connector *connector = intel_dp->attached_connector;

	return connector->base.status == connector_status_connected ||
		intel_dp->is_mst;
}

int intel_dp_retrain_link(struct intel_encoder *encoder,
			  struct drm_modeset_acquire_ctx *ctx)
{
	struct drm_i915_private *dev_priv = to_i915(encoder->base.dev);
	struct intel_dp *intel_dp = enc_to_intel_dp(encoder);
	struct intel_crtc *crtc;
	u8 pipe_mask;
	int ret;

	if (!intel_dp_is_connected(intel_dp))
		return 0;

	ret = drm_modeset_lock(&dev_priv->drm.mode_config.connection_mutex,
			       ctx);
	if (ret)
		return ret;

	if (!intel_dp_needs_link_retrain(intel_dp))
		return 0;

	ret = intel_dp_get_active_pipes(intel_dp, ctx, &pipe_mask);
	if (ret)
		return ret;

	if (pipe_mask == 0)
		return 0;

	if (!intel_dp_needs_link_retrain(intel_dp))
		return 0;

	drm_dbg_kms(&dev_priv->drm, "[ENCODER:%d:%s] retraining link\n",
		    encoder->base.base.id, encoder->base.name);

	for_each_intel_crtc_in_pipe_mask(&dev_priv->drm, crtc, pipe_mask) {
		const struct intel_crtc_state *crtc_state =
			to_intel_crtc_state(crtc->base.state);

		 
		intel_set_cpu_fifo_underrun_reporting(dev_priv, crtc->pipe, false);
		if (crtc_state->has_pch_encoder)
			intel_set_pch_fifo_underrun_reporting(dev_priv,
							      intel_crtc_pch_transcoder(crtc), false);
	}

	for_each_intel_crtc_in_pipe_mask(&dev_priv->drm, crtc, pipe_mask) {
		const struct intel_crtc_state *crtc_state =
			to_intel_crtc_state(crtc->base.state);

		 
		if (DISPLAY_VER(dev_priv) >= 12 &&
		    intel_crtc_has_type(crtc_state, INTEL_OUTPUT_DP_MST) &&
		    !intel_dp_mst_is_master_trans(crtc_state))
			continue;

		intel_dp_check_frl_training(intel_dp);
		intel_dp_pcon_dsc_configure(intel_dp, crtc_state);
		intel_dp_start_link_train(intel_dp, crtc_state);
		intel_dp_stop_link_train(intel_dp, crtc_state);
		break;
	}

	for_each_intel_crtc_in_pipe_mask(&dev_priv->drm, crtc, pipe_mask) {
		const struct intel_crtc_state *crtc_state =
			to_intel_crtc_state(crtc->base.state);

		 
		intel_crtc_wait_for_next_vblank(crtc);

		intel_set_cpu_fifo_underrun_reporting(dev_priv, crtc->pipe, true);
		if (crtc_state->has_pch_encoder)
			intel_set_pch_fifo_underrun_reporting(dev_priv,
							      intel_crtc_pch_transcoder(crtc), true);
	}

	return 0;
}

static int intel_dp_prep_phy_test(struct intel_dp *intel_dp,
				  struct drm_modeset_acquire_ctx *ctx,
				  u8 *pipe_mask)
{
	struct drm_i915_private *i915 = dp_to_i915(intel_dp);
	struct drm_connector_list_iter conn_iter;
	struct intel_connector *connector;
	int ret = 0;

	*pipe_mask = 0;

	drm_connector_list_iter_begin(&i915->drm, &conn_iter);
	for_each_intel_connector_iter(connector, &conn_iter) {
		struct drm_connector_state *conn_state =
			connector->base.state;
		struct intel_crtc_state *crtc_state;
		struct intel_crtc *crtc;

		if (!intel_dp_has_connector(intel_dp, conn_state))
			continue;

		crtc = to_intel_crtc(conn_state->crtc);
		if (!crtc)
			continue;

		ret = drm_modeset_lock(&crtc->base.mutex, ctx);
		if (ret)
			break;

		crtc_state = to_intel_crtc_state(crtc->base.state);

		drm_WARN_ON(&i915->drm, !intel_crtc_has_dp_encoder(crtc_state));

		if (!crtc_state->hw.active)
			continue;

		if (conn_state->commit &&
		    !try_wait_for_completion(&conn_state->commit->hw_done))
			continue;

		*pipe_mask |= BIT(crtc->pipe);
	}
	drm_connector_list_iter_end(&conn_iter);

	return ret;
}

static int intel_dp_do_phy_test(struct intel_encoder *encoder,
				struct drm_modeset_acquire_ctx *ctx)
{
	struct drm_i915_private *dev_priv = to_i915(encoder->base.dev);
	struct intel_dp *intel_dp = enc_to_intel_dp(encoder);
	struct intel_crtc *crtc;
	u8 pipe_mask;
	int ret;

	ret = drm_modeset_lock(&dev_priv->drm.mode_config.connection_mutex,
			       ctx);
	if (ret)
		return ret;

	ret = intel_dp_prep_phy_test(intel_dp, ctx, &pipe_mask);
	if (ret)
		return ret;

	if (pipe_mask == 0)
		return 0;

	drm_dbg_kms(&dev_priv->drm, "[ENCODER:%d:%s] PHY test\n",
		    encoder->base.base.id, encoder->base.name);

	for_each_intel_crtc_in_pipe_mask(&dev_priv->drm, crtc, pipe_mask) {
		const struct intel_crtc_state *crtc_state =
			to_intel_crtc_state(crtc->base.state);

		 
		if (DISPLAY_VER(dev_priv) >= 12 &&
		    intel_crtc_has_type(crtc_state, INTEL_OUTPUT_DP_MST) &&
		    !intel_dp_mst_is_master_trans(crtc_state))
			continue;

		intel_dp_process_phy_request(intel_dp, crtc_state);
		break;
	}

	return 0;
}

void intel_dp_phy_test(struct intel_encoder *encoder)
{
	struct drm_modeset_acquire_ctx ctx;
	int ret;

	drm_modeset_acquire_init(&ctx, 0);

	for (;;) {
		ret = intel_dp_do_phy_test(encoder, &ctx);

		if (ret == -EDEADLK) {
			drm_modeset_backoff(&ctx);
			continue;
		}

		break;
	}

	drm_modeset_drop_locks(&ctx);
	drm_modeset_acquire_fini(&ctx);
	drm_WARN(encoder->base.dev, ret,
		 "Acquiring modeset locks failed with %i\n", ret);
}

static void intel_dp_check_device_service_irq(struct intel_dp *intel_dp)
{
	struct drm_i915_private *i915 = dp_to_i915(intel_dp);
	u8 val;

	if (intel_dp->dpcd[DP_DPCD_REV] < 0x11)
		return;

	if (drm_dp_dpcd_readb(&intel_dp->aux,
			      DP_DEVICE_SERVICE_IRQ_VECTOR, &val) != 1 || !val)
		return;

	drm_dp_dpcd_writeb(&intel_dp->aux, DP_DEVICE_SERVICE_IRQ_VECTOR, val);

	if (val & DP_AUTOMATED_TEST_REQUEST)
		intel_dp_handle_test_request(intel_dp);

	if (val & DP_CP_IRQ)
		intel_hdcp_handle_cp_irq(intel_dp->attached_connector);

	if (val & DP_SINK_SPECIFIC_IRQ)
		drm_dbg_kms(&i915->drm, "Sink specific irq unhandled\n");
}

static void intel_dp_check_link_service_irq(struct intel_dp *intel_dp)
{
	u8 val;

	if (intel_dp->dpcd[DP_DPCD_REV] < 0x11)
		return;

	if (drm_dp_dpcd_readb(&intel_dp->aux,
			      DP_LINK_SERVICE_IRQ_VECTOR_ESI0, &val) != 1 || !val)
		return;

	if (drm_dp_dpcd_writeb(&intel_dp->aux,
			       DP_LINK_SERVICE_IRQ_VECTOR_ESI0, val) != 1)
		return;

	if (val & HDMI_LINK_STATUS_CHANGED)
		intel_dp_handle_hdmi_link_status_change(intel_dp);
}

 
static bool
intel_dp_short_pulse(struct intel_dp *intel_dp)
{
	struct drm_i915_private *dev_priv = dp_to_i915(intel_dp);
	u8 old_sink_count = intel_dp->sink_count;
	bool ret;

	 
	memset(&intel_dp->compliance, 0, sizeof(intel_dp->compliance));

	 
	ret = intel_dp_get_dpcd(intel_dp);

	if ((old_sink_count != intel_dp->sink_count) || !ret) {
		 
		return false;
	}

	intel_dp_check_device_service_irq(intel_dp);
	intel_dp_check_link_service_irq(intel_dp);

	 
	drm_dp_cec_irq(&intel_dp->aux);

	 
	if (intel_dp_needs_link_retrain(intel_dp))
		return false;

	intel_psr_short_pulse(intel_dp);

	switch (intel_dp->compliance.test_type) {
	case DP_TEST_LINK_TRAINING:
		drm_dbg_kms(&dev_priv->drm,
			    "Link Training Compliance Test requested\n");
		 
		drm_kms_helper_hotplug_event(&dev_priv->drm);
		break;
	case DP_TEST_LINK_PHY_TEST_PATTERN:
		drm_dbg_kms(&dev_priv->drm,
			    "PHY test pattern Compliance Test requested\n");
		 
		return false;
	}

	return true;
}

 
static enum drm_connector_status
intel_dp_detect_dpcd(struct intel_dp *intel_dp)
{
	struct drm_i915_private *i915 = dp_to_i915(intel_dp);
	struct intel_digital_port *dig_port = dp_to_dig_port(intel_dp);
	u8 *dpcd = intel_dp->dpcd;
	u8 type;

	if (drm_WARN_ON(&i915->drm, intel_dp_is_edp(intel_dp)))
		return connector_status_connected;

	lspcon_resume(dig_port);

	if (!intel_dp_get_dpcd(intel_dp))
		return connector_status_disconnected;

	 
	if (!drm_dp_is_branch(dpcd))
		return connector_status_connected;

	 
	if (intel_dp_has_sink_count(intel_dp) &&
	    intel_dp->downstream_ports[0] & DP_DS_PORT_HPD) {
		return intel_dp->sink_count ?
		connector_status_connected : connector_status_disconnected;
	}

	if (intel_dp_can_mst(intel_dp))
		return connector_status_connected;

	 
	if (drm_probe_ddc(&intel_dp->aux.ddc))
		return connector_status_connected;

	 
	if (intel_dp->dpcd[DP_DPCD_REV] >= 0x11) {
		type = intel_dp->downstream_ports[0] & DP_DS_PORT_TYPE_MASK;
		if (type == DP_DS_PORT_TYPE_VGA ||
		    type == DP_DS_PORT_TYPE_NON_EDID)
			return connector_status_unknown;
	} else {
		type = intel_dp->dpcd[DP_DOWNSTREAMPORT_PRESENT] &
			DP_DWN_STRM_PORT_TYPE_MASK;
		if (type == DP_DWN_STRM_PORT_TYPE_ANALOG ||
		    type == DP_DWN_STRM_PORT_TYPE_OTHER)
			return connector_status_unknown;
	}

	 
	drm_dbg_kms(&i915->drm, "Broken DP branch device, ignoring\n");
	return connector_status_disconnected;
}

static enum drm_connector_status
edp_detect(struct intel_dp *intel_dp)
{
	return connector_status_connected;
}

 
bool intel_digital_port_connected(struct intel_encoder *encoder)
{
	struct drm_i915_private *dev_priv = to_i915(encoder->base.dev);
	struct intel_digital_port *dig_port = enc_to_dig_port(encoder);
	bool is_connected = false;
	intel_wakeref_t wakeref;

	with_intel_display_power(dev_priv, POWER_DOMAIN_DISPLAY_CORE, wakeref)
		is_connected = dig_port->connected(encoder);

	return is_connected;
}

static const struct drm_edid *
intel_dp_get_edid(struct intel_dp *intel_dp)
{
	struct intel_connector *connector = intel_dp->attached_connector;
	const struct drm_edid *fixed_edid = connector->panel.fixed_edid;

	 
	if (fixed_edid) {
		 
		if (IS_ERR(fixed_edid))
			return NULL;

		return drm_edid_dup(fixed_edid);
	}

	return drm_edid_read_ddc(&connector->base, &intel_dp->aux.ddc);
}

static void
intel_dp_update_dfp(struct intel_dp *intel_dp,
		    const struct drm_edid *drm_edid)
{
	struct drm_i915_private *i915 = dp_to_i915(intel_dp);
	struct intel_connector *connector = intel_dp->attached_connector;
	const struct edid *edid;

	 
	edid = drm_edid_raw(drm_edid);

	intel_dp->dfp.max_bpc =
		drm_dp_downstream_max_bpc(intel_dp->dpcd,
					  intel_dp->downstream_ports, edid);

	intel_dp->dfp.max_dotclock =
		drm_dp_downstream_max_dotclock(intel_dp->dpcd,
					       intel_dp->downstream_ports);

	intel_dp->dfp.min_tmds_clock =
		drm_dp_downstream_min_tmds_clock(intel_dp->dpcd,
						 intel_dp->downstream_ports,
						 edid);
	intel_dp->dfp.max_tmds_clock =
		drm_dp_downstream_max_tmds_clock(intel_dp->dpcd,
						 intel_dp->downstream_ports,
						 edid);

	intel_dp->dfp.pcon_max_frl_bw =
		drm_dp_get_pcon_max_frl_bw(intel_dp->dpcd,
					   intel_dp->downstream_ports);

	drm_dbg_kms(&i915->drm,
		    "[CONNECTOR:%d:%s] DFP max bpc %d, max dotclock %d, TMDS clock %d-%d, PCON Max FRL BW %dGbps\n",
		    connector->base.base.id, connector->base.name,
		    intel_dp->dfp.max_bpc,
		    intel_dp->dfp.max_dotclock,
		    intel_dp->dfp.min_tmds_clock,
		    intel_dp->dfp.max_tmds_clock,
		    intel_dp->dfp.pcon_max_frl_bw);

	intel_dp_get_pcon_dsc_cap(intel_dp);
}

static bool
intel_dp_can_ycbcr420(struct intel_dp *intel_dp)
{
	if (source_can_output(intel_dp, INTEL_OUTPUT_FORMAT_YCBCR420) &&
	    (!drm_dp_is_branch(intel_dp->dpcd) || intel_dp->dfp.ycbcr420_passthrough))
		return true;

	if (source_can_output(intel_dp, INTEL_OUTPUT_FORMAT_RGB) &&
	    dfp_can_convert_from_rgb(intel_dp, INTEL_OUTPUT_FORMAT_YCBCR420))
		return true;

	if (source_can_output(intel_dp, INTEL_OUTPUT_FORMAT_YCBCR444) &&
	    dfp_can_convert_from_ycbcr444(intel_dp, INTEL_OUTPUT_FORMAT_YCBCR420))
		return true;

	return false;
}

static void
intel_dp_update_420(struct intel_dp *intel_dp)
{
	struct drm_i915_private *i915 = dp_to_i915(intel_dp);
	struct intel_connector *connector = intel_dp->attached_connector;

	intel_dp->dfp.ycbcr420_passthrough =
		drm_dp_downstream_420_passthrough(intel_dp->dpcd,
						  intel_dp->downstream_ports);
	 
	intel_dp->dfp.ycbcr_444_to_420 =
		dp_to_dig_port(intel_dp)->lspcon.active ||
		drm_dp_downstream_444_to_420_conversion(intel_dp->dpcd,
							intel_dp->downstream_ports);
	intel_dp->dfp.rgb_to_ycbcr =
		drm_dp_downstream_rgb_to_ycbcr_conversion(intel_dp->dpcd,
							  intel_dp->downstream_ports,
							  DP_DS_HDMI_BT709_RGB_YCBCR_CONV);

	connector->base.ycbcr_420_allowed = intel_dp_can_ycbcr420(intel_dp);

	drm_dbg_kms(&i915->drm,
		    "[CONNECTOR:%d:%s] RGB->YcbCr conversion? %s, YCbCr 4:2:0 allowed? %s, YCbCr 4:4:4->4:2:0 conversion? %s\n",
		    connector->base.base.id, connector->base.name,
		    str_yes_no(intel_dp->dfp.rgb_to_ycbcr),
		    str_yes_no(connector->base.ycbcr_420_allowed),
		    str_yes_no(intel_dp->dfp.ycbcr_444_to_420));
}

static void
intel_dp_set_edid(struct intel_dp *intel_dp)
{
	struct drm_i915_private *i915 = dp_to_i915(intel_dp);
	struct intel_connector *connector = intel_dp->attached_connector;
	const struct drm_edid *drm_edid;
	const struct edid *edid;
	bool vrr_capable;

	intel_dp_unset_edid(intel_dp);
	drm_edid = intel_dp_get_edid(intel_dp);
	connector->detect_edid = drm_edid;

	 
	drm_edid_connector_update(&connector->base, drm_edid);

	vrr_capable = intel_vrr_is_capable(connector);
	drm_dbg_kms(&i915->drm, "[CONNECTOR:%d:%s] VRR capable: %s\n",
		    connector->base.base.id, connector->base.name, str_yes_no(vrr_capable));
	drm_connector_set_vrr_capable_property(&connector->base, vrr_capable);

	intel_dp_update_dfp(intel_dp, drm_edid);
	intel_dp_update_420(intel_dp);

	 
	edid = drm_edid_raw(drm_edid);

	drm_dp_cec_set_edid(&intel_dp->aux, edid);
}

static void
intel_dp_unset_edid(struct intel_dp *intel_dp)
{
	struct intel_connector *connector = intel_dp->attached_connector;

	drm_dp_cec_unset_edid(&intel_dp->aux);
	drm_edid_free(connector->detect_edid);
	connector->detect_edid = NULL;

	intel_dp->dfp.max_bpc = 0;
	intel_dp->dfp.max_dotclock = 0;
	intel_dp->dfp.min_tmds_clock = 0;
	intel_dp->dfp.max_tmds_clock = 0;

	intel_dp->dfp.pcon_max_frl_bw = 0;

	intel_dp->dfp.ycbcr_444_to_420 = false;
	connector->base.ycbcr_420_allowed = false;

	drm_connector_set_vrr_capable_property(&connector->base,
					       false);
}

static int
intel_dp_detect(struct drm_connector *connector,
		struct drm_modeset_acquire_ctx *ctx,
		bool force)
{
	struct drm_i915_private *dev_priv = to_i915(connector->dev);
	struct intel_dp *intel_dp = intel_attached_dp(to_intel_connector(connector));
	struct intel_digital_port *dig_port = dp_to_dig_port(intel_dp);
	struct intel_encoder *encoder = &dig_port->base;
	enum drm_connector_status status;

	drm_dbg_kms(&dev_priv->drm, "[CONNECTOR:%d:%s]\n",
		    connector->base.id, connector->name);
	drm_WARN_ON(&dev_priv->drm,
		    !drm_modeset_is_locked(&dev_priv->drm.mode_config.connection_mutex));

	if (!INTEL_DISPLAY_ENABLED(dev_priv))
		return connector_status_disconnected;

	 
	if (intel_dp_is_edp(intel_dp))
		status = edp_detect(intel_dp);
	else if (intel_digital_port_connected(encoder))
		status = intel_dp_detect_dpcd(intel_dp);
	else
		status = connector_status_disconnected;

	if (status == connector_status_disconnected) {
		memset(&intel_dp->compliance, 0, sizeof(intel_dp->compliance));
		memset(intel_dp->dsc_dpcd, 0, sizeof(intel_dp->dsc_dpcd));

		if (intel_dp->is_mst) {
			drm_dbg_kms(&dev_priv->drm,
				    "MST device may have disappeared %d vs %d\n",
				    intel_dp->is_mst,
				    intel_dp->mst_mgr.mst_state);
			intel_dp->is_mst = false;
			drm_dp_mst_topology_mgr_set_mst(&intel_dp->mst_mgr,
							intel_dp->is_mst);
		}

		goto out;
	}

	 
	if (HAS_DSC(dev_priv))
		intel_dp_get_dsc_sink_cap(intel_dp);

	intel_dp_configure_mst(intel_dp);

	 
	if (intel_dp->reset_link_params || intel_dp->is_mst) {
		intel_dp_reset_max_link_params(intel_dp);
		intel_dp->reset_link_params = false;
	}

	intel_dp_print_rates(intel_dp);

	if (intel_dp->is_mst) {
		 
		status = connector_status_disconnected;
		goto out;
	}

	 
	if (!intel_dp_is_edp(intel_dp)) {
		int ret;

		ret = intel_dp_retrain_link(encoder, ctx);
		if (ret)
			return ret;
	}

	 
	intel_dp->aux.i2c_nack_count = 0;
	intel_dp->aux.i2c_defer_count = 0;

	intel_dp_set_edid(intel_dp);
	if (intel_dp_is_edp(intel_dp) ||
	    to_intel_connector(connector)->detect_edid)
		status = connector_status_connected;

	intel_dp_check_device_service_irq(intel_dp);

out:
	if (status != connector_status_connected && !intel_dp->is_mst)
		intel_dp_unset_edid(intel_dp);

	 
	intel_display_power_flush_work(dev_priv);

	if (!intel_dp_is_edp(intel_dp))
		drm_dp_set_subconnector_property(connector,
						 status,
						 intel_dp->dpcd,
						 intel_dp->downstream_ports);
	return status;
}

static void
intel_dp_force(struct drm_connector *connector)
{
	struct intel_dp *intel_dp = intel_attached_dp(to_intel_connector(connector));
	struct intel_digital_port *dig_port = dp_to_dig_port(intel_dp);
	struct intel_encoder *intel_encoder = &dig_port->base;
	struct drm_i915_private *dev_priv = to_i915(intel_encoder->base.dev);
	enum intel_display_power_domain aux_domain =
		intel_aux_power_domain(dig_port);
	intel_wakeref_t wakeref;

	drm_dbg_kms(&dev_priv->drm, "[CONNECTOR:%d:%s]\n",
		    connector->base.id, connector->name);
	intel_dp_unset_edid(intel_dp);

	if (connector->status != connector_status_connected)
		return;

	wakeref = intel_display_power_get(dev_priv, aux_domain);

	intel_dp_set_edid(intel_dp);

	intel_display_power_put(dev_priv, aux_domain, wakeref);
}

static int intel_dp_get_modes(struct drm_connector *connector)
{
	struct intel_connector *intel_connector = to_intel_connector(connector);
	int num_modes;

	 
	num_modes = drm_edid_connector_add_modes(connector);

	 
	if (intel_dp_is_edp(intel_attached_dp(intel_connector)))
		num_modes += intel_panel_get_modes(intel_connector);

	if (num_modes)
		return num_modes;

	if (!intel_connector->detect_edid) {
		struct intel_dp *intel_dp = intel_attached_dp(intel_connector);
		struct drm_display_mode *mode;

		mode = drm_dp_downstream_mode(connector->dev,
					      intel_dp->dpcd,
					      intel_dp->downstream_ports);
		if (mode) {
			drm_mode_probed_add(connector, mode);
			num_modes++;
		}
	}

	return num_modes;
}

static int
intel_dp_connector_register(struct drm_connector *connector)
{
	struct drm_i915_private *i915 = to_i915(connector->dev);
	struct intel_dp *intel_dp = intel_attached_dp(to_intel_connector(connector));
	struct intel_digital_port *dig_port = dp_to_dig_port(intel_dp);
	struct intel_lspcon *lspcon = &dig_port->lspcon;
	int ret;

	ret = intel_connector_register(connector);
	if (ret)
		return ret;

	drm_dbg_kms(&i915->drm, "registering %s bus for %s\n",
		    intel_dp->aux.name, connector->kdev->kobj.name);

	intel_dp->aux.dev = connector->kdev;
	ret = drm_dp_aux_register(&intel_dp->aux);
	if (!ret)
		drm_dp_cec_register_connector(&intel_dp->aux, connector);

	if (!intel_bios_encoder_is_lspcon(dig_port->base.devdata))
		return ret;

	 
	if (lspcon_init(dig_port)) {
		lspcon_detect_hdr_capability(lspcon);
		if (lspcon->hdr_supported)
			drm_connector_attach_hdr_output_metadata_property(connector);
	}

	return ret;
}

static void
intel_dp_connector_unregister(struct drm_connector *connector)
{
	struct intel_dp *intel_dp = intel_attached_dp(to_intel_connector(connector));

	drm_dp_cec_unregister_connector(&intel_dp->aux);
	drm_dp_aux_unregister(&intel_dp->aux);
	intel_connector_unregister(connector);
}

void intel_dp_encoder_flush_work(struct drm_encoder *encoder)
{
	struct intel_digital_port *dig_port = enc_to_dig_port(to_intel_encoder(encoder));
	struct intel_dp *intel_dp = &dig_port->dp;

	intel_dp_mst_encoder_cleanup(dig_port);

	intel_pps_vdd_off_sync(intel_dp);

	 
	intel_pps_wait_power_cycle(intel_dp);

	intel_dp_aux_fini(intel_dp);
}

void intel_dp_encoder_suspend(struct intel_encoder *intel_encoder)
{
	struct intel_dp *intel_dp = enc_to_intel_dp(intel_encoder);

	intel_pps_vdd_off_sync(intel_dp);
}

void intel_dp_encoder_shutdown(struct intel_encoder *intel_encoder)
{
	struct intel_dp *intel_dp = enc_to_intel_dp(intel_encoder);

	intel_pps_wait_power_cycle(intel_dp);
}

static int intel_modeset_tile_group(struct intel_atomic_state *state,
				    int tile_group_id)
{
	struct drm_i915_private *dev_priv = to_i915(state->base.dev);
	struct drm_connector_list_iter conn_iter;
	struct drm_connector *connector;
	int ret = 0;

	drm_connector_list_iter_begin(&dev_priv->drm, &conn_iter);
	drm_for_each_connector_iter(connector, &conn_iter) {
		struct drm_connector_state *conn_state;
		struct intel_crtc_state *crtc_state;
		struct intel_crtc *crtc;

		if (!connector->has_tile ||
		    connector->tile_group->id != tile_group_id)
			continue;

		conn_state = drm_atomic_get_connector_state(&state->base,
							    connector);
		if (IS_ERR(conn_state)) {
			ret = PTR_ERR(conn_state);
			break;
		}

		crtc = to_intel_crtc(conn_state->crtc);

		if (!crtc)
			continue;

		crtc_state = intel_atomic_get_new_crtc_state(state, crtc);
		crtc_state->uapi.mode_changed = true;

		ret = drm_atomic_add_affected_planes(&state->base, &crtc->base);
		if (ret)
			break;
	}
	drm_connector_list_iter_end(&conn_iter);

	return ret;
}

static int intel_modeset_affected_transcoders(struct intel_atomic_state *state, u8 transcoders)
{
	struct drm_i915_private *dev_priv = to_i915(state->base.dev);
	struct intel_crtc *crtc;

	if (transcoders == 0)
		return 0;

	for_each_intel_crtc(&dev_priv->drm, crtc) {
		struct intel_crtc_state *crtc_state;
		int ret;

		crtc_state = intel_atomic_get_crtc_state(&state->base, crtc);
		if (IS_ERR(crtc_state))
			return PTR_ERR(crtc_state);

		if (!crtc_state->hw.enable)
			continue;

		if (!(transcoders & BIT(crtc_state->cpu_transcoder)))
			continue;

		crtc_state->uapi.mode_changed = true;

		ret = drm_atomic_add_affected_connectors(&state->base, &crtc->base);
		if (ret)
			return ret;

		ret = drm_atomic_add_affected_planes(&state->base, &crtc->base);
		if (ret)
			return ret;

		transcoders &= ~BIT(crtc_state->cpu_transcoder);
	}

	drm_WARN_ON(&dev_priv->drm, transcoders != 0);

	return 0;
}

static int intel_modeset_synced_crtcs(struct intel_atomic_state *state,
				      struct drm_connector *connector)
{
	const struct drm_connector_state *old_conn_state =
		drm_atomic_get_old_connector_state(&state->base, connector);
	const struct intel_crtc_state *old_crtc_state;
	struct intel_crtc *crtc;
	u8 transcoders;

	crtc = to_intel_crtc(old_conn_state->crtc);
	if (!crtc)
		return 0;

	old_crtc_state = intel_atomic_get_old_crtc_state(state, crtc);

	if (!old_crtc_state->hw.active)
		return 0;

	transcoders = old_crtc_state->sync_mode_slaves_mask;
	if (old_crtc_state->master_transcoder != INVALID_TRANSCODER)
		transcoders |= BIT(old_crtc_state->master_transcoder);

	return intel_modeset_affected_transcoders(state,
						  transcoders);
}

static int intel_dp_connector_atomic_check(struct drm_connector *conn,
					   struct drm_atomic_state *_state)
{
	struct drm_i915_private *dev_priv = to_i915(conn->dev);
	struct intel_atomic_state *state = to_intel_atomic_state(_state);
	struct drm_connector_state *conn_state = drm_atomic_get_new_connector_state(_state, conn);
	struct intel_connector *intel_conn = to_intel_connector(conn);
	struct intel_dp *intel_dp = enc_to_intel_dp(intel_conn->encoder);
	int ret;

	ret = intel_digital_connector_atomic_check(conn, &state->base);
	if (ret)
		return ret;

	if (intel_dp_mst_source_support(intel_dp)) {
		ret = drm_dp_mst_root_conn_atomic_check(conn_state, &intel_dp->mst_mgr);
		if (ret)
			return ret;
	}

	 
	if (DISPLAY_VER(dev_priv) < 9)
		return 0;

	if (!intel_connector_needs_modeset(state, conn))
		return 0;

	if (conn->has_tile) {
		ret = intel_modeset_tile_group(state, conn->tile_group->id);
		if (ret)
			return ret;
	}

	return intel_modeset_synced_crtcs(state, conn);
}

static void intel_dp_oob_hotplug_event(struct drm_connector *connector)
{
	struct intel_encoder *encoder = intel_attached_encoder(to_intel_connector(connector));
	struct drm_i915_private *i915 = to_i915(connector->dev);

	spin_lock_irq(&i915->irq_lock);
	i915->display.hotplug.event_bits |= BIT(encoder->hpd_pin);
	spin_unlock_irq(&i915->irq_lock);
	queue_delayed_work(i915->unordered_wq, &i915->display.hotplug.hotplug_work, 0);
}

static const struct drm_connector_funcs intel_dp_connector_funcs = {
	.force = intel_dp_force,
	.fill_modes = drm_helper_probe_single_connector_modes,
	.atomic_get_property = intel_digital_connector_atomic_get_property,
	.atomic_set_property = intel_digital_connector_atomic_set_property,
	.late_register = intel_dp_connector_register,
	.early_unregister = intel_dp_connector_unregister,
	.destroy = intel_connector_destroy,
	.atomic_destroy_state = drm_atomic_helper_connector_destroy_state,
	.atomic_duplicate_state = intel_digital_connector_duplicate_state,
	.oob_hotplug_event = intel_dp_oob_hotplug_event,
};

static const struct drm_connector_helper_funcs intel_dp_connector_helper_funcs = {
	.detect_ctx = intel_dp_detect,
	.get_modes = intel_dp_get_modes,
	.mode_valid = intel_dp_mode_valid,
	.atomic_check = intel_dp_connector_atomic_check,
};

enum irqreturn
intel_dp_hpd_pulse(struct intel_digital_port *dig_port, bool long_hpd)
{
	struct drm_i915_private *i915 = to_i915(dig_port->base.base.dev);
	struct intel_dp *intel_dp = &dig_port->dp;

	if (dig_port->base.type == INTEL_OUTPUT_EDP &&
	    (long_hpd || !intel_pps_have_panel_power_or_vdd(intel_dp))) {
		 
		drm_dbg_kms(&i915->drm,
			    "ignoring %s hpd on eDP [ENCODER:%d:%s]\n",
			    long_hpd ? "long" : "short",
			    dig_port->base.base.base.id,
			    dig_port->base.base.name);
		return IRQ_HANDLED;
	}

	drm_dbg_kms(&i915->drm, "got hpd irq on [ENCODER:%d:%s] - %s\n",
		    dig_port->base.base.base.id,
		    dig_port->base.base.name,
		    long_hpd ? "long" : "short");

	if (long_hpd) {
		intel_dp->reset_link_params = true;
		return IRQ_NONE;
	}

	if (intel_dp->is_mst) {
		if (!intel_dp_check_mst_status(intel_dp))
			return IRQ_NONE;
	} else if (!intel_dp_short_pulse(intel_dp)) {
		return IRQ_NONE;
	}

	return IRQ_HANDLED;
}

static bool _intel_dp_is_port_edp(struct drm_i915_private *dev_priv,
				  const struct intel_bios_encoder_data *devdata,
				  enum port port)
{
	 
	if (DISPLAY_VER(dev_priv) < 5)
		return false;

	if (DISPLAY_VER(dev_priv) < 9 && port == PORT_A)
		return true;

	return devdata && intel_bios_encoder_supports_edp(devdata);
}

bool intel_dp_is_port_edp(struct drm_i915_private *i915, enum port port)
{
	const struct intel_bios_encoder_data *devdata =
		intel_bios_encoder_data_lookup(i915, port);

	return _intel_dp_is_port_edp(i915, devdata, port);
}

static bool
has_gamut_metadata_dip(struct intel_encoder *encoder)
{
	struct drm_i915_private *i915 = to_i915(encoder->base.dev);
	enum port port = encoder->port;

	if (intel_bios_encoder_is_lspcon(encoder->devdata))
		return false;

	if (DISPLAY_VER(i915) >= 11)
		return true;

	if (port == PORT_A)
		return false;

	if (IS_HASWELL(i915) || IS_BROADWELL(i915) ||
	    DISPLAY_VER(i915) >= 9)
		return true;

	return false;
}

static void
intel_dp_add_properties(struct intel_dp *intel_dp, struct drm_connector *connector)
{
	struct drm_i915_private *dev_priv = to_i915(connector->dev);
	enum port port = dp_to_dig_port(intel_dp)->base.port;

	if (!intel_dp_is_edp(intel_dp))
		drm_connector_attach_dp_subconnector_property(connector);

	if (!IS_G4X(dev_priv) && port != PORT_A)
		intel_attach_force_audio_property(connector);

	intel_attach_broadcast_rgb_property(connector);
	if (HAS_GMCH(dev_priv))
		drm_connector_attach_max_bpc_property(connector, 6, 10);
	else if (DISPLAY_VER(dev_priv) >= 5)
		drm_connector_attach_max_bpc_property(connector, 6, 12);

	 
	if (intel_bios_encoder_is_lspcon(dp_to_dig_port(intel_dp)->base.devdata)) {
		drm_connector_attach_content_type_property(connector);
		intel_attach_hdmi_colorspace_property(connector);
	} else {
		intel_attach_dp_colorspace_property(connector);
	}

	if (has_gamut_metadata_dip(&dp_to_dig_port(intel_dp)->base))
		drm_connector_attach_hdr_output_metadata_property(connector);

	if (HAS_VRR(dev_priv))
		drm_connector_attach_vrr_capable_property(connector);
}

static void
intel_edp_add_properties(struct intel_dp *intel_dp)
{
	struct intel_connector *connector = intel_dp->attached_connector;
	struct drm_i915_private *i915 = to_i915(connector->base.dev);
	const struct drm_display_mode *fixed_mode =
		intel_panel_preferred_fixed_mode(connector);

	intel_attach_scaling_mode_property(&connector->base);

	drm_connector_set_panel_orientation_with_quirk(&connector->base,
						       i915->display.vbt.orientation,
						       fixed_mode->hdisplay,
						       fixed_mode->vdisplay);
}

static void intel_edp_backlight_setup(struct intel_dp *intel_dp,
				      struct intel_connector *connector)
{
	struct drm_i915_private *i915 = dp_to_i915(intel_dp);
	enum pipe pipe = INVALID_PIPE;

	if (IS_VALLEYVIEW(i915) || IS_CHERRYVIEW(i915)) {
		 
		pipe = vlv_active_pipe(intel_dp);

		if (pipe != PIPE_A && pipe != PIPE_B)
			pipe = intel_dp->pps.pps_pipe;

		if (pipe != PIPE_A && pipe != PIPE_B)
			pipe = PIPE_A;
	}

	intel_backlight_setup(connector, pipe);
}

static bool intel_edp_init_connector(struct intel_dp *intel_dp,
				     struct intel_connector *intel_connector)
{
	struct drm_i915_private *dev_priv = dp_to_i915(intel_dp);
	struct drm_connector *connector = &intel_connector->base;
	struct drm_display_mode *fixed_mode;
	struct intel_encoder *encoder = &dp_to_dig_port(intel_dp)->base;
	bool has_dpcd;
	const struct drm_edid *drm_edid;

	if (!intel_dp_is_edp(intel_dp))
		return true;

	 
	if (intel_get_lvds_encoder(dev_priv)) {
		drm_WARN_ON(&dev_priv->drm,
			    !(HAS_PCH_IBX(dev_priv) || HAS_PCH_CPT(dev_priv)));
		drm_info(&dev_priv->drm,
			 "LVDS was detected, not registering eDP\n");

		return false;
	}

	intel_bios_init_panel_early(dev_priv, &intel_connector->panel,
				    encoder->devdata);

	if (!intel_pps_init(intel_dp)) {
		drm_info(&dev_priv->drm,
			 "[ENCODER:%d:%s] unusable PPS, disabling eDP\n",
			 encoder->base.base.id, encoder->base.name);
		 
		goto out_vdd_off;
	}

	 
	intel_hpd_enable_detection(encoder);

	 
	has_dpcd = intel_edp_init_dpcd(intel_dp);

	if (!has_dpcd) {
		 
		drm_info(&dev_priv->drm,
			 "[ENCODER:%d:%s] failed to retrieve link info, disabling eDP\n",
			 encoder->base.base.id, encoder->base.name);
		goto out_vdd_off;
	}

	 
	if (intel_bios_dp_has_shared_aux_ch(encoder->devdata)) {
		 
		if (!intel_digital_port_connected(encoder)) {
			drm_info(&dev_priv->drm,
				 "[ENCODER:%d:%s] HPD is down, disabling eDP\n",
				 encoder->base.base.id, encoder->base.name);
			goto out_vdd_off;
		}

		 
		if (DISPLAY_VER(dev_priv) == 9 && drm_dp_is_branch(intel_dp->dpcd) &&
		    (intel_dp->dpcd[DP_DOWNSTREAMPORT_PRESENT] & DP_DWN_STRM_PORT_TYPE_MASK) ==
		    DP_DWN_STRM_PORT_TYPE_ANALOG) {
			drm_info(&dev_priv->drm,
				 "[ENCODER:%d:%s] VGA converter detected, disabling eDP\n",
				 encoder->base.base.id, encoder->base.name);
			goto out_vdd_off;
		}
	}

	mutex_lock(&dev_priv->drm.mode_config.mutex);
	drm_edid = drm_edid_read_ddc(connector, &intel_dp->aux.ddc);
	if (!drm_edid) {
		 
		drm_edid = intel_opregion_get_edid(intel_connector);
		if (drm_edid)
			drm_dbg_kms(&dev_priv->drm,
				    "[CONNECTOR:%d:%s] Using OpRegion EDID\n",
				    connector->base.id, connector->name);
	}
	if (drm_edid) {
		if (drm_edid_connector_update(connector, drm_edid) ||
		    !drm_edid_connector_add_modes(connector)) {
			drm_edid_connector_update(connector, NULL);
			drm_edid_free(drm_edid);
			drm_edid = ERR_PTR(-EINVAL);
		}
	} else {
		drm_edid = ERR_PTR(-ENOENT);
	}

	intel_bios_init_panel_late(dev_priv, &intel_connector->panel, encoder->devdata,
				   IS_ERR(drm_edid) ? NULL : drm_edid);

	intel_panel_add_edid_fixed_modes(intel_connector, true);

	 
	intel_edp_mso_init(intel_dp);

	 
	list_for_each_entry(fixed_mode, &intel_connector->panel.fixed_modes, head)
		intel_edp_mso_mode_fixup(intel_connector, fixed_mode);

	 
	if (!intel_panel_preferred_fixed_mode(intel_connector))
		intel_panel_add_vbt_lfp_fixed_mode(intel_connector);

	mutex_unlock(&dev_priv->drm.mode_config.mutex);

	if (!intel_panel_preferred_fixed_mode(intel_connector)) {
		drm_info(&dev_priv->drm,
			 "[ENCODER:%d:%s] failed to find fixed mode for the panel, disabling eDP\n",
			 encoder->base.base.id, encoder->base.name);
		goto out_vdd_off;
	}

	intel_panel_init(intel_connector, drm_edid);

	intel_edp_backlight_setup(intel_dp, intel_connector);

	intel_edp_add_properties(intel_dp);

	intel_pps_init_late(intel_dp);

	return true;

out_vdd_off:
	intel_pps_vdd_off_sync(intel_dp);

	return false;
}

static void intel_dp_modeset_retry_work_fn(struct work_struct *work)
{
	struct intel_connector *intel_connector;
	struct drm_connector *connector;

	intel_connector = container_of(work, typeof(*intel_connector),
				       modeset_retry_work);
	connector = &intel_connector->base;
	drm_dbg_kms(connector->dev, "[CONNECTOR:%d:%s]\n", connector->base.id,
		    connector->name);

	 
	mutex_lock(&connector->dev->mode_config.mutex);
	 
	drm_connector_set_link_status_property(connector,
					       DRM_MODE_LINK_STATUS_BAD);
	mutex_unlock(&connector->dev->mode_config.mutex);
	 
	drm_kms_helper_connector_hotplug_event(connector);
}

bool
intel_dp_init_connector(struct intel_digital_port *dig_port,
			struct intel_connector *intel_connector)
{
	struct drm_connector *connector = &intel_connector->base;
	struct intel_dp *intel_dp = &dig_port->dp;
	struct intel_encoder *intel_encoder = &dig_port->base;
	struct drm_device *dev = intel_encoder->base.dev;
	struct drm_i915_private *dev_priv = to_i915(dev);
	enum port port = intel_encoder->port;
	enum phy phy = intel_port_to_phy(dev_priv, port);
	int type;

	 
	INIT_WORK(&intel_connector->modeset_retry_work,
		  intel_dp_modeset_retry_work_fn);

	if (drm_WARN(dev, dig_port->max_lanes < 1,
		     "Not enough lanes (%d) for DP on [ENCODER:%d:%s]\n",
		     dig_port->max_lanes, intel_encoder->base.base.id,
		     intel_encoder->base.name))
		return false;

	intel_dp->reset_link_params = true;
	intel_dp->pps.pps_pipe = INVALID_PIPE;
	intel_dp->pps.active_pipe = INVALID_PIPE;

	 
	intel_dp->DP = intel_de_read(dev_priv, intel_dp->output_reg);
	intel_dp->attached_connector = intel_connector;

	if (_intel_dp_is_port_edp(dev_priv, intel_encoder->devdata, port)) {
		 
		drm_WARN_ON(dev, intel_phy_is_tc(dev_priv, phy));
		type = DRM_MODE_CONNECTOR_eDP;
		intel_encoder->type = INTEL_OUTPUT_EDP;

		 
		if (drm_WARN_ON(dev, (IS_VALLEYVIEW(dev_priv) ||
				      IS_CHERRYVIEW(dev_priv)) &&
				port != PORT_B && port != PORT_C))
			return false;
	} else {
		type = DRM_MODE_CONNECTOR_DisplayPort;
	}

	intel_dp_set_default_sink_rates(intel_dp);
	intel_dp_set_default_max_sink_lane_count(intel_dp);

	if (IS_VALLEYVIEW(dev_priv) || IS_CHERRYVIEW(dev_priv))
		intel_dp->pps.active_pipe = vlv_active_pipe(intel_dp);

	drm_dbg_kms(&dev_priv->drm,
		    "Adding %s connector on [ENCODER:%d:%s]\n",
		    type == DRM_MODE_CONNECTOR_eDP ? "eDP" : "DP",
		    intel_encoder->base.base.id, intel_encoder->base.name);

	drm_connector_init(dev, connector, &intel_dp_connector_funcs, type);
	drm_connector_helper_add(connector, &intel_dp_connector_helper_funcs);

	if (!HAS_GMCH(dev_priv) && DISPLAY_VER(dev_priv) < 12)
		connector->interlace_allowed = true;

	intel_connector->polled = DRM_CONNECTOR_POLL_HPD;

	intel_dp_aux_init(intel_dp);

	intel_connector_attach_encoder(intel_connector, intel_encoder);

	if (HAS_DDI(dev_priv))
		intel_connector->get_hw_state = intel_ddi_connector_get_hw_state;
	else
		intel_connector->get_hw_state = intel_connector_get_hw_state;

	if (!intel_edp_init_connector(intel_dp, intel_connector)) {
		intel_dp_aux_fini(intel_dp);
		goto fail;
	}

	intel_dp_set_source_rates(intel_dp);
	intel_dp_set_common_rates(intel_dp);
	intel_dp_reset_max_link_params(intel_dp);

	 
	intel_dp_mst_encoder_init(dig_port,
				  intel_connector->base.base.id);

	intel_dp_add_properties(intel_dp, connector);

	if (is_hdcp_supported(dev_priv, port) && !intel_dp_is_edp(intel_dp)) {
		int ret = intel_dp_hdcp_init(dig_port, intel_connector);
		if (ret)
			drm_dbg_kms(&dev_priv->drm,
				    "HDCP init failed, skipping.\n");
	}

	 
	if (IS_G45(dev_priv)) {
		u32 temp = intel_de_read(dev_priv, PEG_BAND_GAP_DATA);
		intel_de_write(dev_priv, PEG_BAND_GAP_DATA,
			       (temp & ~0xf) | 0xd);
	}

	intel_dp->frl.is_trained = false;
	intel_dp->frl.trained_rate_gbps = 0;

	intel_psr_init(intel_dp);

	return true;

fail:
	intel_display_power_flush_work(dev_priv);
	drm_connector_cleanup(connector);

	return false;
}

void intel_dp_mst_suspend(struct drm_i915_private *dev_priv)
{
	struct intel_encoder *encoder;

	if (!HAS_DISPLAY(dev_priv))
		return;

	for_each_intel_encoder(&dev_priv->drm, encoder) {
		struct intel_dp *intel_dp;

		if (encoder->type != INTEL_OUTPUT_DDI)
			continue;

		intel_dp = enc_to_intel_dp(encoder);

		if (!intel_dp_mst_source_support(intel_dp))
			continue;

		if (intel_dp->is_mst)
			drm_dp_mst_topology_mgr_suspend(&intel_dp->mst_mgr);
	}
}

void intel_dp_mst_resume(struct drm_i915_private *dev_priv)
{
	struct intel_encoder *encoder;

	if (!HAS_DISPLAY(dev_priv))
		return;

	for_each_intel_encoder(&dev_priv->drm, encoder) {
		struct intel_dp *intel_dp;
		int ret;

		if (encoder->type != INTEL_OUTPUT_DDI)
			continue;

		intel_dp = enc_to_intel_dp(encoder);

		if (!intel_dp_mst_source_support(intel_dp))
			continue;

		ret = drm_dp_mst_topology_mgr_resume(&intel_dp->mst_mgr,
						     true);
		if (ret) {
			intel_dp->is_mst = false;
			drm_dp_mst_topology_mgr_set_mst(&intel_dp->mst_mgr,
							false);
		}
	}
}
