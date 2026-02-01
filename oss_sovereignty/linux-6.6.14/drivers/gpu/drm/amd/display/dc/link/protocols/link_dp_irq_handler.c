 

 

#include "link_dp_irq_handler.h"
#include "link_dpcd.h"
#include "link_dp_training.h"
#include "link_dp_capability.h"
#include "link_edp_panel_control.h"
#include "link/accessories/link_dp_trace.h"
#include "link/link_dpms.h"
#include "dm_helpers.h"

#define DC_LOGGER_INIT(logger)

bool dp_parse_link_loss_status(
	struct dc_link *link,
	union hpd_irq_data *hpd_irq_dpcd_data)
{
	uint8_t irq_reg_rx_power_state = 0;
	enum dc_status dpcd_result = DC_ERROR_UNEXPECTED;
	union lane_status lane_status;
	uint32_t lane;
	bool sink_status_changed;
	bool return_code;

	sink_status_changed = false;
	return_code = false;

	if (link->cur_link_settings.lane_count == 0)
		return return_code;

	 

	 
	for (lane = 0; lane < link->cur_link_settings.lane_count; lane++) {
		 
		lane_status.raw = dp_get_nibble_at_index(
			&hpd_irq_dpcd_data->bytes.lane01_status.raw,
			lane);

		if (!lane_status.bits.CHANNEL_EQ_DONE_0 ||
			!lane_status.bits.CR_DONE_0 ||
			!lane_status.bits.SYMBOL_LOCKED_0) {
			 
			sink_status_changed = true;
			break;
		}
	}

	 
	if (link_dp_get_encoding_format(&link->cur_link_settings) == DP_128b_132b_ENCODING &&
			(!hpd_irq_dpcd_data->bytes.lane_status_updated.bits.EQ_INTERLANE_ALIGN_DONE_128b_132b ||
			 !hpd_irq_dpcd_data->bytes.lane_status_updated.bits.CDS_INTERLANE_ALIGN_DONE_128b_132b)) {
		sink_status_changed = true;
	} else if (!hpd_irq_dpcd_data->bytes.lane_status_updated.bits.INTERLANE_ALIGN_DONE) {
		sink_status_changed = true;
	}

	if (sink_status_changed) {

		DC_LOG_HW_HPD_IRQ("%s: Link Status changed.\n", __func__);

		return_code = true;

		 
		dpcd_result = core_link_read_dpcd(link,
			DP_SET_POWER,
			&irq_reg_rx_power_state,
			sizeof(irq_reg_rx_power_state));

		if (dpcd_result != DC_OK) {
			DC_LOG_HW_HPD_IRQ("%s: DPCD read failed to obtain power state.\n",
				__func__);
		} else {
			if (irq_reg_rx_power_state != DP_SET_POWER_D0)
				return_code = false;
		}
	}

	return return_code;
}

static bool handle_hpd_irq_psr_sink(struct dc_link *link)
{
	union dpcd_psr_configuration psr_configuration;

	if (!link->psr_settings.psr_feature_enabled)
		return false;

	dm_helpers_dp_read_dpcd(
		link->ctx,
		link,
		368, 
		&psr_configuration.raw,
		sizeof(psr_configuration.raw));

	if (psr_configuration.bits.ENABLE) {
		unsigned char dpcdbuf[3] = {0};
		union psr_error_status psr_error_status;
		union psr_sink_psr_status psr_sink_psr_status;

		dm_helpers_dp_read_dpcd(
			link->ctx,
			link,
			0x2006,  
			(unsigned char *) dpcdbuf,
			sizeof(dpcdbuf));

		 
		psr_error_status.raw = dpcdbuf[0];
		 
		psr_sink_psr_status.raw = dpcdbuf[2];

		if (psr_error_status.bits.LINK_CRC_ERROR ||
				psr_error_status.bits.RFB_STORAGE_ERROR ||
				psr_error_status.bits.VSC_SDP_ERROR) {
			bool allow_active;

			 
			dm_helpers_dp_write_dpcd(
				link->ctx,
				link,
				8198, 
				&psr_error_status.raw,
				sizeof(psr_error_status.raw));

			 
			if (link->psr_settings.psr_allow_active) {
				allow_active = false;
				edp_set_psr_allow_active(link, &allow_active, true, false, NULL);
				allow_active = true;
				edp_set_psr_allow_active(link, &allow_active, true, false, NULL);
			}

			return true;
		} else if (psr_sink_psr_status.bits.SINK_SELF_REFRESH_STATUS ==
				PSR_SINK_STATE_ACTIVE_DISPLAY_FROM_SINK_RFB){
			 
			return true;
		}
	}
	return false;
}

