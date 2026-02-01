 

#include <linux/delay.h>

#include "dc_bios_types.h"
#include "dcn20_stream_encoder.h"
#include "reg_helper.h"
#include "hw_shared.h"
#include "link.h"
#include "dpcd_defs.h"

#define DC_LOGGER \
		enc1->base.ctx->logger

#define REG(reg)\
	(enc1->regs->reg)

#undef FN
#define FN(reg_name, field_name) \
	enc1->se_shift->field_name, enc1->se_mask->field_name


#define CTX \
	enc1->base.ctx


static void enc2_update_hdmi_info_packet(
	struct dcn10_stream_encoder *enc1,
	uint32_t packet_index,
	const struct dc_info_packet *info_packet)
{
	uint32_t cont, send, line;

	if (info_packet->valid) {
		enc1_update_generic_info_packet(
			enc1,
			packet_index,
			info_packet);

		 
		cont = 1;
		 
		send = 1;
		 
		line = 2;
	} else {
		cont = 0;
		send = 0;
		line = 0;
	}

	 

	 
	switch (packet_index) {
	case 0:
		REG_UPDATE_2(HDMI_GENERIC_PACKET_CONTROL0,
				HDMI_GENERIC0_CONT, cont,
				HDMI_GENERIC0_SEND, send);
		REG_UPDATE(HDMI_GENERIC_PACKET_CONTROL1,
				HDMI_GENERIC0_LINE, line);
		break;
	case 1:
		REG_UPDATE_2(HDMI_GENERIC_PACKET_CONTROL0,
				HDMI_GENERIC1_CONT, cont,
				HDMI_GENERIC1_SEND, send);
		REG_UPDATE(HDMI_GENERIC_PACKET_CONTROL1,
				HDMI_GENERIC1_LINE, line);
		break;
	case 2:
		REG_UPDATE_2(HDMI_GENERIC_PACKET_CONTROL0,
				HDMI_GENERIC2_CONT, cont,
				HDMI_GENERIC2_SEND, send);
		REG_UPDATE(HDMI_GENERIC_PACKET_CONTROL2,
				HDMI_GENERIC2_LINE, line);
		break;
	case 3:
		REG_UPDATE_2(HDMI_GENERIC_PACKET_CONTROL0,
				HDMI_GENERIC3_CONT, cont,
				HDMI_GENERIC3_SEND, send);
		REG_UPDATE(HDMI_GENERIC_PACKET_CONTROL2,
				HDMI_GENERIC3_LINE, line);
		break;
	case 4:
		REG_UPDATE_2(HDMI_GENERIC_PACKET_CONTROL0,
				HDMI_GENERIC4_CONT, cont,
				HDMI_GENERIC4_SEND, send);
		REG_UPDATE(HDMI_GENERIC_PACKET_CONTROL3,
				HDMI_GENERIC4_LINE, line);
		break;
	case 5:
		REG_UPDATE_2(HDMI_GENERIC_PACKET_CONTROL0,
				HDMI_GENERIC5_CONT, cont,
				HDMI_GENERIC5_SEND, send);
		REG_UPDATE(HDMI_GENERIC_PACKET_CONTROL3,
				HDMI_GENERIC5_LINE, line);
		break;
	case 6:
		REG_UPDATE_2(HDMI_GENERIC_PACKET_CONTROL0,
				HDMI_GENERIC6_CONT, cont,
				HDMI_GENERIC6_SEND, send);
		REG_UPDATE(HDMI_GENERIC_PACKET_CONTROL4,
				HDMI_GENERIC6_LINE, line);
		break;
	case 7:
		REG_UPDATE_2(HDMI_GENERIC_PACKET_CONTROL0,
				HDMI_GENERIC7_CONT, cont,
				HDMI_GENERIC7_SEND, send);
		REG_UPDATE(HDMI_GENERIC_PACKET_CONTROL4,
				HDMI_GENERIC7_LINE, line);
		break;
	default:
		 
		DC_LOG_WARNING(
			"Invalid HW packet index: %s()\n",
			__func__);
		return;
	}
}

