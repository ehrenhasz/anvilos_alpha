 

 

#include "link_dpms.h"
#include "link_detection.h"
#include "link_hwss.h"
#include "protocols/link_edp_panel_control.h"
#include "protocols/link_ddc.h"
#include "protocols/link_hpd.h"
#include "protocols/link_dpcd.h"
#include "protocols/link_dp_capability.h"
#include "protocols/link_dp_dpia.h"
#include "protocols/link_dp_phy.h"
#include "protocols/link_dp_training.h"
#include "accessories/link_dp_trace.h"

#include "link_enc_cfg.h"
#include "dm_helpers.h"
#include "clk_mgr.h"

#define DC_LOGGER_INIT(logger)

#define LINK_INFO(...) \
	DC_LOG_HW_HOTPLUG(  \
		__VA_ARGS__)
 
#define LINK_TRAINING_MAX_VERIFY_RETRY 2

static const u8 DP_SINK_BRANCH_DEV_NAME_7580[] = "7580\x80u";

static const uint8_t dp_hdmi_dongle_signature_str[] = "DP-HDMI ADAPTOR";

static enum ddc_transaction_type get_ddc_transaction_type(enum signal_type sink_signal)
{
	enum ddc_transaction_type transaction_type = DDC_TRANSACTION_TYPE_NONE;

	switch (sink_signal) {
	case SIGNAL_TYPE_DVI_SINGLE_LINK:
	case SIGNAL_TYPE_DVI_DUAL_LINK:
	case SIGNAL_TYPE_HDMI_TYPE_A:
	case SIGNAL_TYPE_LVDS:
	case SIGNAL_TYPE_RGB:
		transaction_type = DDC_TRANSACTION_TYPE_I2C;
		break;

	case SIGNAL_TYPE_DISPLAY_PORT:
	case SIGNAL_TYPE_EDP:
		transaction_type = DDC_TRANSACTION_TYPE_I2C_OVER_AUX;
		break;

	case SIGNAL_TYPE_DISPLAY_PORT_MST:
		 
		transaction_type = DDC_TRANSACTION_TYPE_I2C_OVER_AUX;
		break;

	default:
		break;
	}

	return transaction_type;
}

static enum signal_type get_basic_signal_type(struct graphics_object_id encoder,
					      struct graphics_object_id downstream)
{
	if (downstream.type == OBJECT_TYPE_CONNECTOR) {
		switch (downstream.id) {
		case CONNECTOR_ID_SINGLE_LINK_DVII:
			switch (encoder.id) {
			case ENCODER_ID_INTERNAL_DAC1:
			case ENCODER_ID_INTERNAL_KLDSCP_DAC1:
			case ENCODER_ID_INTERNAL_DAC2:
			case ENCODER_ID_INTERNAL_KLDSCP_DAC2:
				return SIGNAL_TYPE_RGB;
			default:
				return SIGNAL_TYPE_DVI_SINGLE_LINK;
			}
		break;
		case CONNECTOR_ID_DUAL_LINK_DVII:
		{
			switch (encoder.id) {
			case ENCODER_ID_INTERNAL_DAC1:
			case ENCODER_ID_INTERNAL_KLDSCP_DAC1:
			case ENCODER_ID_INTERNAL_DAC2:
			case ENCODER_ID_INTERNAL_KLDSCP_DAC2:
				return SIGNAL_TYPE_RGB;
			default:
				return SIGNAL_TYPE_DVI_DUAL_LINK;
			}
		}
		break;
		case CONNECTOR_ID_SINGLE_LINK_DVID:
			return SIGNAL_TYPE_DVI_SINGLE_LINK;
		case CONNECTOR_ID_DUAL_LINK_DVID:
			return SIGNAL_TYPE_DVI_DUAL_LINK;
		case CONNECTOR_ID_VGA:
			return SIGNAL_TYPE_RGB;
		case CONNECTOR_ID_HDMI_TYPE_A:
			return SIGNAL_TYPE_HDMI_TYPE_A;
		case CONNECTOR_ID_LVDS:
			return SIGNAL_TYPE_LVDS;
		case CONNECTOR_ID_DISPLAY_PORT:
		case CONNECTOR_ID_USBC:
			return SIGNAL_TYPE_DISPLAY_PORT;
		case CONNECTOR_ID_EDP:
			return SIGNAL_TYPE_EDP;
		default:
			return SIGNAL_TYPE_NONE;
		}
	} else if (downstream.type == OBJECT_TYPE_ENCODER) {
		switch (downstream.id) {
		case ENCODER_ID_EXTERNAL_NUTMEG:
		case ENCODER_ID_EXTERNAL_TRAVIS:
			return SIGNAL_TYPE_DISPLAY_PORT;
		default:
			return SIGNAL_TYPE_NONE;
		}
	}

	return SIGNAL_TYPE_NONE;
}

 
static enum signal_type link_detect_sink_signal_type(struct dc_link *link,
					 enum dc_detect_reason reason)
{
	enum signal_type result;
	struct graphics_object_id enc_id;

	if (link->is_dig_mapping_flexible)
		enc_id = (struct graphics_object_id){.id = ENCODER_ID_UNKNOWN};
	else
		enc_id = link->link_enc->id;
	result = get_basic_signal_type(enc_id, link->link_id);

	 
	if (link->ep_type != DISPLAY_ENDPOINT_PHY)
		return result;

	 

	 

	 
	if (link->link_id.id == CONNECTOR_ID_PCIE) {
		 
	}

	switch (link->link_id.id) {
	case CONNECTOR_ID_HDMI_TYPE_A: {
		 
		struct audio_support *aud_support =
					&link->dc->res_pool->audio_support;

		if (!aud_support->hdmi_audio_native)
			if (link->link_id.id == CONNECTOR_ID_HDMI_TYPE_A)
				result = SIGNAL_TYPE_DVI_SINGLE_LINK;
	}
	break;
	case CONNECTOR_ID_DISPLAY_PORT:
	case CONNECTOR_ID_USBC: {
		 
		if (reason != DETECT_REASON_HPDRX) {
			 
			if (!dm_helpers_is_dp_sink_present(link))
				result = SIGNAL_TYPE_DVI_SINGLE_LINK;
		}
	}
	break;
	default:
	break;
	}

