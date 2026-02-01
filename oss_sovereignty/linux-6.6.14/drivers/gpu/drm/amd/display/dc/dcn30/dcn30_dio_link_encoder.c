 

#include "reg_helper.h"

#include "core_types.h"
#include "link_encoder.h"
#include "dcn30_dio_link_encoder.h"
#include "stream_encoder.h"
#include "dc_bios_types.h"
 

#include "gpio_service_interface.h"

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


bool dcn30_link_encoder_validate_output_with_stream(
	struct link_encoder *enc,
	const struct dc_stream_state *stream)
{
		return dcn10_link_encoder_validate_output_with_stream(enc, stream);
}

static const struct link_encoder_funcs dcn30_link_enc_funcs = {
	.read_state = link_enc2_read_state,
	.validate_output_with_stream =
			dcn30_link_encoder_validate_output_with_stream,
	.hw_init = enc3_hw_init,
	.setup = dcn10_link_encoder_setup,
	.enable_tmds_output = dcn10_link_encoder_enable_tmds_output,
	.enable_dp_output = dcn20_link_encoder_enable_dp_output,
	.enable_dp_mst_output = dcn10_link_encoder_enable_dp_mst_output,
	.disable_output = dcn10_link_encoder_disable_output,
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
	.is_in_alt_mode = dcn20_link_encoder_is_in_alt_mode,
	.get_max_link_cap = dcn20_link_encoder_get_max_link_cap,
};

void dcn30_link_encoder_construct(
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

	enc10->base.funcs = &dcn30_link_enc_funcs;
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
	case TRANSMITTER_UNIPHY_G:
		enc10->base.preferred_engine = ENGINE_ID_DIGG;
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

#define AUX_REG(reg)\
	(enc10->aux_regs->reg)

#define AUX_REG_READ(reg_name) \
		dm_read_reg(CTX, AUX_REG(reg_name))

#define AUX_REG_WRITE(reg_name, val) \
			dm_write_reg(CTX, AUX_REG(reg_name), val)
void enc3_hw_init(struct link_encoder *enc)
{
	struct dcn10_link_encoder *enc10 = TO_DCN10_LINK_ENC(enc);

 

 
	AUX_REG_WRITE(AUX_DPHY_RX_CONTROL0, 0x103d1110);

	AUX_REG_WRITE(AUX_DPHY_TX_CONTROL, 0x21c7a);

	
	
	
	
	

	
	REG_UPDATE(TMDS_CTL_BITS, TMDS_CTL0, 1);

	dcn10_aux_initialize(enc10);
}
