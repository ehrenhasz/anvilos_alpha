 


#include "reg_helper.h"

#include "core_types.h"
#include "link_encoder.h"
#include "dcn31_dio_link_encoder.h"
#include "stream_encoder.h"
#include "dc_bios_types.h"

#include "gpio_service_interface.h"

#include "link_enc_cfg.h"
#include "dc_dmub_srv.h"
#include "dal_asic_id.h"
#include "link.h"

#define CTX \
	enc10->base.ctx
#define DC_LOGGER \
	enc10->base.ctx->logger

#define REG(reg)\
	(enc10->link_regs->reg)

#undef FN
#define FN(reg_name, field_name) \
	enc10->link_shift->field_name, enc10->link_mask->field_name

#define IND_REG(index) \
	(enc10->link_regs->index)

#define AUX_REG(reg)\
	(enc10->aux_regs->reg)

#define AUX_REG_READ(reg_name) \
		dm_read_reg(CTX, AUX_REG(reg_name))

#define AUX_REG_WRITE(reg_name, val) \
			dm_write_reg(CTX, AUX_REG(reg_name), val)

#ifndef MIN
#define MIN(X, Y) ((X) < (Y) ? (X) : (Y))
#endif

static uint8_t phy_id_from_transmitter(enum transmitter t)
{
	uint8_t phy_id;

	switch (t) {
	case TRANSMITTER_UNIPHY_A:
		phy_id = 0;
		break;
	case TRANSMITTER_UNIPHY_B:
		phy_id = 1;
		break;
	case TRANSMITTER_UNIPHY_C:
		phy_id = 2;
		break;
	case TRANSMITTER_UNIPHY_D:
		phy_id = 3;
		break;
	case TRANSMITTER_UNIPHY_E:
		phy_id = 4;
		break;
	case TRANSMITTER_UNIPHY_F:
		phy_id = 5;
		break;
	case TRANSMITTER_UNIPHY_G:
		phy_id = 6;
		break;
	default:
		phy_id = 0;
		break;
	}
	return phy_id;
}

static bool has_query_dp_alt(struct link_encoder *enc)
{
	struct dc_dmub_srv *dc_dmub_srv = enc->ctx->dmub_srv;

	if (enc->ctx->dce_version >= DCN_VERSION_3_15)
		return true;

	 
	return dc_dmub_srv &&
	       !(dc_dmub_srv->dmub->fw_version >= DMUB_FW_VERSION(4, 0, 0) &&
		 dc_dmub_srv->dmub->fw_version <= DMUB_FW_VERSION(4, 0, 10));
}

static bool query_dp_alt_from_dmub(struct link_encoder *enc,
				   union dmub_rb_cmd *cmd)
{
	struct dcn10_link_encoder *enc10 = TO_DCN10_LINK_ENC(enc);

	memset(cmd, 0, sizeof(*cmd));
	cmd->query_dp_alt.header.type = DMUB_CMD__VBIOS;
	cmd->query_dp_alt.header.sub_type =
		DMUB_CMD__VBIOS_TRANSMITTER_QUERY_DP_ALT;
	cmd->query_dp_alt.header.payload_bytes = sizeof(cmd->query_dp_alt.data);
	cmd->query_dp_alt.data.phy_id = phy_id_from_transmitter(enc10->base.transmitter);

	if (!dm_execute_dmub_cmd(enc->ctx, cmd, DM_DMUB_WAIT_TYPE_WAIT_WITH_REPLY))
		return false;

	return true;
}