	return result;
}

static enum signal_type decide_signal_from_strap_and_dongle_type(enum display_dongle_type dongle_type,
								 struct audio_support *audio_support)
{
	enum signal_type signal = SIGNAL_TYPE_NONE;

	switch (dongle_type) {
	case DISPLAY_DONGLE_DP_HDMI_DONGLE:
		if (audio_support->hdmi_audio_on_dongle)
			signal = SIGNAL_TYPE_HDMI_TYPE_A;
		else
			signal = SIGNAL_TYPE_DVI_SINGLE_LINK;
		break;
	case DISPLAY_DONGLE_DP_DVI_DONGLE:
		signal = SIGNAL_TYPE_DVI_SINGLE_LINK;
		break;
	case DISPLAY_DONGLE_DP_HDMI_MISMATCHED_DONGLE:
		if (audio_support->hdmi_audio_native)
			signal =  SIGNAL_TYPE_HDMI_TYPE_A;
		else
			signal = SIGNAL_TYPE_DVI_SINGLE_LINK;
		break;
	default:
		signal = SIGNAL_TYPE_NONE;
		break;
	}

	return signal;
}

static void read_scdc_caps(struct ddc_service *ddc_service,
		struct dc_sink *sink)
{
	uint8_t slave_address = HDMI_SCDC_ADDRESS;
	uint8_t offset = HDMI_SCDC_MANUFACTURER_OUI;

	link_query_ddc_data(ddc_service, slave_address, &offset,
			sizeof(offset), sink->scdc_caps.manufacturer_OUI.byte,
			sizeof(sink->scdc_caps.manufacturer_OUI.byte));

	offset = HDMI_SCDC_DEVICE_ID;

	link_query_ddc_data(ddc_service, slave_address, &offset,
			sizeof(offset), &(sink->scdc_caps.device_id.byte),
			sizeof(sink->scdc_caps.device_id.byte));
}

static bool i2c_read(
	struct ddc_service *ddc,
	uint32_t address,
	uint8_t *buffer,
	uint32_t len)
{
	uint8_t offs_data = 0;
	struct i2c_payload payloads[2] = {
		{
		.write = true,
		.address = address,
		.length = 1,
		.data = &offs_data },
		{
		.write = false,
		.address = address,
		.length = len,
		.data = buffer } };

	struct i2c_command command = {
		.payloads = payloads,
		.number_of_payloads = 2,
		.engine = DDC_I2C_COMMAND_ENGINE,
		.speed = ddc->ctx->dc->caps.i2c_speed_in_khz };

	return dm_helpers_submit_i2c(
			ddc->ctx,
			ddc->link,
			&command);
}

enum {
	DP_SINK_CAP_SIZE =
		DP_EDP_CONFIGURATION_CAP - DP_DPCD_REV + 1
};

static void query_dp_dual_mode_adaptor(
	struct ddc_service *ddc,
	struct display_sink_capability *sink_cap)
{
	uint8_t i;
	bool is_valid_hdmi_signature;
	enum display_dongle_type *dongle = &sink_cap->dongle_type;
	uint8_t type2_dongle_buf[DP_ADAPTOR_TYPE2_SIZE];
	bool is_type2_dongle = false;
	int retry_count = 2;
	struct dp_hdmi_dongle_signature_data *dongle_signature;

	 
	*dongle = DISPLAY_DONGLE_NONE;
	sink_cap->max_hdmi_pixel_clock = DP_ADAPTOR_HDMI_SAFE_MAX_TMDS_CLK;

	 
	if (!i2c_read(
		ddc,
		DP_HDMI_DONGLE_ADDRESS,
		type2_dongle_buf,
		sizeof(type2_dongle_buf))) {
		 
		while (retry_count > 0) {
			if (i2c_read(ddc,
				DP_HDMI_DONGLE_ADDRESS,
				type2_dongle_buf,
				sizeof(type2_dongle_buf)))
				break;
			retry_count--;
		}
		if (retry_count == 0) {
			*dongle = DISPLAY_DONGLE_DP_DVI_DONGLE;
			sink_cap->max_hdmi_pixel_clock = DP_ADAPTOR_DVI_MAX_TMDS_CLK;

			CONN_DATA_DETECT(ddc->link, type2_dongle_buf, sizeof(type2_dongle_buf),
					"DP-DVI passive dongle %dMhz: ",
					DP_ADAPTOR_DVI_MAX_TMDS_CLK / 1000);
			return;
		}
	}

	 
	if (type2_dongle_buf[DP_ADAPTOR_TYPE2_REG_ID] == DP_ADAPTOR_TYPE2_ID)
		is_type2_dongle = true;

	dongle_signature =
		(struct dp_hdmi_dongle_signature_data *)type2_dongle_buf;

	is_valid_hdmi_signature = true;

	 
	if (dongle_signature->eot != DP_HDMI_DONGLE_SIGNATURE_EOT) {
		is_valid_hdmi_signature = false;
	}

	 
	for (i = 0; i < sizeof(dongle_signature->id); ++i) {
		 
		if (dongle_signature->id[i] !=
			dp_hdmi_dongle_signature_str[i] && i != 3) {

			if (is_type2_dongle) {
				is_valid_hdmi_signature = false;
				break;
			}

		}
	}