static void enc2_stream_encoder_update_hdmi_info_packets(
	struct stream_encoder *enc,
	const struct encoder_info_frame *info_frame)
{
	struct dcn10_stream_encoder *enc1 = DCN10STRENC_FROM_STRENC(enc);

	 
	REG_UPDATE(HDMI_DB_CONTROL, HDMI_DB_DISABLE, 1);

	 
	enc2_update_hdmi_info_packet(enc1, 0, &info_frame->avi);
	enc2_update_hdmi_info_packet(enc1, 1, &info_frame->hfvsif);
	enc2_update_hdmi_info_packet(enc1, 2, &info_frame->gamut);
	enc2_update_hdmi_info_packet(enc1, 3, &info_frame->vendor);
	enc2_update_hdmi_info_packet(enc1, 4, &info_frame->spd);
	enc2_update_hdmi_info_packet(enc1, 5, &info_frame->hdrsmd);
	enc2_update_hdmi_info_packet(enc1, 6, &info_frame->vtem);
}

static void enc2_stream_encoder_stop_hdmi_info_packets(
	struct stream_encoder *enc)
{
	struct dcn10_stream_encoder *enc1 = DCN10STRENC_FROM_STRENC(enc);

	 
	REG_SET_4(HDMI_GENERIC_PACKET_CONTROL0, 0,
		HDMI_GENERIC0_CONT, 0,
		HDMI_GENERIC0_SEND, 0,
		HDMI_GENERIC1_CONT, 0,
		HDMI_GENERIC1_SEND, 0);
	REG_SET_2(HDMI_GENERIC_PACKET_CONTROL1, 0,
		HDMI_GENERIC0_LINE, 0,
		HDMI_GENERIC1_LINE, 0);

	 
	REG_SET_4(HDMI_GENERIC_PACKET_CONTROL0, 0,
		HDMI_GENERIC2_CONT, 0,
		HDMI_GENERIC2_SEND, 0,
		HDMI_GENERIC3_CONT, 0,
		HDMI_GENERIC3_SEND, 0);
	REG_SET_2(HDMI_GENERIC_PACKET_CONTROL2, 0,
		HDMI_GENERIC2_LINE, 0,
		HDMI_GENERIC3_LINE, 0);

	 
	REG_SET_4(HDMI_GENERIC_PACKET_CONTROL0, 0,
		HDMI_GENERIC4_CONT, 0,
		HDMI_GENERIC4_SEND, 0,
		HDMI_GENERIC5_CONT, 0,
		HDMI_GENERIC5_SEND, 0);
	REG_SET_2(HDMI_GENERIC_PACKET_CONTROL3, 0,
		HDMI_GENERIC4_LINE, 0,
		HDMI_GENERIC5_LINE, 0);

	 
	REG_SET_4(HDMI_GENERIC_PACKET_CONTROL0, 0,
		HDMI_GENERIC6_CONT, 0,
		HDMI_GENERIC6_SEND, 0,
		HDMI_GENERIC7_CONT, 0,
		HDMI_GENERIC7_SEND, 0);
	REG_SET_2(HDMI_GENERIC_PACKET_CONTROL4, 0,
		HDMI_GENERIC6_LINE, 0,
		HDMI_GENERIC7_LINE, 0);
}


 
static void enc2_update_gsp7_128_info_packet(
	struct dcn10_stream_encoder *enc1,
	const struct dc_info_packet_128 *info_packet,
	bool immediate_update)
{
	uint32_t i;

	 
	uint32_t max_retries = 50;
	const uint32_t *content = (const uint32_t *) &info_packet->sb[0];

	ASSERT(info_packet->hb1  == DC_DP_INFOFRAME_TYPE_PPS);

	 
	REG_UPDATE(DP_SEC_CNTL2, DP_SEC_GSP7_PPS, 1);

	 
	REG_UPDATE(AFMT_CNTL, AFMT_AUDIO_CLOCK_EN, 1);

	 
	 

	 
	REG_WAIT(AFMT_VBI_PACKET_CONTROL, AFMT_GENERIC_CONFLICT,
			0, 10, max_retries);

	 
	REG_UPDATE(AFMT_VBI_PACKET_CONTROL, AFMT_GENERIC_CONFLICT_CLR, 1);

	 
	REG_UPDATE(AFMT_VBI_PACKET_CONTROL, AFMT_GENERIC_INDEX, 7);
	REG_SET_4(AFMT_GENERIC_HDR, 0,
			AFMT_GENERIC_HB0, info_packet->hb0,
			AFMT_GENERIC_HB1, info_packet->hb1,
			AFMT_GENERIC_HB2, info_packet->hb2,
			AFMT_GENERIC_HB3, info_packet->hb3);

	 
	for (i = 0; i < 4; i++) {
		uint32_t packet_index = 7 + i;
		REG_UPDATE(AFMT_VBI_PACKET_CONTROL, AFMT_GENERIC_INDEX, packet_index);

		REG_WRITE(AFMT_GENERIC_0, *content++);
		REG_WRITE(AFMT_GENERIC_1, *content++);
		REG_WRITE(AFMT_GENERIC_2, *content++);
		REG_WRITE(AFMT_GENERIC_3, *content++);
		REG_WRITE(AFMT_GENERIC_4, *content++);
		REG_WRITE(AFMT_GENERIC_5, *content++);
		REG_WRITE(AFMT_GENERIC_6, *content++);
		REG_WRITE(AFMT_GENERIC_7, *content++);
	}

	REG_UPDATE_2(AFMT_VBI_PACKET_CONTROL1,
			AFMT_GENERIC7_FRAME_UPDATE, !immediate_update,
			AFMT_GENERIC7_IMMEDIATE_UPDATE, immediate_update);
}

 
static void enc2_dp_set_dsc_config(struct stream_encoder *enc,
					enum optc_dsc_mode dsc_mode,
					uint32_t dsc_bytes_per_pixel,
					uint32_t dsc_slice_width)
{
	struct dcn10_stream_encoder *enc1 = DCN10STRENC_FROM_STRENC(enc);

	REG_UPDATE_2(DP_DSC_CNTL,
			DP_DSC_MODE, dsc_mode,
			DP_DSC_SLICE_WIDTH, dsc_slice_width);

	REG_SET(DP_DSC_BYTES_PER_PIXEL, 0,
		DP_DSC_BYTES_PER_PIXEL, dsc_bytes_per_pixel);
}