static bool handle_hpd_irq_replay_sink(struct dc_link *link)
{
	union dpcd_replay_configuration replay_configuration;
	 
	union psr_error_status replay_error_status;

	if (!link->replay_settings.replay_feature_enabled)
		return false;

	dm_helpers_dp_read_dpcd(
		link->ctx,
		link,
		DP_SINK_PR_REPLAY_STATUS,
		&replay_configuration.raw,
		sizeof(replay_configuration.raw));

	dm_helpers_dp_read_dpcd(
		link->ctx,
		link,
		DP_PSR_ERROR_STATUS,
		&replay_error_status.raw,
		sizeof(replay_error_status.raw));

	link->replay_settings.config.replay_error_status.bits.LINK_CRC_ERROR =
		replay_error_status.bits.LINK_CRC_ERROR;
	link->replay_settings.config.replay_error_status.bits.DESYNC_ERROR =
		replay_configuration.bits.DESYNC_ERROR_STATUS;
	link->replay_settings.config.replay_error_status.bits.STATE_TRANSITION_ERROR =
		replay_configuration.bits.STATE_TRANSITION_ERROR_STATUS;

	if (link->replay_settings.config.replay_error_status.bits.LINK_CRC_ERROR ||
		link->replay_settings.config.replay_error_status.bits.DESYNC_ERROR ||
		link->replay_settings.config.replay_error_status.bits.STATE_TRANSITION_ERROR) {
		bool allow_active;

		 
		dm_helpers_dp_write_dpcd(
			link->ctx,
			link,
			DP_SINK_PR_REPLAY_STATUS,
			&replay_configuration.raw,
			sizeof(replay_configuration.raw));

		 
		dm_helpers_dp_write_dpcd(
			link->ctx,
			link,
			DP_PSR_ERROR_STATUS, 
			&replay_error_status.raw,
			sizeof(replay_error_status.raw));

		 
		if (link->replay_settings.replay_allow_active) {
			allow_active = false;
			edp_set_replay_allow_active(link, &allow_active, true, false, NULL);
			allow_active = true;
			edp_set_replay_allow_active(link, &allow_active, true, false, NULL);
		}
	}
	return true;
}

void dp_handle_link_loss(struct dc_link *link)
{
	struct pipe_ctx *pipes[MAX_PIPES];
	struct dc_state *state = link->dc->current_state;
	uint8_t count;
	int i;

	link_get_master_pipes_with_dpms_on(link, state, &count, pipes);

	for (i = 0; i < count; i++)
		link_set_dpms_off(pipes[i]);

	for (i = count - 1; i >= 0; i--) {
		
		if (link->is_automated) {
			pipes[i]->link_config.dp_link_settings.lane_count =
					link->verified_link_cap.lane_count;
			pipes[i]->link_config.dp_link_settings.link_rate =
					link->verified_link_cap.link_rate;
			pipes[i]->link_config.dp_link_settings.link_spread =
					link->verified_link_cap.link_spread;
		}
		link_set_dpms_on(link->dc->current_state, pipes[i]);
	}
}

static void read_dpcd204h_on_irq_hpd(struct dc_link *link, union hpd_irq_data *irq_data)
{
	enum dc_status retval;
	union lane_align_status_updated dpcd_lane_status_updated;

	retval = core_link_read_dpcd(
			link,
			DP_LANE_ALIGN_STATUS_UPDATED,
			&dpcd_lane_status_updated.raw,
			sizeof(union lane_align_status_updated));

	if (retval == DC_OK) {
		irq_data->bytes.lane_status_updated.bits.EQ_INTERLANE_ALIGN_DONE_128b_132b =
				dpcd_lane_status_updated.bits.EQ_INTERLANE_ALIGN_DONE_128b_132b;
		irq_data->bytes.lane_status_updated.bits.CDS_INTERLANE_ALIGN_DONE_128b_132b =
				dpcd_lane_status_updated.bits.CDS_INTERLANE_ALIGN_DONE_128b_132b;
	}
}