	if (is_type2_dongle) {
		uint32_t max_tmds_clk =
			type2_dongle_buf[DP_ADAPTOR_TYPE2_REG_MAX_TMDS_CLK];

		max_tmds_clk = max_tmds_clk * 2 + max_tmds_clk / 2;

		if (0 == max_tmds_clk ||
				max_tmds_clk < DP_ADAPTOR_TYPE2_MIN_TMDS_CLK ||
				max_tmds_clk > DP_ADAPTOR_TYPE2_MAX_TMDS_CLK) {
			*dongle = DISPLAY_DONGLE_DP_DVI_DONGLE;

			CONN_DATA_DETECT(ddc->link, type2_dongle_buf,
					sizeof(type2_dongle_buf),
					"DP-DVI passive dongle %dMhz: ",
					DP_ADAPTOR_DVI_MAX_TMDS_CLK / 1000);
		} else {
			if (is_valid_hdmi_signature == true) {
				*dongle = DISPLAY_DONGLE_DP_HDMI_DONGLE;

				CONN_DATA_DETECT(ddc->link, type2_dongle_buf,
						sizeof(type2_dongle_buf),
						"Type 2 DP-HDMI passive dongle %dMhz: ",
						max_tmds_clk);
			} else {
				*dongle = DISPLAY_DONGLE_DP_HDMI_MISMATCHED_DONGLE;

				CONN_DATA_DETECT(ddc->link, type2_dongle_buf,
						sizeof(type2_dongle_buf),
						"Type 2 DP-HDMI passive dongle (no signature) %dMhz: ",
						max_tmds_clk);

			}

			 
			sink_cap->max_hdmi_pixel_clock =
				max_tmds_clk * 1000;
		}
		sink_cap->is_dongle_type_one = false;

	} else {
		if (is_valid_hdmi_signature == true) {
			*dongle = DISPLAY_DONGLE_DP_HDMI_DONGLE;

			CONN_DATA_DETECT(ddc->link, type2_dongle_buf,
					sizeof(type2_dongle_buf),
					"Type 1 DP-HDMI passive dongle %dMhz: ",
					sink_cap->max_hdmi_pixel_clock / 1000);
		} else {
			*dongle = DISPLAY_DONGLE_DP_HDMI_MISMATCHED_DONGLE;

			CONN_DATA_DETECT(ddc->link, type2_dongle_buf,
					sizeof(type2_dongle_buf),
					"Type 1 DP-HDMI passive dongle (no signature) %dMhz: ",
					sink_cap->max_hdmi_pixel_clock / 1000);
		}
		sink_cap->is_dongle_type_one = true;
	}

	return;
}

static enum signal_type dp_passive_dongle_detection(struct ddc_service *ddc,
						    struct display_sink_capability *sink_cap,
						    struct audio_support *audio_support)
{
	query_dp_dual_mode_adaptor(ddc, sink_cap);

	return decide_signal_from_strap_and_dongle_type(sink_cap->dongle_type,
							audio_support);
}

static void link_disconnect_sink(struct dc_link *link)
{
	if (link->local_sink) {
		dc_sink_release(link->local_sink);
		link->local_sink = NULL;
	}

	link->dpcd_sink_count = 0;
	
}

static void link_disconnect_remap(struct dc_sink *prev_sink, struct dc_link *link)
{
	dc_sink_release(link->local_sink);
	link->local_sink = prev_sink;
}

static void query_hdcp_capability(enum signal_type signal, struct dc_link *link)
{
	struct hdcp_protection_message msg22;
	struct hdcp_protection_message msg14;

	memset(&msg22, 0, sizeof(struct hdcp_protection_message));
	memset(&msg14, 0, sizeof(struct hdcp_protection_message));
	memset(link->hdcp_caps.rx_caps.raw, 0,
		sizeof(link->hdcp_caps.rx_caps.raw));

	if ((link->connector_signal == SIGNAL_TYPE_DISPLAY_PORT &&
			link->ddc->transaction_type ==
			DDC_TRANSACTION_TYPE_I2C_OVER_AUX) ||
			link->connector_signal == SIGNAL_TYPE_EDP) {
		msg22.data = link->hdcp_caps.rx_caps.raw;
		msg22.length = sizeof(link->hdcp_caps.rx_caps.raw);
		msg22.msg_id = HDCP_MESSAGE_ID_RX_CAPS;
	} else {
		msg22.data = &link->hdcp_caps.rx_caps.fields.version;
		msg22.length = sizeof(link->hdcp_caps.rx_caps.fields.version);
		msg22.msg_id = HDCP_MESSAGE_ID_HDCP2VERSION;
	}
	msg22.version = HDCP_VERSION_22;
	msg22.link = HDCP_LINK_PRIMARY;
	msg22.max_retries = 5;
	dc_process_hdcp_msg(signal, link, &msg22);

	if (signal == SIGNAL_TYPE_DISPLAY_PORT || signal == SIGNAL_TYPE_DISPLAY_PORT_MST) {
		msg14.data = &link->hdcp_caps.bcaps.raw;
		msg14.length = sizeof(link->hdcp_caps.bcaps.raw);
		msg14.msg_id = HDCP_MESSAGE_ID_READ_BCAPS;
		msg14.version = HDCP_VERSION_14;
		msg14.link = HDCP_LINK_PRIMARY;
		msg14.max_retries = 5;

		dc_process_hdcp_msg(signal, link, &msg14);
	}

}
static void read_current_link_settings_on_detect(struct dc_link *link)
{
	union lane_count_set lane_count_set = {0};
	uint8_t link_bw_set;
	uint8_t link_rate_set;
	uint32_t read_dpcd_retry_cnt = 10;
	enum dc_status status = DC_ERROR_UNEXPECTED;
	int i;
	union max_down_spread max_down_spread = {0};

	
	for (i = 0; i < read_dpcd_retry_cnt; i++) {
		status = core_link_read_dpcd(link,
					     DP_LANE_COUNT_SET,
					     &lane_count_set.raw,
					     sizeof(lane_count_set));
		 
		if (status == DC_OK) {
			link->cur_link_settings.lane_count =
					lane_count_set.bits.LANE_COUNT_SET;
			break;
		}

		msleep(8);
	}

	
	core_link_read_dpcd(link, DP_LINK_BW_SET,
			    &link_bw_set, sizeof(link_bw_set));

	if (link_bw_set == 0) {
		if (link->connector_signal == SIGNAL_TYPE_EDP) {
			 
			core_link_read_dpcd(link, DP_LINK_RATE_SET,
					    &link_rate_set, sizeof(link_rate_set));

			
			if (link_rate_set < link->dpcd_caps.edp_supported_link_rates_count) {
				link->cur_link_settings.link_rate =
					link->dpcd_caps.edp_supported_link_rates[link_rate_set];
				link->cur_link_settings.link_rate_set = link_rate_set;
				link->cur_link_settings.use_link_rate_set = true;
			}
		} else {
			
			ASSERT(false);
		}
	} else {
		link->cur_link_settings.link_rate = link_bw_set;
		link->cur_link_settings.use_link_rate_set = false;
	}
	
	core_link_read_dpcd(link, DP_MAX_DOWNSPREAD,
			    &max_down_spread.raw, sizeof(max_down_spread));
	link->cur_link_settings.link_spread =
		max_down_spread.bits.MAX_DOWN_SPREAD ?
		LINK_SPREAD_05_DOWNSPREAD_30KHZ : LINK_SPREAD_DISABLED;
}