static void enc2_dp_set_dsc_pps_info_packet(struct stream_encoder *enc,
					bool enable,
					uint8_t *dsc_packed_pps,
					bool immediate_update)
{
	struct dcn10_stream_encoder *enc1 = DCN10STRENC_FROM_STRENC(enc);

	if (enable) {
		struct dc_info_packet_128 pps_sdp;

		ASSERT(dsc_packed_pps);

		 
		pps_sdp.valid = true;
		pps_sdp.hb0 = 0;
		pps_sdp.hb1 = DC_DP_INFOFRAME_TYPE_PPS;
		pps_sdp.hb2 = 127;
		pps_sdp.hb3 = 0;
		memcpy(&pps_sdp.sb[0], dsc_packed_pps, sizeof(pps_sdp.sb));
		enc2_update_gsp7_128_info_packet(enc1, &pps_sdp, immediate_update);

		 
		
		

		 
		REG_UPDATE(DP_SEC_CNTL6,
				DP_SEC_GSP7_LINE_NUM, 2);
		REG_UPDATE_2(DP_MSA_VBID_MISC,
				DP_VBID6_LINE_REFERENCE, 0,
				DP_VBID6_LINE_NUM, 3);

		 
		REG_UPDATE_2(DP_SEC_CNTL,
			DP_SEC_GSP7_ENABLE, 1,
			DP_SEC_STREAM_ENABLE, 1);
	} else {
		 
		REG_UPDATE(DP_SEC_CNTL, DP_SEC_GSP7_ENABLE, 0);
		REG_UPDATE(DP_SEC_CNTL2, DP_SEC_GSP7_PPS, 0);
	}
}


 
static void enc2_read_state(struct stream_encoder *enc, struct enc_state *s)
{
	struct dcn10_stream_encoder *enc1 = DCN10STRENC_FROM_STRENC(enc);

	
	REG_GET(DP_DSC_CNTL, DP_DSC_MODE, &s->dsc_mode);
	if (s->dsc_mode) {
		REG_GET(DP_DSC_CNTL, DP_DSC_SLICE_WIDTH, &s->dsc_slice_width);
		REG_GET(DP_SEC_CNTL6, DP_SEC_GSP7_LINE_NUM, &s->sec_gsp_pps_line_num);

		REG_GET(DP_MSA_VBID_MISC, DP_VBID6_LINE_REFERENCE, &s->vbid6_line_reference);
		REG_GET(DP_MSA_VBID_MISC, DP_VBID6_LINE_NUM, &s->vbid6_line_num);

		REG_GET(DP_SEC_CNTL, DP_SEC_GSP7_ENABLE, &s->sec_gsp_pps_enable);
		REG_GET(DP_SEC_CNTL, DP_SEC_STREAM_ENABLE, &s->sec_stream_enable);
	}
}

 
void enc2_set_dynamic_metadata(struct stream_encoder *enc,
		bool enable_dme,
		uint32_t hubp_requestor_id,
		enum dynamic_metadata_mode dmdata_mode)
{
	struct dcn10_stream_encoder *enc1 = DCN10STRENC_FROM_STRENC(enc);