void dcn31_link_encoder_set_dio_phy_mux(
	struct link_encoder *enc,
	enum encoder_type_select sel,
	uint32_t hpo_inst)
{
	struct dcn10_link_encoder *enc10 = TO_DCN10_LINK_ENC(enc);

	switch (enc->transmitter) {
	case TRANSMITTER_UNIPHY_A:
		if (sel == ENCODER_TYPE_HDMI_FRL)
			REG_UPDATE(DIO_LINKA_CNTL,
					HPO_HDMI_ENC_SEL, hpo_inst);
		else if (sel == ENCODER_TYPE_DP_128B132B)
			REG_UPDATE(DIO_LINKA_CNTL,
					HPO_DP_ENC_SEL, hpo_inst);
		REG_UPDATE(DIO_LINKA_CNTL,
				ENC_TYPE_SEL, sel);
		break;
	case TRANSMITTER_UNIPHY_B:
		if (sel == ENCODER_TYPE_HDMI_FRL)
			REG_UPDATE(DIO_LINKB_CNTL,
					HPO_HDMI_ENC_SEL, hpo_inst);
		else if (sel == ENCODER_TYPE_DP_128B132B)
			REG_UPDATE(DIO_LINKB_CNTL,
					HPO_DP_ENC_SEL, hpo_inst);
		REG_UPDATE(DIO_LINKB_CNTL,
				ENC_TYPE_SEL, sel);
		break;
	case TRANSMITTER_UNIPHY_C:
		if (sel == ENCODER_TYPE_HDMI_FRL)
			REG_UPDATE(DIO_LINKC_CNTL,
					HPO_HDMI_ENC_SEL, hpo_inst);
		else if (sel == ENCODER_TYPE_DP_128B132B)
			REG_UPDATE(DIO_LINKC_CNTL,
					HPO_DP_ENC_SEL, hpo_inst);
		REG_UPDATE(DIO_LINKC_CNTL,
				ENC_TYPE_SEL, sel);
		break;
	case TRANSMITTER_UNIPHY_D:
		if (sel == ENCODER_TYPE_HDMI_FRL)
			REG_UPDATE(DIO_LINKD_CNTL,
					HPO_HDMI_ENC_SEL, hpo_inst);
		else if (sel == ENCODER_TYPE_DP_128B132B)
			REG_UPDATE(DIO_LINKD_CNTL,
					HPO_DP_ENC_SEL, hpo_inst);
		REG_UPDATE(DIO_LINKD_CNTL,
				ENC_TYPE_SEL, sel);
		break;
	case TRANSMITTER_UNIPHY_E:
		if (sel == ENCODER_TYPE_HDMI_FRL)
			REG_UPDATE(DIO_LINKE_CNTL,
					HPO_HDMI_ENC_SEL, hpo_inst);
		else if (sel == ENCODER_TYPE_DP_128B132B)
			REG_UPDATE(DIO_LINKE_CNTL,
					HPO_DP_ENC_SEL, hpo_inst);
		REG_UPDATE(DIO_LINKE_CNTL,
				ENC_TYPE_SEL, sel);
		break;
	case TRANSMITTER_UNIPHY_F:
		if (sel == ENCODER_TYPE_HDMI_FRL)
			REG_UPDATE(DIO_LINKF_CNTL,
					HPO_HDMI_ENC_SEL, hpo_inst);
		else if (sel == ENCODER_TYPE_DP_128B132B)
			REG_UPDATE(DIO_LINKF_CNTL,
					HPO_DP_ENC_SEL, hpo_inst);
		REG_UPDATE(DIO_LINKF_CNTL,
				ENC_TYPE_SEL, sel);
		break;
	default:
		 
		break;
	}
}

static void enc31_hw_init(struct link_encoder *enc)
{
	struct dcn10_link_encoder *enc10 = TO_DCN10_LINK_ENC(enc);

 

 
	

	
	
	
	
	

	
	REG_UPDATE(TMDS_CTL_BITS, TMDS_CTL0, 1);

	dcn10_aux_initialize(enc10);
}

static const struct link_encoder_funcs dcn31_link_enc_funcs = {
	.read_state = link_enc2_read_state,
	.validate_output_with_stream =
			dcn30_link_encoder_validate_output_with_stream,
	.hw_init = enc31_hw_init,
	.setup = dcn10_link_encoder_setup,
	.enable_tmds_output = dcn10_link_encoder_enable_tmds_output,
	.enable_dp_output = dcn31_link_encoder_enable_dp_output,
	.enable_dp_mst_output = dcn31_link_encoder_enable_dp_mst_output,
	.disable_output = dcn31_link_encoder_disable_output,
	.dp_set_lane_settings = dcn10_link_encoder_dp_set_lane_settings,
	.dp_set_phy_pattern = dcn10_link_encoder_dp_set_phy_pattern,
	.update_mst_stream_allocation_table =
		dcn10_link_encoder_update_mst_stream_allocation_table,
	.psr_program_dp_dphy_fast_training =
			dcn10_psr_program_dp_dphy_fast_training,
	.psr_program_secondary_packet = dcn10_psr_program_secondary_packet,
	.connect_dig_be_to_fe = dcn10_link_encoder_connect_dig_be_to_fe,
	.enable_hpd = dcn10_link_encoder_enable_hpd,
	.disable_hpd = dcn10_link_encoder_disable_hpd,
	.is_dig_enabled = dcn10_is_dig_enabled,
	.destroy = dcn10_link_encoder_destroy,
	.fec_set_enable = enc2_fec_set_enable,
	.fec_set_ready = enc2_fec_set_ready,
	.fec_is_active = enc2_fec_is_active,
	.get_dig_frontend = dcn10_get_dig_frontend,
	.get_dig_mode = dcn10_get_dig_mode,
	.is_in_alt_mode = dcn31_link_encoder_is_in_alt_mode,
	.get_max_link_cap = dcn31_link_encoder_get_max_link_cap,
	.set_dio_phy_mux = dcn31_link_encoder_set_dio_phy_mux,
};