static bool detect_dp(struct dc_link *link,
		      struct display_sink_capability *sink_caps,
		      enum dc_detect_reason reason)
{
	struct audio_support *audio_support = &link->dc->res_pool->audio_support;

	sink_caps->signal = link_detect_sink_signal_type(link, reason);
	sink_caps->transaction_type =
		get_ddc_transaction_type(sink_caps->signal);

	if (sink_caps->transaction_type == DDC_TRANSACTION_TYPE_I2C_OVER_AUX) {
		sink_caps->signal = SIGNAL_TYPE_DISPLAY_PORT;
		if (!detect_dp_sink_caps(link))
			return false;

		if (is_dp_branch_device(link))
			 
			link->type = dc_connection_sst_branch;
	} else {
		if (link->dc->debug.disable_dp_plus_plus_wa &&
				link->link_enc->features.flags.bits.IS_UHBR20_CAPABLE)
			return false;

		 
		sink_caps->signal = dp_passive_dongle_detection(link->ddc,
								sink_caps,
								audio_support);
		link->dpcd_caps.dongle_type = sink_caps->dongle_type;
		link->dpcd_caps.is_dongle_type_one = sink_caps->is_dongle_type_one;
		link->dpcd_caps.dpcd_rev.raw = 0;
	}

	return true;
}

static bool is_same_edid(struct dc_edid *old_edid, struct dc_edid *new_edid)
{
	if (old_edid->length != new_edid->length)
		return false;

	if (new_edid->length == 0)
		return false;

	return (memcmp(old_edid->raw_edid,
		       new_edid->raw_edid, new_edid->length) == 0);
}

static bool wait_for_entering_dp_alt_mode(struct dc_link *link)
{

	 
	unsigned int sleep_time_in_microseconds = 500;
	unsigned int tries_allowed = 400;
	bool is_in_alt_mode;
	unsigned long long enter_timestamp;
	unsigned long long finish_timestamp;
	unsigned long long time_taken_in_ns;
	int tries_taken;

	DC_LOGGER_INIT(link->ctx->logger);

	 
	if (!link->link_enc->funcs->is_in_alt_mode)
		return true;

	is_in_alt_mode = link->link_enc->funcs->is_in_alt_mode(link->link_enc);
	DC_LOG_DC("DP Alt mode state on HPD: %d\n", is_in_alt_mode);

	if (is_in_alt_mode)
		return true;

	enter_timestamp = dm_get_timestamp(link->ctx);

	for (tries_taken = 0; tries_taken < tries_allowed; tries_taken++) {
		udelay(sleep_time_in_microseconds);
		 
		if (link->link_enc->funcs->is_in_alt_mode(link->link_enc)) {
			finish_timestamp = dm_get_timestamp(link->ctx);
			time_taken_in_ns =
				dm_get_elapse_time_in_ns(link->ctx,
							 finish_timestamp,
							 enter_timestamp);
			DC_LOG_WARNING("Alt mode entered finished after %llu ms\n",
				       div_u64(time_taken_in_ns, 1000000));
			return true;
		}
	}
	finish_timestamp = dm_get_timestamp(link->ctx);
	time_taken_in_ns = dm_get_elapse_time_in_ns(link->ctx, finish_timestamp,
						    enter_timestamp);
	DC_LOG_WARNING("Alt mode has timed out after %llu ms\n",
			div_u64(time_taken_in_ns, 1000000));
	return false;
}

static void apply_dpia_mst_dsc_always_on_wa(struct dc_link *link)
{
	 
	if (link->ep_type == DISPLAY_ENDPOINT_USB4_DPIA &&
			link->type == dc_connection_mst_branch &&
			link->dpcd_caps.branch_dev_id == DP_BRANCH_DEVICE_ID_90CC24 &&
			link->dpcd_caps.branch_hw_revision == DP_BRANCH_HW_REV_20 &&
			link->dpcd_caps.dsc_caps.dsc_basic_caps.fields.dsc_support.DSC_SUPPORT &&
			!link->dc->debug.dpia_debug.bits.disable_mst_dsc_work_around)
		link->wa_flags.dpia_mst_dsc_always_on = true;
}

static void revert_dpia_mst_dsc_always_on_wa(struct dc_link *link)
{
	 
	if (link->ep_type == DISPLAY_ENDPOINT_USB4_DPIA)
		link->wa_flags.dpia_mst_dsc_always_on = false;
}