	if (enable_dme) {
		REG_UPDATE_2(DME_CONTROL,
				METADATA_HUBP_REQUESTOR_ID, hubp_requestor_id,
				METADATA_STREAM_TYPE, (dmdata_mode == dmdata_dolby_vision) ? 1 : 0);

		 
		if (dmdata_mode == dmdata_dp)
			REG_UPDATE_3(DP_SEC_METADATA_TRANSMISSION,
					DP_SEC_METADATA_PACKET_ENABLE, 1,
					DP_SEC_METADATA_PACKET_LINE_REFERENCE, 0,
					DP_SEC_METADATA_PACKET_LINE, 20);
		else {
			REG_UPDATE_3(HDMI_METADATA_PACKET_CONTROL,
					HDMI_METADATA_PACKET_ENABLE, 1,
					HDMI_METADATA_PACKET_LINE_REFERENCE, 0,
					HDMI_METADATA_PACKET_LINE, 2);

			if (dmdata_mode == dmdata_dolby_vision)
				REG_UPDATE(DIG_FE_CNTL,
						DOLBY_VISION_EN, 1);
		}

		REG_UPDATE(DME_CONTROL,
				METADATA_ENGINE_EN, 1);
	} else {
		REG_UPDATE(DME_CONTROL,
				METADATA_ENGINE_EN, 0);

		if (dmdata_mode == dmdata_dp)
			REG_UPDATE(DP_SEC_METADATA_TRANSMISSION,
					DP_SEC_METADATA_PACKET_ENABLE, 0);
		else {
			REG_UPDATE(HDMI_METADATA_PACKET_CONTROL,
					HDMI_METADATA_PACKET_ENABLE, 0);
			REG_UPDATE(DIG_FE_CNTL,
					DOLBY_VISION_EN, 0);
		}
	}
}

static void enc2_stream_encoder_update_dp_info_packets_sdp_line_num(
		struct stream_encoder *enc,
		struct encoder_info_frame *info_frame)
{
	struct dcn10_stream_encoder *enc1 = DCN10STRENC_FROM_STRENC(enc);

	if (info_frame->adaptive_sync.valid == true &&
		info_frame->sdp_line_num.adaptive_sync_line_num_valid == true) {
		
		REG_UPDATE(DP_SEC_CNTL1, DP_SEC_GSP5_LINE_REFERENCE, 1);

		REG_UPDATE(DP_SEC_CNTL5, DP_SEC_GSP5_LINE_NUM,
					info_frame->sdp_line_num.adaptive_sync_line_num);
	}
}