void dcn31_link_encoder_construct(
	struct dcn20_link_encoder *enc20,
	const struct encoder_init_data *init_data,
	const struct encoder_feature_support *enc_features,
	const struct dcn10_link_enc_registers *link_regs,
	const struct dcn10_link_enc_aux_registers *aux_regs,
	const struct dcn10_link_enc_hpd_registers *hpd_regs,
	const struct dcn10_link_enc_shift *link_shift,
	const struct dcn10_link_enc_mask *link_mask)
{
	struct bp_encoder_cap_info bp_cap_info = {0};
	const struct dc_vbios_funcs *bp_funcs = init_data->ctx->dc_bios->funcs;
	enum bp_result result = BP_RESULT_OK;
	struct dcn10_link_encoder *enc10 = &enc20->enc10;

	enc10->base.funcs = &dcn31_link_enc_funcs;
	enc10->base.ctx = init_data->ctx;
	enc10->base.id = init_data->encoder;

	enc10->base.hpd_source = init_data->hpd_source;
	enc10->base.connector = init_data->connector;

	enc10->base.preferred_engine = ENGINE_ID_UNKNOWN;

	enc10->base.features = *enc_features;

	enc10->base.transmitter = init_data->transmitter;

	 

 

	enc10->base.output_signals =
		SIGNAL_TYPE_DVI_SINGLE_LINK |
		SIGNAL_TYPE_DVI_DUAL_LINK |
		SIGNAL_TYPE_LVDS |
		SIGNAL_TYPE_DISPLAY_PORT |
		SIGNAL_TYPE_DISPLAY_PORT_MST |
		SIGNAL_TYPE_EDP |
		SIGNAL_TYPE_HDMI_TYPE_A;

	 

	enc10->link_regs = link_regs;
	enc10->aux_regs = aux_regs;
	enc10->hpd_regs = hpd_regs;
	enc10->link_shift = link_shift;
	enc10->link_mask = link_mask;

	switch (enc10->base.transmitter) {
	case TRANSMITTER_UNIPHY_A:
		enc10->base.preferred_engine = ENGINE_ID_DIGA;
	break;
	case TRANSMITTER_UNIPHY_B:
		enc10->base.preferred_engine = ENGINE_ID_DIGB;
	break;
	case TRANSMITTER_UNIPHY_C:
		enc10->base.preferred_engine = ENGINE_ID_DIGC;
	break;
	case TRANSMITTER_UNIPHY_D:
		enc10->base.preferred_engine = ENGINE_ID_DIGD;
	break;
	case TRANSMITTER_UNIPHY_E:
		enc10->base.preferred_engine = ENGINE_ID_DIGE;
	break;
	case TRANSMITTER_UNIPHY_F:
		enc10->base.preferred_engine = ENGINE_ID_DIGF;
	break;
	default:
		ASSERT_CRITICAL(false);
		enc10->base.preferred_engine = ENGINE_ID_UNKNOWN;
	}

	 
	enc10->base.features.flags.bits.HDMI_6GB_EN = 1;

	result = bp_funcs->get_encoder_cap_info(enc10->base.ctx->dc_bios,
						enc10->base.id, &bp_cap_info);

	 
	if (result == BP_RESULT_OK) {
		enc10->base.features.flags.bits.IS_HBR2_CAPABLE =
				bp_cap_info.DP_HBR2_EN;
		enc10->base.features.flags.bits.IS_HBR3_CAPABLE =
				bp_cap_info.DP_HBR3_EN;
		enc10->base.features.flags.bits.HDMI_6GB_EN = bp_cap_info.HDMI_6GB_EN;
		enc10->base.features.flags.bits.IS_DP2_CAPABLE = bp_cap_info.IS_DP2_CAPABLE;
		enc10->base.features.flags.bits.IS_UHBR10_CAPABLE = bp_cap_info.DP_UHBR10_EN;
		enc10->base.features.flags.bits.IS_UHBR13_5_CAPABLE = bp_cap_info.DP_UHBR13_5_EN;
		enc10->base.features.flags.bits.IS_UHBR20_CAPABLE = bp_cap_info.DP_UHBR20_EN;
		enc10->base.features.flags.bits.DP_IS_USB_C =
				bp_cap_info.DP_IS_USB_C;
	} else {
		DC_LOG_WARNING("%s: Failed to get encoder_cap_info from VBIOS with error code %d!\n",
				__func__,
				result);
	}
	if (enc10->base.ctx->dc->debug.hdmi20_disable) {
		enc10->base.features.flags.bits.HDMI_6GB_EN = 0;
	}
}