static bool discover_dp_mst_topology(struct dc_link *link, enum dc_detect_reason reason)
{
	DC_LOGGER_INIT(link->ctx->logger);

	LINK_INFO("link=%d, mst branch is now Connected\n",
		  link->link_index);

	link->type = dc_connection_mst_branch;
	apply_dpia_mst_dsc_always_on_wa(link);

	dm_helpers_dp_update_branch_info(link->ctx, link);
	if (dm_helpers_dp_mst_start_top_mgr(link->ctx,
			link, (reason == DETECT_REASON_BOOT || reason == DETECT_REASON_RESUMEFROMS3S4))) {
		link_disconnect_sink(link);
	} else {
		link->type = dc_connection_sst_branch;
	}

	return link->type == dc_connection_mst_branch;
}

bool link_reset_cur_dp_mst_topology(struct dc_link *link)
{
	DC_LOGGER_INIT(link->ctx->logger);

	LINK_INFO("link=%d, mst branch is now Disconnected\n",
		  link->link_index);

	revert_dpia_mst_dsc_always_on_wa(link);
	return dm_helpers_dp_mst_stop_top_mgr(link->ctx, link);
}

static bool should_prepare_phy_clocks_for_link_verification(const struct dc *dc,
		enum dc_detect_reason reason)
{
	int i;
	bool can_apply_seamless_boot = false;

	for (i = 0; i < dc->current_state->stream_count; i++) {
		if (dc->current_state->streams[i]->apply_seamless_boot_optimization) {
			can_apply_seamless_boot = true;
			break;
		}
	}

	return !can_apply_seamless_boot && reason != DETECT_REASON_BOOT;
}

static void prepare_phy_clocks_for_destructive_link_verification(const struct dc *dc)
{
	dc_z10_restore(dc);
	clk_mgr_exit_optimized_pwr_state(dc, dc->clk_mgr);
}

static void restore_phy_clocks_for_destructive_link_verification(const struct dc *dc)
{
	clk_mgr_optimize_pwr_state(dc, dc->clk_mgr);
}

static void verify_link_capability_destructive(struct dc_link *link,
		struct dc_sink *sink,
		enum dc_detect_reason reason)
{
	bool should_prepare_phy_clocks =
			should_prepare_phy_clocks_for_link_verification(link->dc, reason);

	if (should_prepare_phy_clocks)
		prepare_phy_clocks_for_destructive_link_verification(link->dc);

	if (dc_is_dp_signal(link->local_sink->sink_signal)) {
		struct dc_link_settings known_limit_link_setting =
				dp_get_max_link_cap(link);
		link_set_all_streams_dpms_off_for_link(link);
		dp_verify_link_cap_with_retries(
				link, &known_limit_link_setting,
				LINK_TRAINING_MAX_VERIFY_RETRY);
	} else {
		ASSERT(0);
	}

	if (should_prepare_phy_clocks)
		restore_phy_clocks_for_destructive_link_verification(link->dc);
}

static void verify_link_capability_non_destructive(struct dc_link *link)
{
	if (dc_is_dp_signal(link->local_sink->sink_signal)) {
		if (dc_is_embedded_signal(link->local_sink->sink_signal) ||
				link->ep_type == DISPLAY_ENDPOINT_USB4_DPIA)
			 
			link->verified_link_cap = link->reported_link_cap;
		else
			link->verified_link_cap = dp_get_max_link_cap(link);
	}
}

static bool should_verify_link_capability_destructively(struct dc_link *link,
		enum dc_detect_reason reason)
{
	bool destrictive = false;
	struct dc_link_settings max_link_cap;
	bool is_link_enc_unavailable = link->link_enc &&
			link->dc->res_pool->funcs->link_encs_assign &&
			!link_enc_cfg_is_link_enc_avail(
					link->ctx->dc,
					link->link_enc->preferred_engine,
					link);

	if (dc_is_dp_signal(link->local_sink->sink_signal)) {
		max_link_cap = dp_get_max_link_cap(link);
		destrictive = true;

		if (link->dc->debug.skip_detection_link_training ||
				dc_is_embedded_signal(link->local_sink->sink_signal) ||
				link->ep_type == DISPLAY_ENDPOINT_USB4_DPIA) {
			destrictive = false;
		} else if (link_dp_get_encoding_format(&max_link_cap) ==
				DP_8b_10b_ENCODING) {
			if (link->dpcd_caps.is_mst_capable ||
					is_link_enc_unavailable) {
				destrictive = false;
			}
		}
	}

	return destrictive;
}

static void verify_link_capability(struct dc_link *link, struct dc_sink *sink,
		enum dc_detect_reason reason)
{
	if (should_verify_link_capability_destructively(link, reason))
		verify_link_capability_destructive(link, sink, reason);
	else
		verify_link_capability_non_destructive(link);
}

 
static bool detect_link_and_local_sink(struct dc_link *link,
				  enum dc_detect_reason reason)
{
	struct dc_sink_init_data sink_init_data = { 0 };
	struct display_sink_capability sink_caps = { 0 };
	uint32_t i;
	bool converter_disable_audio = false;
	struct audio_support *aud_support = &link->dc->res_pool->audio_support;
	bool same_edid = false;
	enum dc_edid_status edid_status;
	struct dc_context *dc_ctx = link->ctx;
	struct dc *dc = dc_ctx->dc;
	struct dc_sink *sink = NULL;
	struct dc_sink *prev_sink = NULL;
	struct dpcd_caps prev_dpcd_caps;
	enum dc_connection_type new_connection_type = dc_connection_none;
	enum dc_connection_type pre_connection_type = link->type;
	const uint32_t post_oui_delay = 30;  

	DC_LOGGER_INIT(link->ctx->logger);

	if (dc_is_virtual_signal(link->connector_signal))
		return false;

	if (((link->connector_signal == SIGNAL_TYPE_LVDS ||
		link->connector_signal == SIGNAL_TYPE_EDP) &&
		(!link->dc->config.allow_edp_hotplug_detection)) &&
		link->local_sink) {
		 
		if (link->connector_signal == SIGNAL_TYPE_EDP &&
			(link->dpcd_sink_ext_caps.bits.oled == 1)) {
			dpcd_set_source_specific_data(link);
			msleep(post_oui_delay);
			set_default_brightness_aux(link);
		}

		return true;
	}