static void enc2_stream_encoder_update_dp_info_packets(
	struct stream_encoder *enc,
	const struct encoder_info_frame *info_frame)
{
	struct dcn10_stream_encoder *enc1 = DCN10STRENC_FROM_STRENC(enc);
	uint32_t dmdata_packet_enabled = 0;

	enc1_stream_encoder_update_dp_info_packets(enc, info_frame);

	 
	REG_GET(DP_SEC_METADATA_TRANSMISSION,
			DP_SEC_METADATA_PACKET_ENABLE, &dmdata_packet_enabled);

	if (dmdata_packet_enabled)
		REG_UPDATE(DP_SEC_CNTL, DP_SEC_STREAM_ENABLE, 1);
}

static bool is_two_pixels_per_containter(const struct dc_crtc_timing *timing)
{
	bool two_pix = timing->pixel_encoding == PIXEL_ENCODING_YCBCR420;

	two_pix = two_pix || (timing->flags.DSC && timing->pixel_encoding == PIXEL_ENCODING_YCBCR422
			&& !timing->dsc_cfg.ycbcr422_simple);
	return two_pix;
}

void enc2_stream_encoder_dp_unblank(
		struct dc_link *link,
		struct stream_encoder *enc,
		const struct encoder_unblank_param *param)
{
	struct dcn10_stream_encoder *enc1 = DCN10STRENC_FROM_STRENC(enc);

	if (param->link_settings.link_rate != LINK_RATE_UNKNOWN) {
		uint32_t n_vid = 0x8000;
		uint32_t m_vid;
		uint32_t n_multiply = 0;
		uint64_t m_vid_l = n_vid;

		 
		if (is_two_pixels_per_containter(&param->timing) || param->opp_cnt > 1) {
			 
			n_multiply = 1;
		}
		 

		m_vid_l *= param->timing.pix_clk_100hz / 10;
		m_vid_l = div_u64(m_vid_l,
			param->link_settings.link_rate
				* LINK_RATE_REF_FREQ_IN_KHZ);

		m_vid = (uint32_t) m_vid_l;

		 

		REG_UPDATE(DP_VID_TIMING, DP_VID_M_N_GEN_EN, 0);

		 

		REG_UPDATE(DP_VID_N, DP_VID_N, n_vid);

		REG_UPDATE(DP_VID_M, DP_VID_M, m_vid);

		REG_UPDATE_2(DP_VID_TIMING,
				DP_VID_M_N_GEN_EN, 1,
				DP_VID_N_MUL, n_multiply);
	}

	 
	REG_UPDATE(DP_VID_STREAM_CNTL, DP_VID_STREAM_ENABLE, false);
	REG_WAIT(DP_VID_STREAM_CNTL, DP_VID_STREAM_STATUS, 0, 10, 5000);

	 
	REG_UPDATE(DIG_FE_CNTL, DIG_START, 1);
	udelay(1);

	 

	REG_UPDATE(DIG_FE_CNTL, DIG_START, 0);

	 
	REG_UPDATE(DP_STEER_FIFO, DP_STEER_FIFO_RESET, 1);
	udelay(10);

	REG_UPDATE(DP_STEER_FIFO, DP_STEER_FIFO_RESET, 0);

	 
	udelay(100);

	 

	REG_UPDATE(DP_VID_STREAM_CNTL, DP_VID_STREAM_ENABLE, true);

	link->dc->link_srv->dp_trace_source_sequence(link,
			DPCD_SOURCE_SEQ_AFTER_ENABLE_DP_VID_STREAM);
}

static void enc2_dp_set_odm_combine(
	struct stream_encoder *enc,
	bool odm_combine)
{
	struct dcn10_stream_encoder *enc1 = DCN10STRENC_FROM_STRENC(enc);

	REG_UPDATE(DP_PIXEL_FORMAT, DP_PIXEL_COMBINE, odm_combine);
}

void enc2_stream_encoder_dp_set_stream_attribute(
	struct stream_encoder *enc,
	struct dc_crtc_timing *crtc_timing,
	enum dc_color_space output_color_space,
	bool use_vsc_sdp_for_colorimetry,
	uint32_t enable_sdp_splitting)
{
	struct dcn10_stream_encoder *enc1 = DCN10STRENC_FROM_STRENC(enc);

	enc1_stream_encoder_dp_set_stream_attribute(enc,
			crtc_timing,
			output_color_space,
			use_vsc_sdp_for_colorimetry,
			enable_sdp_splitting);

	REG_UPDATE(DP_SEC_FRAMING4,
		DP_SST_SDP_SPLITTING, enable_sdp_splitting);
}