void dcn31_link_encoder_construct_minimal(
	struct dcn20_link_encoder *enc20,
	struct dc_context *ctx,
	const struct encoder_feature_support *enc_features,
	const struct dcn10_link_enc_registers *link_regs,
	enum engine_id eng_id)
{
	struct dcn10_link_encoder *enc10 = &enc20->enc10;

	enc10->base.funcs = &dcn31_link_enc_funcs;
	enc10->base.ctx = ctx;
	enc10->base.id.type = OBJECT_TYPE_ENCODER;
	enc10->base.hpd_source = HPD_SOURCEID_UNKNOWN;
	enc10->base.connector.type = OBJECT_TYPE_CONNECTOR;
	enc10->base.preferred_engine = eng_id;
	enc10->base.features = *enc_features;
	enc10->base.transmitter = TRANSMITTER_UNKNOWN;
	enc10->link_regs = link_regs;

	enc10->base.output_signals =
		SIGNAL_TYPE_DISPLAY_PORT |
		SIGNAL_TYPE_DISPLAY_PORT_MST |
		SIGNAL_TYPE_EDP;
}

 
static bool link_dpia_control(struct dc_context *dc_ctx,
	struct dmub_cmd_dig_dpia_control_data *dpia_control)
{
	union dmub_rb_cmd cmd;

	memset(&cmd, 0, sizeof(cmd));

	cmd.dig1_dpia_control.header.type = DMUB_CMD__DPIA;
	cmd.dig1_dpia_control.header.sub_type =
			DMUB_CMD__DPIA_DIG1_DPIA_CONTROL;
	cmd.dig1_dpia_control.header.payload_bytes =
		sizeof(cmd.dig1_dpia_control) -
		sizeof(cmd.dig1_dpia_control.header);

	cmd.dig1_dpia_control.dpia_control = *dpia_control;

	dm_execute_dmub_cmd(dc_ctx, &cmd, DM_DMUB_WAIT_TYPE_WAIT);

	return true;
}

static void link_encoder_disable(struct dcn10_link_encoder *enc10)
{
	 
	REG_UPDATE(DP_LINK_CNTL, DP_LINK_TRAINING_COMPLETE, 0);
}

