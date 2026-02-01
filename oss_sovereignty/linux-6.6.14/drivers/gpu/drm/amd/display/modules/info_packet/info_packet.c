 

#include "mod_info_packet.h"
#include "core_types.h"
#include "dc_types.h"
#include "mod_shared.h"
#include "mod_freesync.h"
#include "dc.h"

enum vsc_packet_revision {
	vsc_packet_undefined = 0,
	
	vsc_packet_rev1 = 1,
	
	vsc_packet_rev2 = 2,
	
	vsc_packet_rev3 = 3,
	
	vsc_packet_rev4 = 4,
	
	vsc_packet_rev5 = 5,
};

#define HDMI_INFOFRAME_TYPE_VENDOR 0x81
#define HF_VSIF_VERSION 1


#define VTEM_PB0		0
#define VTEM_PB1		1
#define VTEM_PB2		2
#define VTEM_PB3		3
#define VTEM_PB4		4
#define VTEM_PB5		5
#define VTEM_PB6		6

#define VTEM_MD0		7
#define VTEM_MD1		8
#define VTEM_MD2		9
#define VTEM_MD3		10




#define MASK_VTEM_PB0__RESERVED0  0x01
#define MASK_VTEM_PB0__SYNC       0x02
#define MASK_VTEM_PB0__VFR        0x04
#define MASK_VTEM_PB0__AFR        0x08
#define MASK_VTEM_PB0__DS_TYPE    0x30
	
	
	
	
#define MASK_VTEM_PB0__END        0x40
#define MASK_VTEM_PB0__NEW        0x80


#define MASK_VTEM_PB1__RESERVED1 0xFF


#define MASK_VTEM_PB2__ORGANIZATION_ID 0xFF
	
	
	
	

#define MASK_VTEM_PB3__DATA_SET_TAG_MSB    0xFF

#define MASK_VTEM_PB4__DATA_SET_TAG_LSB    0xFF

#define MASK_VTEM_PB5__DATA_SET_LENGTH_MSB 0xFF

#define MASK_VTEM_PB6__DATA_SET_LENGTH_LSB 0xFF





#define MASK_VTEM_MD0__VRR_EN         0x01
#define MASK_VTEM_MD0__M_CONST        0x02
#define MASK_VTEM_MD0__QMS_EN         0x04
#define MASK_VTEM_MD0__RESERVED2      0x08
#define MASK_VTEM_MD0__FVA_FACTOR_M1  0xF0


#define MASK_VTEM_MD1__BASE_VFRONT    0xFF


#define MASK_VTEM_MD2__BASE_REFRESH_RATE_98  0x03
#define MASK_VTEM_MD2__RB                    0x04
#define MASK_VTEM_MD2__NEXT_TFR              0xF8


#define MASK_VTEM_MD3__BASE_REFRESH_RATE_07  0xFF

enum ColorimetryRGBDP {
	ColorimetryRGB_DP_sRGB               = 0,
	ColorimetryRGB_DP_AdobeRGB           = 3,
	ColorimetryRGB_DP_P3                 = 4,
	ColorimetryRGB_DP_CustomColorProfile = 5,
	ColorimetryRGB_DP_ITU_R_BT2020RGB    = 6,
};
enum ColorimetryYCCDP {
	ColorimetryYCC_DP_ITU601        = 0,
	ColorimetryYCC_DP_ITU709        = 1,
	ColorimetryYCC_DP_AdobeYCC      = 5,
	ColorimetryYCC_DP_ITU2020YCC    = 6,
	ColorimetryYCC_DP_ITU2020YCbCr  = 7,
};

void mod_build_vsc_infopacket(const struct dc_stream_state *stream,
		struct dc_info_packet *info_packet,
		enum dc_color_space cs,
		enum color_transfer_func tf)
{
	unsigned int vsc_packet_revision = vsc_packet_undefined;
	unsigned int i;
	unsigned int pixelEncoding = 0;
	unsigned int colorimetryFormat = 0;
	bool stereo3dSupport = false;

	if (stream->timing.timing_3d_format != TIMING_3D_FORMAT_NONE && stream->view_format != VIEW_3D_FORMAT_NONE) {
		vsc_packet_revision = vsc_packet_rev1;
		stereo3dSupport = true;
	}

	 
	if (stream->link->psr_settings.psr_feature_enabled) {
		if (stream->link->psr_settings.psr_version == DC_PSR_VERSION_SU_1)
			vsc_packet_revision = vsc_packet_rev4;
		else if (stream->link->psr_settings.psr_version == DC_PSR_VERSION_1)
			vsc_packet_revision = vsc_packet_rev2;
	}

	if (stream->link->replay_settings.config.replay_supported)
		vsc_packet_revision = vsc_packet_rev4;

	 
	if (stream->use_vsc_sdp_for_colorimetry)
		vsc_packet_revision = vsc_packet_rev5;

	 
	if (vsc_packet_revision == vsc_packet_undefined)
		return;