enum dc_status dp_read_hpd_rx_irq_data(
	struct dc_link *link,
	union hpd_irq_data *irq_data)
{
	static enum dc_status retval;

	 
	if (link->dpcd_caps.dpcd_rev.raw < DPCD_REV_14)
		retval = core_link_read_dpcd(
			link,
			DP_SINK_COUNT,
			irq_data->raw,
			sizeof(union hpd_irq_data));
	else {
		 

		uint8_t tmp[DP_SINK_STATUS_ESI - DP_SINK_COUNT_ESI + 1];

		retval = core_link_read_dpcd(
			link,
			DP_SINK_COUNT_ESI,
			tmp,
			sizeof(tmp));

		if (retval != DC_OK)
			return retval;

		irq_data->bytes.sink_cnt.raw = tmp[DP_SINK_COUNT_ESI - DP_SINK_COUNT_ESI];
		irq_data->bytes.device_service_irq.raw = tmp[DP_DEVICE_SERVICE_IRQ_VECTOR_ESI0 - DP_SINK_COUNT_ESI];
		irq_data->bytes.lane01_status.raw = tmp[DP_LANE0_1_STATUS_ESI - DP_SINK_COUNT_ESI];
		irq_data->bytes.lane23_status.raw = tmp[DP_LANE2_3_STATUS_ESI - DP_SINK_COUNT_ESI];
		irq_data->bytes.lane_status_updated.raw = tmp[DP_LANE_ALIGN_STATUS_UPDATED_ESI - DP_SINK_COUNT_ESI];
		irq_data->bytes.sink_status.raw = tmp[DP_SINK_STATUS_ESI - DP_SINK_COUNT_ESI];

		 
		if (link->wa_flags.read_dpcd204h_on_irq_hpd)
			read_dpcd204h_on_irq_hpd(link, irq_data);
	}

	return retval;
}

 
bool dp_should_allow_hpd_rx_irq(const struct dc_link *link)
{
	 

	if ((link->cur_link_settings.lane_count != LANE_COUNT_UNKNOWN) ||
		is_dp_branch_device(link))
		return true;

	return false;
}

bool dp_handle_hpd_rx_irq(struct dc_link *link,
		union hpd_irq_data *out_hpd_irq_dpcd_data, bool *out_link_loss,
		bool defer_handling, bool *has_left_work)
{
	union hpd_irq_data hpd_irq_dpcd_data = {0};
	union device_service_irq device_service_clear = {0};
	enum dc_status result;
	bool status = false;

	if (out_link_loss)
		*out_link_loss = false;

	if (has_left_work)
		*has_left_work = false;
	 

	DC_LOG_HW_HPD_IRQ("%s: Got short pulse HPD on link %d\n",
		__func__, link->link_index);


	  
	result = dp_read_hpd_rx_irq_data(link, &hpd_irq_dpcd_data);
	if (out_hpd_irq_dpcd_data)
		*out_hpd_irq_dpcd_data = hpd_irq_dpcd_data;

	if (result != DC_OK) {
		DC_LOG_HW_HPD_IRQ("%s: DPCD read failed to obtain irq data\n",
			__func__);
		return false;
	}

	if (hpd_irq_dpcd_data.bytes.device_service_irq.bits.AUTOMATED_TEST) {
		 
		link->is_automated = true;
		device_service_clear.bits.AUTOMATED_TEST = 1;
		core_link_write_dpcd(
			link,
			DP_DEVICE_SERVICE_IRQ_VECTOR,
			&device_service_clear.raw,
			sizeof(device_service_clear.raw));
		device_service_clear.raw = 0;
		if (defer_handling && has_left_work)
			*has_left_work = true;
		else
			dc_link_dp_handle_automated_test(link);
		return false;
	}

	if (!dp_should_allow_hpd_rx_irq(link)) {
		DC_LOG_HW_HPD_IRQ("%s: skipping HPD handling on %d\n",
			__func__, link->link_index);
		return false;
	}

	if (handle_hpd_irq_psr_sink(link))
		 
		return true;

	if (handle_hpd_irq_replay_sink(link))
		 
		return true;

	 

	if (hpd_irq_dpcd_data.bytes.device_service_irq.bits.UP_REQ_MSG_RDY) {
		if (defer_handling && has_left_work)
			*has_left_work = true;
		return true;
	}

	 
	if (hpd_irq_dpcd_data.bytes.device_service_irq.bits.DOWN_REP_MSG_RDY) {
		if (defer_handling && has_left_work)
			*has_left_work = true;
		return false;
	}

	 
	if ((link->connector_signal != SIGNAL_TYPE_EDP) &&
			dp_parse_link_loss_status(
					link,
					&hpd_irq_dpcd_data)) {
		 
		CONN_DATA_LINK_LOSS(link,
					hpd_irq_dpcd_data.raw,
					sizeof(hpd_irq_dpcd_data),
					"Status: ");

		if (defer_handling && has_left_work)
			*has_left_work = true;
		else
			dp_handle_link_loss(link);

		status = false;
		if (out_link_loss)
			*out_link_loss = true;

		dp_trace_link_loss_increment(link);
	}

	if (link->type == dc_connection_sst_branch &&
		hpd_irq_dpcd_data.bytes.sink_cnt.bits.SINK_COUNT
			!= link->dpcd_sink_count)
		status = true;

	 
	return status;
}