void dcn31_link_encoder_enable_dp_output(
	struct link_encoder *enc,
	const struct dc_link_settings *link_settings,
	enum clock_source_id clock_source)
{
	struct dcn10_link_encoder *enc10 = TO_DCN10_LINK_ENC(enc);

	 
	if (!link_enc_cfg_is_transmitter_mappable(enc->ctx->dc, enc)) {

		DC_LOG_DEBUG("%s: enc_id(%d)\n", __func__, enc->preferred_engine);
		dcn20_link_encoder_enable_dp_output(enc, link_settings, clock_source);

	} else {

		struct dmub_cmd_dig_dpia_control_data dpia_control = { 0 };
		struct dc_link *link;

		link = link_enc_cfg_get_link_using_link_enc(enc->ctx->dc, enc->preferred_engine);

		enc1_configure_encoder(enc10, link_settings);

		dpia_control.action = (uint8_t)TRANSMITTER_CONTROL_ENABLE;
		dpia_control.enc_id = enc->preferred_engine;
		dpia_control.mode_laneset.digmode = 0;  
		dpia_control.lanenum = (uint8_t)link_settings->lane_count;
		dpia_control.symclk_10khz = link_settings->link_rate *
				LINK_RATE_REF_FREQ_IN_KHZ / 10;
		 
		dpia_control.hpdsel = 6;

		if (link) {
			dpia_control.dpia_id = link->ddc_hw_inst;
			dpia_control.fec_rdy = link->dc->link_srv->dp_should_enable_fec(link);
		} else {
			DC_LOG_ERROR("%s: Failed to execute DPIA enable DMUB command.\n", __func__);
			BREAK_TO_DEBUGGER();
			return;
		}

		DC_LOG_DEBUG("%s: DPIA(%d) - enc_id(%d)\n", __func__, dpia_control.dpia_id, dpia_control.enc_id);
		link_dpia_control(enc->ctx, &dpia_control);
	}
}

void dcn31_link_encoder_enable_dp_mst_output(
	struct link_encoder *enc,
	const struct dc_link_settings *link_settings,
	enum clock_source_id clock_source)
{
	struct dcn10_link_encoder *enc10 = TO_DCN10_LINK_ENC(enc);

	 
	if (!link_enc_cfg_is_transmitter_mappable(enc->ctx->dc, enc)) {

		DC_LOG_DEBUG("%s: enc_id(%d)\n", __func__, enc->preferred_engine);
		dcn10_link_encoder_enable_dp_mst_output(enc, link_settings, clock_source);

	} else {

		struct dmub_cmd_dig_dpia_control_data dpia_control = { 0 };
		struct dc_link *link;

		link = link_enc_cfg_get_link_using_link_enc(enc->ctx->dc, enc->preferred_engine);

		enc1_configure_encoder(enc10, link_settings);

		dpia_control.action = (uint8_t)TRANSMITTER_CONTROL_ENABLE;
		dpia_control.enc_id = enc->preferred_engine;
		dpia_control.mode_laneset.digmode = 5;  
		dpia_control.lanenum = (uint8_t)link_settings->lane_count;
		dpia_control.symclk_10khz = link_settings->link_rate *
				LINK_RATE_REF_FREQ_IN_KHZ / 10;
		 
		dpia_control.hpdsel = 6;

		if (link) {
			dpia_control.dpia_id = link->ddc_hw_inst;
			dpia_control.fec_rdy = link->dc->link_srv->dp_should_enable_fec(link);
		} else {
			DC_LOG_ERROR("%s: Failed to execute DPIA enable DMUB command.\n", __func__);
			BREAK_TO_DEBUGGER();
			return;
		}

		DC_LOG_DEBUG("%s: DPIA(%d) - enc_id(%d)\n", __func__, dpia_control.dpia_id, dpia_control.enc_id);
		link_dpia_control(enc->ctx, &dpia_control);
	}
}

void dcn31_link_encoder_disable_output(
	struct link_encoder *enc,
	enum signal_type signal)
{
	struct dcn10_link_encoder *enc10 = TO_DCN10_LINK_ENC(enc);

	 
	if (!link_enc_cfg_is_transmitter_mappable(enc->ctx->dc, enc)) {

		DC_LOG_DEBUG("%s: enc_id(%d)\n", __func__, enc->preferred_engine);
		dcn10_link_encoder_disable_output(enc, signal);

	} else {

		struct dmub_cmd_dig_dpia_control_data dpia_control = { 0 };
		struct dc_link *link;

		if (enc->funcs->is_dig_enabled && !enc->funcs->is_dig_enabled(enc))
			return;

		link = link_enc_cfg_get_link_using_link_enc(enc->ctx->dc, enc->preferred_engine);

		dpia_control.action = (uint8_t)TRANSMITTER_CONTROL_DISABLE;
		dpia_control.enc_id = enc->preferred_engine;
		if (signal == SIGNAL_TYPE_DISPLAY_PORT) {
			dpia_control.mode_laneset.digmode = 0;  
		} else if (signal == SIGNAL_TYPE_DISPLAY_PORT_MST) {
			dpia_control.mode_laneset.digmode = 5;  
		} else {
			DC_LOG_ERROR("%s: USB4 DPIA only supports DisplayPort.\n", __func__);
			BREAK_TO_DEBUGGER();
		}

		if (link) {
			dpia_control.dpia_id = link->ddc_hw_inst;
		} else {
			DC_LOG_ERROR("%s: Failed to execute DPIA enable DMUB command.\n", __func__);
			BREAK_TO_DEBUGGER();
			return;
		}

		DC_LOG_DEBUG("%s: DPIA(%d) - enc_id(%d)\n", __func__, dpia_control.dpia_id, dpia_control.enc_id);
		link_dpia_control(enc->ctx, &dpia_control);

		link_encoder_disable(enc10);
	}
}