	if (vsc_packet_revision == vsc_packet_rev4) {
		 
		info_packet->hb0 = 0x00;
		 
		info_packet->hb1 = 0x07;
		 
		info_packet->hb2 = 0x04;
		 
		info_packet->hb3 = 0x0E;

		for (i = 0; i < 28; i++)
			info_packet->sb[i] = 0;

		info_packet->valid = true;
	}

	if (vsc_packet_revision == vsc_packet_rev2) {
		 
		info_packet->hb0 = 0x00;
		 
		info_packet->hb1 = 0x07;
		 
		info_packet->hb2 = 0x02;
		 
		info_packet->hb3 = 0x08;

		for (i = 0; i < 28; i++)
			info_packet->sb[i] = 0;

		info_packet->valid = true;
	}

	if (vsc_packet_revision == vsc_packet_rev1) {

		info_packet->hb0 = 0x00;	
		info_packet->hb1 = 0x07;	
		info_packet->hb2 = 0x01;	
		info_packet->hb3 = 0x01;	

		info_packet->valid = true;
	}

	if (stereo3dSupport) {
		 
		switch (stream->timing.timing_3d_format) {
		case TIMING_3D_FORMAT_HW_FRAME_PACKING:
		case TIMING_3D_FORMAT_SW_FRAME_PACKING:
		case TIMING_3D_FORMAT_TOP_AND_BOTTOM:
		case TIMING_3D_FORMAT_TB_SW_PACKED:
			info_packet->sb[0] = 0x02; 
			break;
		case TIMING_3D_FORMAT_DP_HDMI_INBAND_FA:
		case TIMING_3D_FORMAT_INBAND_FA:
			info_packet->sb[0] = 0x01; 
			break;
		case TIMING_3D_FORMAT_SIDE_BY_SIDE:
		case TIMING_3D_FORMAT_SBS_SW_PACKED:
			info_packet->sb[0] = 0x04; 
			break;
		default:
			info_packet->sb[0] = 0x00; 
			break;
		}

	}

	 
	if (vsc_packet_revision == vsc_packet_rev5) {
		 
		info_packet->hb0 = 0x00;
		 
		info_packet->hb1 = 0x07;
		 
		info_packet->hb2 = 0x05;
		 
		info_packet->hb3 = 0x13;

		info_packet->valid = true;

		 

		 
		switch (stream->timing.pixel_encoding) {
		case PIXEL_ENCODING_RGB:
			pixelEncoding = 0x0;   
			break;
		case PIXEL_ENCODING_YCBCR444:
			pixelEncoding = 0x1;   
			break;
		case PIXEL_ENCODING_YCBCR422:
			pixelEncoding = 0x2;   
			break;
		case PIXEL_ENCODING_YCBCR420:
			pixelEncoding = 0x3;   
			break;
		default:
			pixelEncoding = 0x0;   
			break;
		}

		 
		switch (stream->timing.pixel_encoding) {
		case PIXEL_ENCODING_RGB:
			if ((cs == COLOR_SPACE_SRGB) ||
					(cs == COLOR_SPACE_SRGB_LIMITED))
				colorimetryFormat = ColorimetryRGB_DP_sRGB;
			else if (cs == COLOR_SPACE_ADOBERGB)
				colorimetryFormat = ColorimetryRGB_DP_AdobeRGB;
			else if ((cs == COLOR_SPACE_2020_RGB_FULLRANGE) ||
					(cs == COLOR_SPACE_2020_RGB_LIMITEDRANGE))
				colorimetryFormat = ColorimetryRGB_DP_ITU_R_BT2020RGB;
			break;

		case PIXEL_ENCODING_YCBCR444:
		case PIXEL_ENCODING_YCBCR422:
		case PIXEL_ENCODING_YCBCR420:
			 
			if (cs == COLOR_SPACE_YCBCR601)
				colorimetryFormat = ColorimetryYCC_DP_ITU601;
			else if (cs == COLOR_SPACE_YCBCR709)
				colorimetryFormat = ColorimetryYCC_DP_ITU709;
			else if (cs == COLOR_SPACE_ADOBERGB)
				colorimetryFormat = ColorimetryYCC_DP_AdobeYCC;
			else if (cs == COLOR_SPACE_2020_YCBCR)
				colorimetryFormat = ColorimetryYCC_DP_ITU2020YCbCr;

			if (cs == COLOR_SPACE_2020_YCBCR && tf == TRANSFER_FUNC_GAMMA_22)
				colorimetryFormat = ColorimetryYCC_DP_ITU709;
			break;

		default:
			colorimetryFormat = ColorimetryRGB_DP_sRGB;
			break;
		}

		info_packet->sb[16] = (pixelEncoding << 4) | colorimetryFormat;

		 
		switch (stream->timing.display_color_depth) {
		case COLOR_DEPTH_666:
			 
			info_packet->sb[17] = 0;
			break;
		case COLOR_DEPTH_888:
			info_packet->sb[17] = 1;
			break;
		case COLOR_DEPTH_101010:
			info_packet->sb[17] = 2;
			break;
		case COLOR_DEPTH_121212:
			info_packet->sb[17] = 3;
			break;
		 
		case COLOR_DEPTH_161616:
			info_packet->sb[17] = 4;
			break;
		default:
			info_packet->sb[17] = 0;
			break;
		}

		 
		if ((cs == COLOR_SPACE_SRGB_LIMITED) ||
				(cs == COLOR_SPACE_2020_RGB_LIMITEDRANGE) ||
				(pixelEncoding != 0x0)) {
			info_packet->sb[17] |= 0x80;  
		}

		 
		info_packet->sb[18] = 0;
	}
}

 
void mod_build_hf_vsif_infopacket(const struct dc_stream_state *stream,
		struct dc_info_packet *info_packet)
{
		unsigned int length = 5;
		bool hdmi_vic_mode = false;
		uint8_t checksum = 0;
		uint32_t i = 0;
		enum dc_timing_3d_format format;