	if (!link_detect_connection_type(link, &new_connection_type)) {
		BREAK_TO_DEBUGGER();
		return false;
	}

	prev_sink = link->local_sink;
	if (prev_sink) {
		dc_sink_retain(prev_sink);
		memcpy(&prev_dpcd_caps, &link->dpcd_caps, sizeof(struct dpcd_caps));
	}

	link_disconnect_sink(link);
	if (new_connection_type != dc_connection_none) {
		link->type = new_connection_type;
		link->link_state_valid = false;

		 
		switch (link->connector_signal) {
		case SIGNAL_TYPE_HDMI_TYPE_A: {
			sink_caps.transaction_type = DDC_TRANSACTION_TYPE_I2C;
			if (aud_support->hdmi_audio_native)
				sink_caps.signal = SIGNAL_TYPE_HDMI_TYPE_A;
			else
				sink_caps.signal = SIGNAL_TYPE_DVI_SINGLE_LINK;
			break;
		}

		case SIGNAL_TYPE_DVI_SINGLE_LINK: {
			sink_caps.transaction_type = DDC_TRANSACTION_TYPE_I2C;
			sink_caps.signal = SIGNAL_TYPE_DVI_SINGLE_LINK;
			break;
		}

		case SIGNAL_TYPE_DVI_DUAL_LINK: {
			sink_caps.transaction_type = DDC_TRANSACTION_TYPE_I2C;
			sink_caps.signal = SIGNAL_TYPE_DVI_DUAL_LINK;
			break;
		}

		case SIGNAL_TYPE_LVDS: {
			sink_caps.transaction_type = DDC_TRANSACTION_TYPE_I2C;
			sink_caps.signal = SIGNAL_TYPE_LVDS;
			break;
		}

		case SIGNAL_TYPE_EDP: {
			detect_edp_sink_caps(link);
			read_current_link_settings_on_detect(link);

			 
			if (dc->config.enable_mipi_converter_optimization &&
				dc_ctx->dce_version == DCN_VERSION_3_01 &&
				link->dpcd_caps.sink_dev_id == DP_BRANCH_DEVICE_ID_0022B9 &&
				memcmp(&link->dpcd_caps.branch_dev_name, DP_SINK_BRANCH_DEV_NAME_7580,
					sizeof(link->dpcd_caps.branch_dev_name)) == 0) {
				dc->config.edp_no_power_sequencing = true;

				if (!link->dpcd_caps.set_power_state_capable_edp)
					link->wa_flags.dp_keep_receiver_powered = true;
			}

			sink_caps.transaction_type = DDC_TRANSACTION_TYPE_I2C_OVER_AUX;
			sink_caps.signal = SIGNAL_TYPE_EDP;
			break;
		}

		case SIGNAL_TYPE_DISPLAY_PORT: {

			 
			if (link->ep_type == DISPLAY_ENDPOINT_PHY &&
			    link->link_enc->features.flags.bits.DP_IS_USB_C == 1) {

				 
				if (!wait_for_entering_dp_alt_mode(link))
					return false;
			}

			if (!detect_dp(link, &sink_caps, reason)) {
				link->type = pre_connection_type;

				if (prev_sink)
					dc_sink_release(prev_sink);
				return false;
			}

			 
			if (link->type == dc_connection_sst_branch &&
			    link->dpcd_caps.sink_count.bits.SINK_COUNT == 0) {
				if (prev_sink)
					 
					dc_sink_release(prev_sink);
				return true;
			}

			 
			if (link->type == dc_connection_sst_branch &&
					is_dp_active_dongle(link) &&
					(link->dpcd_caps.dongle_type !=
							DISPLAY_DONGLE_DP_HDMI_CONVERTER))
				converter_disable_audio = true;

			 
			if (link->ep_type == DISPLAY_ENDPOINT_USB4_DPIA &&
					link->reported_link_cap.link_rate > LINK_RATE_HIGH3)
				link->reported_link_cap.link_rate = LINK_RATE_HIGH3;
			break;
		}

		default:
			DC_ERROR("Invalid connector type! signal:%d\n",
				 link->connector_signal);
			if (prev_sink)
				dc_sink_release(prev_sink);
			return false;
		}  

		if (link->dpcd_caps.sink_count.bits.SINK_COUNT)
			link->dpcd_sink_count =
				link->dpcd_caps.sink_count.bits.SINK_COUNT;
		else
			link->dpcd_sink_count = 1;

		set_ddc_transaction_type(link->ddc,
						     sink_caps.transaction_type);

		link->aux_mode =
			link_is_in_aux_transaction_mode(link->ddc);

		sink_init_data.link = link;
		sink_init_data.sink_signal = sink_caps.signal;

		sink = dc_sink_create(&sink_init_data);
		if (!sink) {
			DC_ERROR("Failed to create sink!\n");
			if (prev_sink)
				dc_sink_release(prev_sink);
			return false;
		}

		sink->link->dongle_max_pix_clk = sink_caps.max_hdmi_pixel_clock;
		sink->converter_disable_audio = converter_disable_audio;

		 
		link->local_sink = sink;

		edid_status = dm_helpers_read_local_edid(link->ctx,
							 link, sink);

		switch (edid_status) {
		case EDID_BAD_CHECKSUM:
			DC_LOG_ERROR("EDID checksum invalid.\n");
			break;
		case EDID_PARTIAL_VALID:
			DC_LOG_ERROR("Partial EDID valid, abandon invalid blocks.\n");
			break;
		case EDID_NO_RESPONSE:
			DC_LOG_ERROR("No EDID read.\n");
			 
			if (dc_is_hdmi_signal(link->connector_signal) ||
			    dc_is_dvi_signal(link->connector_signal)) {
				if (prev_sink)
					dc_sink_release(prev_sink);

				return false;
			}

			if (link->type == dc_connection_sst_branch &&
					link->dpcd_caps.dongle_type ==
						DISPLAY_DONGLE_DP_VGA_CONVERTER &&
					reason == DETECT_REASON_HPDRX) {
				 
				if (prev_sink)
					dc_sink_release(prev_sink);
				link_disconnect_sink(link);

				return true;
			}

			break;
		default:
			break;
		}

		
		if ((prev_sink) &&
		    (edid_status == EDID_THE_SAME || edid_status == EDID_OK))
			same_edid = is_same_edid(&prev_sink->dc_edid,
						 &sink->dc_edid);

		if (sink->edid_caps.panel_patch.skip_scdc_overwrite)
			link->ctx->dc->debug.hdmi20_disable = true;

		if (sink->edid_caps.panel_patch.remove_sink_ext_caps)
			link->dpcd_sink_ext_caps.raw = 0;

		if (dc_is_hdmi_signal(link->connector_signal))
			read_scdc_caps(link->ddc, link->local_sink);

		if (link->connector_signal == SIGNAL_TYPE_DISPLAY_PORT &&
		    sink_caps.transaction_type ==
		    DDC_TRANSACTION_TYPE_I2C_OVER_AUX) {
			 
			query_hdcp_capability(sink->sink_signal, link);
		} else {
			 
			if (same_edid) {
				link_disconnect_remap(prev_sink, link);
				sink = prev_sink;
				prev_sink = NULL;
			}
			query_hdcp_capability(sink->sink_signal, link);
		}

		 
		if (sink->sink_signal == SIGNAL_TYPE_HDMI_TYPE_A &&
		    !sink->edid_caps.edid_hdmi)
			sink->sink_signal = SIGNAL_TYPE_DVI_SINGLE_LINK;

		if (link->local_sink && dc_is_dp_signal(sink_caps.signal))
			dp_trace_init(link);

		 
		for (i = 0; i < sink->dc_edid.length / DC_EDID_BLOCK_SIZE; i++) {
			CONN_DATA_DETECT(link,
					 &sink->dc_edid.raw_edid[i * DC_EDID_BLOCK_SIZE],
					 DC_EDID_BLOCK_SIZE,
					 "%s: [Block %d] ", sink->edid_caps.display_name, i);
		}

		DC_LOG_DETECTION_EDID_PARSER("%s: "
			"manufacturer_id = %X, "
			"product_id = %X, "
			"serial_number = %X, "
			"manufacture_week = %d, "
			"manufacture_year = %d, "
			"display_name = %s, "
			"speaker_flag = %d, "
			"audio_mode_count = %d\n",
			__func__,
			sink->edid_caps.manufacturer_id,
			sink->edid_caps.product_id,
			sink->edid_caps.serial_number,
			sink->edid_caps.manufacture_week,
			sink->edid_caps.manufacture_year,
			sink->edid_caps.display_name,
			sink->edid_caps.speaker_flags,
			sink->edid_caps.audio_mode_count);

		for (i = 0; i < sink->edid_caps.audio_mode_count; i++) {
			DC_LOG_DETECTION_EDID_PARSER("%s: mode number = %d, "
				"format_code = %d, "
				"channel_count = %d, "
				"sample_rate = %d, "
				"sample_size = %d\n",
				__func__,
				i,
				sink->edid_caps.audio_modes[i].format_code,
				sink->edid_caps.audio_modes[i].channel_count,
				sink->edid_caps.audio_modes[i].sample_rate,
				sink->edid_caps.audio_modes[i].sample_size);
		}

		if (link->connector_signal == SIGNAL_TYPE_EDP) {
			 
			if (dc_ctx->dc->res_pool->funcs->get_panel_config_defaults)
				dc_ctx->dc->res_pool->funcs->get_panel_config_defaults(&link->panel_config);
			 
			dm_helpers_init_panel_settings(dc_ctx, &link->panel_config, sink);
			 
			dm_helpers_override_panel_settings(dc_ctx, &link->panel_config);

			 
			if (link->reported_link_cap.link_rate == LINK_RATE_UNKNOWN)
				link->panel_config.ilr.optimize_edp_link_rate = true;
			if (edp_is_ilr_optimization_enabled(link))
				link->reported_link_cap.link_rate = get_max_link_rate_from_ilr_table(link);
		}

	} else {
		 
		link->type = dc_connection_none;
		sink_caps.signal = SIGNAL_TYPE_NONE;
		memset(&link->hdcp_caps, 0, sizeof(struct hdcp_caps));
		 
		link->dongle_max_pix_clk = 0;

		dc_link_clear_dprx_states(link);
		dp_trace_reset(link);
	}