bool dcn31_link_encoder_is_in_alt_mode(struct link_encoder *enc)
{
	struct dcn10_link_encoder *enc10 = TO_DCN10_LINK_ENC(enc);
	union dmub_rb_cmd cmd;
	uint32_t dp_alt_mode_disable;

	 
	if (!enc->features.flags.bits.DP_IS_USB_C)
		return false;

	 
	if (has_query_dp_alt(enc)) {
		if (!query_dp_alt_from_dmub(enc, &cmd))
			return false;

		return (cmd.query_dp_alt.data.is_dp_alt_disable == 0);
	}

	 
	if (enc->ctx->asic_id.hw_internal_rev != YELLOW_CARP_B0) {
		REG_GET(RDPCSTX_PHY_CNTL6, RDPCS_PHY_DPALT_DISABLE,
			&dp_alt_mode_disable);
	} else {
		 
		if ((enc10->base.transmitter == TRANSMITTER_UNIPHY_A) ||
		    (enc10->base.transmitter == TRANSMITTER_UNIPHY_B) ||
		    (enc10->base.transmitter == TRANSMITTER_UNIPHY_E)) {
			REG_GET(RDPCSTX_PHY_CNTL6, RDPCS_PHY_DPALT_DISABLE,
				&dp_alt_mode_disable);
		} else {
			REG_GET(RDPCSPIPE_PHY_CNTL6, RDPCS_PHY_DPALT_DISABLE,
				&dp_alt_mode_disable);
		}
	}

	return (dp_alt_mode_disable == 0);
}

void dcn31_link_encoder_get_max_link_cap(struct link_encoder *enc, struct dc_link_settings *link_settings)
{
	struct dcn10_link_encoder *enc10 = TO_DCN10_LINK_ENC(enc);
	union dmub_rb_cmd cmd;
	uint32_t is_in_usb_c_dp4_mode = 0;

	dcn10_link_encoder_get_max_link_cap(enc, link_settings);

	 
	if (!enc->features.flags.bits.DP_IS_USB_C)
		return;

	 
	if (has_query_dp_alt(enc)) {
		if (!query_dp_alt_from_dmub(enc, &cmd))
			return;

		if (cmd.query_dp_alt.data.is_usb &&
		    cmd.query_dp_alt.data.is_dp4 == 0)
			link_settings->lane_count = MIN(LANE_COUNT_TWO, link_settings->lane_count);

		return;
	}

	 
	if (enc->ctx->asic_id.hw_internal_rev != YELLOW_CARP_B0) {
		REG_GET(RDPCSTX_PHY_CNTL6, RDPCS_PHY_DPALT_DP4,
			&is_in_usb_c_dp4_mode);
	} else {
		if ((enc10->base.transmitter == TRANSMITTER_UNIPHY_A) ||
		    (enc10->base.transmitter == TRANSMITTER_UNIPHY_B) ||
		    (enc10->base.transmitter == TRANSMITTER_UNIPHY_E)) {
			REG_GET(RDPCSTX_PHY_CNTL6, RDPCS_PHY_DPALT_DP4,
				&is_in_usb_c_dp4_mode);
		} else {
			REG_GET(RDPCSPIPE_PHY_CNTL6, RDPCS_PHY_DPALT_DP4,
				&is_in_usb_c_dp4_mode);
		}
	}

	if (!is_in_usb_c_dp4_mode)
		link_settings->lane_count = MIN(LANE_COUNT_TWO, link_settings->lane_count);
}
