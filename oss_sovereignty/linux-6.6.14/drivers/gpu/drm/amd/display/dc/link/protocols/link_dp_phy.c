 

 

#include "link_dp_phy.h"
#include "link_dpcd.h"
#include "link_dp_training.h"
#include "link_dp_capability.h"
#include "clk_mgr.h"
#include "resource.h"
#include "link_enc_cfg.h"
#define DC_LOGGER \
	link->ctx->logger

void dpcd_write_rx_power_ctrl(struct dc_link *link, bool on)
{
	uint8_t state;

	state = on ? DP_POWER_STATE_D0 : DP_POWER_STATE_D3;

	if (link->sync_lt_in_progress)
		return;

	core_link_write_dpcd(link, DP_SET_POWER, &state,
						 sizeof(state));

}

void dp_enable_link_phy(
	struct dc_link *link,
	const struct link_resource *link_res,
	enum signal_type signal,
	enum clock_source_id clock_source,
	const struct dc_link_settings *link_settings)
{
	link->cur_link_settings = *link_settings;
	link->dc->hwss.enable_dp_link_output(link, link_res, signal,
			clock_source, link_settings);
	dpcd_write_rx_power_ctrl(link, true);
}

void dp_disable_link_phy(struct dc_link *link,
		const struct link_resource *link_res,
		enum signal_type signal)
{
	struct dc  *dc = link->ctx->dc;

	if (!link->wa_flags.dp_keep_receiver_powered &&
		!link->skip_implict_edp_power_control)
		dpcd_write_rx_power_ctrl(link, false);

	dc->hwss.disable_link_output(link, link_res, signal);
	 
	memset(&link->cur_link_settings, 0,
			sizeof(link->cur_link_settings));

	if (dc->clk_mgr->funcs->notify_link_rate_change)
		dc->clk_mgr->funcs->notify_link_rate_change(dc->clk_mgr, link);
}

static inline bool is_immediate_downstream(struct dc_link *link, uint32_t offset)
{
	return (dp_parse_lttpr_repeater_count(link->dpcd_caps.lttpr_caps.phy_repeater_cnt) ==
			offset);
}

void dp_set_hw_lane_settings(
	struct dc_link *link,
	const struct link_resource *link_res,
	const struct link_training_settings *link_settings,
	uint32_t offset)
{
	const struct link_hwss *link_hwss = get_link_hwss(link, link_res);

	if ((link_settings->lttpr_mode == LTTPR_MODE_NON_TRANSPARENT) &&
			!is_immediate_downstream(link, offset))
		return;

	if (link_hwss->ext.set_dp_lane_settings)
		link_hwss->ext.set_dp_lane_settings(link, link_res,
				&link_settings->link_settings,
				link_settings->hw_lane_settings);

	memmove(link->cur_lane_setting,
			link_settings->hw_lane_settings,
			sizeof(link->cur_lane_setting));
}

void dp_set_drive_settings(
	struct dc_link *link,
	const struct link_resource *link_res,
	struct link_training_settings *lt_settings)
{
	 
	dp_set_hw_lane_settings(link, link_res, lt_settings, DPRX);

	dp_hw_to_dpcd_lane_settings(lt_settings,
			lt_settings->hw_lane_settings,
			lt_settings->dpcd_lane_settings);

	 
	dpcd_set_lane_settings(link, lt_settings, DPRX);
}

enum dc_status dp_set_fec_ready(struct dc_link *link, const struct link_resource *link_res, bool ready)
{
	 
	struct link_encoder *link_enc = NULL;
	enum dc_status status = DC_OK;
	uint8_t fec_config = 0;

	link_enc = link_enc_cfg_get_link_enc(link);
	ASSERT(link_enc);

	if (!dp_should_enable_fec(link))
		return status;

	if (link_enc->funcs->fec_set_ready &&
			link->dpcd_caps.fec_cap.bits.FEC_CAPABLE) {
		if (ready) {
			fec_config = 1;
			status = core_link_write_dpcd(link,
					DP_FEC_CONFIGURATION,
					&fec_config,
					sizeof(fec_config));
			if (status == DC_OK) {
				link_enc->funcs->fec_set_ready(link_enc, true);
				link->fec_state = dc_link_fec_ready;
			} else {
				link_enc->funcs->fec_set_ready(link_enc, false);
				link->fec_state = dc_link_fec_not_ready;
				dm_error("dpcd write failed to set fec_ready");
			}
		} else if (link->fec_state == dc_link_fec_ready) {
			fec_config = 0;
			status = core_link_write_dpcd(link,
					DP_FEC_CONFIGURATION,
					&fec_config,
					sizeof(fec_config));
			link_enc->funcs->fec_set_ready(link_enc, false);
			link->fec_state = dc_link_fec_not_ready;
		}
	}

	return status;
}

void dp_set_fec_enable(struct dc_link *link, bool enable)
{
	struct link_encoder *link_enc = NULL;

	link_enc = link_enc_cfg_get_link_enc(link);
	ASSERT(link_enc);

	if (!dp_should_enable_fec(link))
		return;

	if (link_enc->funcs->fec_set_enable &&
			link->dpcd_caps.fec_cap.bits.FEC_CAPABLE) {
		if (link->fec_state == dc_link_fec_ready && enable) {
			 
			udelay(7);
			link_enc->funcs->fec_set_enable(link_enc, true);
			link->fec_state = dc_link_fec_enabled;
		} else if (link->fec_state == dc_link_fec_enabled && !enable) {
			link_enc->funcs->fec_set_enable(link_enc, false);
			link->fec_state = dc_link_fec_ready;
		}
	}
}