	LINK_INFO("link=%d, dc_sink_in=%p is now %s prev_sink=%p edid same=%d\n",
		  link->link_index, sink,
		  (sink_caps.signal ==
		   SIGNAL_TYPE_NONE ? "Disconnected" : "Connected"),
		  prev_sink, same_edid);

	if (prev_sink)
		dc_sink_release(prev_sink);

	return true;
}

 
bool link_detect_connection_type(struct dc_link *link, enum dc_connection_type *type)
{
	uint32_t is_hpd_high = 0;

	if (link->connector_signal == SIGNAL_TYPE_LVDS) {
		*type = dc_connection_single;
		return true;
	}

	if (link->connector_signal == SIGNAL_TYPE_EDP) {
		 
		if (!link->dc->config.edp_no_power_sequencing)
			link->dc->hwss.edp_power_control(link, true);
		link->dc->hwss.edp_wait_for_hpd_ready(link, true);
	}

	 
	if (link->ep_type != DISPLAY_ENDPOINT_PHY) {
		if (link->is_hpd_pending || !dpia_query_hpd_status(link))
			*type = dc_connection_none;
		else
			*type = dc_connection_single;

		return true;
	}


	if (!query_hpd_status(link, &is_hpd_high))
		goto hpd_gpio_failure;

	if (is_hpd_high) {
		*type = dc_connection_single;
		 
	} else {
		*type = dc_connection_none;
		if (link->connector_signal == SIGNAL_TYPE_EDP) {
			 
			if (!link->dc->config.edp_no_power_sequencing)
				link->dc->hwss.edp_power_control(link, false);
		}
	}

	return true;

hpd_gpio_failure:
	return false;
}