		info_packet->valid = false;
		format = stream->timing.timing_3d_format;
		if (stream->view_format == VIEW_3D_FORMAT_NONE)
			format = TIMING_3D_FORMAT_NONE;

		if (stream->timing.hdmi_vic != 0
				&& stream->timing.h_total >= 3840
				&& stream->timing.v_total >= 2160
				&& format == TIMING_3D_FORMAT_NONE)
			hdmi_vic_mode = true;

		if ((format == TIMING_3D_FORMAT_NONE) && !hdmi_vic_mode)
			return;

		info_packet->sb[1] = 0x03;
		info_packet->sb[2] = 0x0C;
		info_packet->sb[3] = 0x00;

		if (format != TIMING_3D_FORMAT_NONE)
			info_packet->sb[4] = (2 << 5);

		else if (hdmi_vic_mode)
			info_packet->sb[4] = (1 << 5);

		switch (format) {
		case TIMING_3D_FORMAT_HW_FRAME_PACKING:
		case TIMING_3D_FORMAT_SW_FRAME_PACKING:
			info_packet->sb[5] = (0x0 << 4);
			break;

		case TIMING_3D_FORMAT_SIDE_BY_SIDE:
		case TIMING_3D_FORMAT_SBS_SW_PACKED:
			info_packet->sb[5] = (0x8 << 4);
			length = 6;
			break;

		case TIMING_3D_FORMAT_TOP_AND_BOTTOM:
		case TIMING_3D_FORMAT_TB_SW_PACKED:
			info_packet->sb[5] = (0x6 << 4);
			break;

		default:
			break;
		}

		if (hdmi_vic_mode)
			info_packet->sb[5] = stream->timing.hdmi_vic;

		info_packet->hb0 = HDMI_INFOFRAME_TYPE_VENDOR;
		info_packet->hb1 = 0x01;
		info_packet->hb2 = (uint8_t) (length);

		checksum += info_packet->hb0;
		checksum += info_packet->hb1;
		checksum += info_packet->hb2;

		for (i = 1; i <= length; i++)
			checksum += info_packet->sb[i];

		info_packet->sb[0] = (uint8_t) (0x100 - checksum);

		info_packet->valid = true;
}

void mod_build_adaptive_sync_infopacket(const struct dc_stream_state *stream,
		enum adaptive_sync_type asType,
		const struct AS_Df_params *param,
		struct dc_info_packet *info_packet)
{
	info_packet->valid = false;

	memset(info_packet, 0, sizeof(struct dc_info_packet));

	switch (asType) {
	case ADAPTIVE_SYNC_TYPE_DP:
		if (stream != NULL)
			mod_build_adaptive_sync_infopacket_v2(stream, param, info_packet);
		break;
	case FREESYNC_TYPE_PCON_IN_WHITELIST:
		mod_build_adaptive_sync_infopacket_v1(info_packet);
		break;
	case ADAPTIVE_SYNC_TYPE_EDP:
		mod_build_adaptive_sync_infopacket_v1(info_packet);
		break;
	case ADAPTIVE_SYNC_TYPE_NONE:
	case FREESYNC_TYPE_PCON_NOT_IN_WHITELIST:
	default:
		break;
	}
}

void mod_build_adaptive_sync_infopacket_v1(struct dc_info_packet *info_packet)
{
	info_packet->valid = true;
	
	info_packet->hb0 = 0x00;
	info_packet->hb1 = 0x22;
	info_packet->hb2 = AS_SDP_VER_1;
	info_packet->hb3 = 0x00;
}

void mod_build_adaptive_sync_infopacket_v2(const struct dc_stream_state *stream,
		const struct AS_Df_params *param,
		struct dc_info_packet *info_packet)
{
	info_packet->valid = true;
	
	info_packet->hb0 = 0x00;
	info_packet->hb1 = 0x22;
	info_packet->hb2 = AS_SDP_VER_2;
	info_packet->hb3 = AS_DP_SDP_LENGTH;

	
	info_packet->sb[0] = param->supportMode; 
	info_packet->sb[1] = (stream->timing.v_total & 0x00FF);
	info_packet->sb[2] = (stream->timing.v_total & 0xFF00) >> 8;
	
	info_packet->sb[4] = (param->increase.support << 6 | param->decrease.support << 7);
	info_packet->sb[5] = param->increase.frame_duration_hex;
	info_packet->sb[6] = param->decrease.frame_duration_hex;
}