uint32_t enc2_get_fifo_cal_average_level(
		struct stream_encoder *enc)
{
	struct dcn10_stream_encoder *enc1 = DCN10STRENC_FROM_STRENC(enc);
	uint32_t fifo_level;

	REG_GET(DIG_FIFO_STATUS,
			DIG_FIFO_CAL_AVERAGE_LEVEL, &fifo_level);
	return fifo_level;
}

static const struct stream_encoder_funcs dcn20_str_enc_funcs = {
	.dp_set_odm_combine =
		enc2_dp_set_odm_combine,
	.dp_set_stream_attribute =
		enc2_stream_encoder_dp_set_stream_attribute,
	.hdmi_set_stream_attribute =
		enc1_stream_encoder_hdmi_set_stream_attribute,
	.dvi_set_stream_attribute =
		enc1_stream_encoder_dvi_set_stream_attribute,
	.set_throttled_vcp_size =
		enc1_stream_encoder_set_throttled_vcp_size,
	.update_hdmi_info_packets =
		enc2_stream_encoder_update_hdmi_info_packets,
	.stop_hdmi_info_packets =
		enc2_stream_encoder_stop_hdmi_info_packets,
	.update_dp_info_packets_sdp_line_num =
		enc2_stream_encoder_update_dp_info_packets_sdp_line_num,
	.update_dp_info_packets =
		enc2_stream_encoder_update_dp_info_packets,
	.send_immediate_sdp_message =
		enc1_stream_encoder_send_immediate_sdp_message,
	.stop_dp_info_packets =
		enc1_stream_encoder_stop_dp_info_packets,
	.dp_blank =
		enc1_stream_encoder_dp_blank,
	.dp_unblank =
		enc2_stream_encoder_dp_unblank,
	.audio_mute_control = enc1_se_audio_mute_control,

	.dp_audio_setup = enc1_se_dp_audio_setup,
	.dp_audio_enable = enc1_se_dp_audio_enable,
	.dp_audio_disable = enc1_se_dp_audio_disable,

	.hdmi_audio_setup = enc1_se_hdmi_audio_setup,
	.hdmi_audio_disable = enc1_se_hdmi_audio_disable,
	.setup_stereo_sync  = enc1_setup_stereo_sync,
	.set_avmute = enc1_stream_encoder_set_avmute,
	.dig_connect_to_otg  = enc1_dig_connect_to_otg,
	.dig_source_otg = enc1_dig_source_otg,

	.dp_get_pixel_format =
		enc1_stream_encoder_dp_get_pixel_format,

	.enc_read_state = enc2_read_state,
	.dp_set_dsc_config = enc2_dp_set_dsc_config,
	.dp_set_dsc_pps_info_packet = enc2_dp_set_dsc_pps_info_packet,
	.set_dynamic_metadata = enc2_set_dynamic_metadata,
	.hdmi_reset_stream_attribute = enc1_reset_hdmi_stream_attribute,
	.get_fifo_cal_average_level = enc2_get_fifo_cal_average_level,
};

void dcn20_stream_encoder_construct(
	struct dcn10_stream_encoder *enc1,
	struct dc_context *ctx,
	struct dc_bios *bp,
	enum engine_id eng_id,
	const struct dcn10_stream_enc_registers *regs,
	const struct dcn10_stream_encoder_shift *se_shift,
	const struct dcn10_stream_encoder_mask *se_mask)
{
	enc1->base.funcs = &dcn20_str_enc_funcs;
	enc1->base.ctx = ctx;
	enc1->base.id = eng_id;
	enc1->base.bp = bp;
	enc1->regs = regs;
	enc1->se_shift = se_shift;
	enc1->se_mask = se_mask;
	enc1->base.stream_enc_inst = eng_id - ENGINE_ID_DIGA;
}

