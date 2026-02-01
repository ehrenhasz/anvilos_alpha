 
#ifndef __DAL_DDC_SERVICE_TYPES_H__
#define __DAL_DDC_SERVICE_TYPES_H__

 
#define DP_BRANCH_DEVICE_ID_0010FA 0x0010FA
 
#define DP_BRANCH_DEVICE_ID_0022B9 0x0022B9
#define DP_BRANCH_DEVICE_ID_00001A 0x00001A
#define DP_BRANCH_DEVICE_ID_0080E1 0x0080e1
#define DP_BRANCH_DEVICE_ID_90CC24 0x90CC24
#define DP_BRANCH_DEVICE_ID_00E04C 0x00E04C
#define DP_BRANCH_DEVICE_ID_006037 0x006037
#define DP_BRANCH_DEVICE_ID_001CF8 0x001CF8
#define DP_BRANCH_DEVICE_ID_0060AD 0x0060AD
#define DP_BRANCH_HW_REV_10 0x10
#define DP_BRANCH_HW_REV_20 0x20

#define DP_DEVICE_ID_38EC11 0x38EC11
#define DP_DEVICE_ID_BA4159 0xBA4159
#define DP_FORCE_PSRSU_CAPABILITY 0x40F

#define DP_SINK_PSR_ACTIVE_VTOTAL		0x373
#define DP_SINK_PSR_ACTIVE_VTOTAL_CONTROL_MODE	0x375
#define DP_SOURCE_PSR_ACTIVE_VTOTAL		0x376

enum ddc_result {
	DDC_RESULT_UNKNOWN = 0,
	DDC_RESULT_SUCESSFULL,
	DDC_RESULT_FAILED_CHANNEL_BUSY,
	DDC_RESULT_FAILED_TIMEOUT,
	DDC_RESULT_FAILED_PROTOCOL_ERROR,
	DDC_RESULT_FAILED_NACK,
	DDC_RESULT_FAILED_INCOMPLETE,
	DDC_RESULT_FAILED_OPERATION,
	DDC_RESULT_FAILED_INVALID_OPERATION,
	DDC_RESULT_FAILED_BUFFER_OVERFLOW,
	DDC_RESULT_FAILED_HPD_DISCON
};

enum ddc_service_type {
	DDC_SERVICE_TYPE_CONNECTOR,
	DDC_SERVICE_TYPE_DISPLAY_PORT_MST,
};

 
struct display_sink_capability {
	 
	enum display_dongle_type dongle_type;
	bool is_dongle_type_one;

	 
	 
	uint32_t downstrm_sink_count;
	 
	bool downstrm_sink_count_valid;

	 
	uint32_t additional_audio_delay;
	 
	uint32_t audio_latency;
	 
	uint32_t video_latency_interlace;
	 
	uint32_t video_latency_progressive;
	 
	uint32_t max_hdmi_pixel_clock;
	 
	enum dc_color_depth max_hdmi_deep_color;

	 
	 
	bool ss_supported;
	 
	uint32_t dp_link_lane_count;
	uint32_t dp_link_rate;
	uint32_t dp_link_spead;

	 
	bool is_dp_hdmi_s3d_converter;
	 
	bool is_edp_sink_cap_valid;

	enum ddc_transaction_type transaction_type;
	enum signal_type signal;
};

struct av_sync_data {
	uint8_t av_granularity; 
	uint8_t aud_dec_lat1; 
	uint8_t aud_dec_lat2; 
	uint8_t aud_pp_lat1; 
	uint8_t aud_pp_lat2; 
	uint8_t vid_inter_lat; 
	uint8_t vid_prog_lat; 
	uint8_t aud_del_ins1; 
	uint8_t aud_del_ins2; 
	uint8_t aud_del_ins3; 
};

#endif  