bool link_detect(struct dc_link *link, enum dc_detect_reason reason)
{
	bool is_local_sink_detect_success;
	bool is_delegated_to_mst_top_mgr = false;
	enum dc_connection_type pre_link_type = link->type;

	DC_LOGGER_INIT(link->ctx->logger);

	is_local_sink_detect_success = detect_link_and_local_sink(link, reason);

	if (is_local_sink_detect_success && link->local_sink)
		verify_link_capability(link, link->local_sink, reason);

	DC_LOG_DC("%s: link_index=%d is_local_sink_detect_success=%d pre_link_type=%d link_type=%d\n", __func__,
				link->link_index, is_local_sink_detect_success, pre_link_type, link->type);

	if (is_local_sink_detect_success && link->local_sink &&
			dc_is_dp_signal(link->local_sink->sink_signal) &&
			link->dpcd_caps.is_mst_capable)
		is_delegated_to_mst_top_mgr = discover_dp_mst_topology(link, reason);

	if (is_local_sink_detect_success &&
			pre_link_type == dc_connection_mst_branch &&
			link->type != dc_connection_mst_branch)
		is_delegated_to_mst_top_mgr = link_reset_cur_dp_mst_topology(link);

	return is_local_sink_detect_success && !is_delegated_to_mst_top_mgr;
}

void link_clear_dprx_states(struct dc_link *link)
{
	memset(&link->dprx_states, 0, sizeof(link->dprx_states));
}

bool link_is_hdcp14(struct dc_link *link, enum signal_type signal)
{
	bool ret = false;

	switch (signal)	{
	case SIGNAL_TYPE_DISPLAY_PORT:
	case SIGNAL_TYPE_DISPLAY_PORT_MST:
		ret = link->hdcp_caps.bcaps.bits.HDCP_CAPABLE;
		break;
	case SIGNAL_TYPE_DVI_SINGLE_LINK:
	case SIGNAL_TYPE_DVI_DUAL_LINK:
	case SIGNAL_TYPE_HDMI_TYPE_A:
	 
		ret = true;
		break;
	default:
		break;
	}
	return ret;
}

bool link_is_hdcp22(struct dc_link *link, enum signal_type signal)
{
	bool ret = false;

	switch (signal)	{
	case SIGNAL_TYPE_DISPLAY_PORT:
	case SIGNAL_TYPE_DISPLAY_PORT_MST:
		ret = (link->hdcp_caps.bcaps.bits.HDCP_CAPABLE &&
				link->hdcp_caps.rx_caps.fields.byte0.hdcp_capable &&
				(link->hdcp_caps.rx_caps.fields.version == 0x2)) ? 1 : 0;
		break;
	case SIGNAL_TYPE_DVI_SINGLE_LINK:
	case SIGNAL_TYPE_DVI_DUAL_LINK:
	case SIGNAL_TYPE_HDMI_TYPE_A:
		ret = (link->hdcp_caps.rx_caps.fields.version == 0x4) ? 1:0;
		break;
	default:
		break;
	}

	return ret;
}

const struct dc_link_status *link_get_status(const struct dc_link *link)
{
	return &link->link_status;
}


static bool link_add_remote_sink_helper(struct dc_link *dc_link, struct dc_sink *sink)
{
	if (dc_link->sink_count >= MAX_SINKS_PER_LINK) {
		BREAK_TO_DEBUGGER();
		return false;
	}

	dc_sink_retain(sink);

	dc_link->remote_sinks[dc_link->sink_count] = sink;
	dc_link->sink_count++;

	return true;
}

struct dc_sink *link_add_remote_sink(
		struct dc_link *link,
		const uint8_t *edid,
		int len,
		struct dc_sink_init_data *init_data)
{
	struct dc_sink *dc_sink;
	enum dc_edid_status edid_status;

	if (len > DC_MAX_EDID_BUFFER_SIZE) {
		dm_error("Max EDID buffer size breached!\n");
		return NULL;
	}

	if (!init_data) {
		BREAK_TO_DEBUGGER();
		return NULL;
	}

	if (!init_data->link) {
		BREAK_TO_DEBUGGER();
		return NULL;
	}

	dc_sink = dc_sink_create(init_data);

	if (!dc_sink)
		return NULL;

	memmove(dc_sink->dc_edid.raw_edid, edid, len);
	dc_sink->dc_edid.length = len;

	if (!link_add_remote_sink_helper(
			link,
			dc_sink))
		goto fail_add_sink;

	edid_status = dm_helpers_parse_edid_caps(
			link,
			&dc_sink->dc_edid,
			&dc_sink->edid_caps);

	 
	if (edid_status != EDID_OK && edid_status != EDID_PARTIAL_VALID) {
		dc_sink->dc_edid.length = 0;
		dm_error("Bad EDID, status%d!\n", edid_status);
	}

	return dc_sink;

fail_add_sink:
	dc_sink_release(dc_sink);
	return NULL;
}

void link_remove_remote_sink(struct dc_link *link, struct dc_sink *sink)
{
	int i;

	if (!link->sink_count) {
		BREAK_TO_DEBUGGER();
		return;
	}

	for (i = 0; i < link->sink_count; i++) {
		if (link->remote_sinks[i] == sink) {
			dc_sink_release(sink);
			link->remote_sinks[i] = NULL;

			 
			while (i < link->sink_count - 1) {
				link->remote_sinks[i] = link->remote_sinks[i+1];
				i++;
			}
			link->remote_sinks[i] = NULL;
			link->sink_count--;
			return;
		}
	}
}
