 
#ifndef DC_TYPES_H_
#define DC_TYPES_H_

 
#include "os_types.h"
#include "fixed31_32.h"
#include "irq_types.h"
#include "dc_ddc_types.h"
#include "dc_dp_types.h"
#include "dc_hdmi_types.h"
#include "dc_hw_types.h"
#include "dal_types.h"
#include "grph_object_defs.h"
#include "grph_object_ctrl_defs.h"

#include "dm_cp_psp.h"

 
struct dc_plane_state;
struct dc_stream_state;
struct dc_link;
struct dc_sink;
struct dal;
struct dc_dmub_srv;

 
enum dce_environment {
	DCE_ENV_PRODUCTION_DRV = 0,
	 
	DCE_ENV_FPGA_MAXIMUS,
	 
	DCE_ENV_DIAG,
	 
	DCE_ENV_VIRTUAL_HW
};

struct dc_perf_trace {
	unsigned long read_count;
	unsigned long write_count;
	unsigned long last_entry_read;
	unsigned long last_entry_write;
};

#define MAX_SURFACE_NUM 6
#define NUM_PIXEL_FORMATS 10

enum tiling_mode {
	TILING_MODE_INVALID,
	TILING_MODE_LINEAR,
	TILING_MODE_TILED,
	TILING_MODE_COUNT
};

enum view_3d_format {
	VIEW_3D_FORMAT_NONE = 0,
	VIEW_3D_FORMAT_FRAME_SEQUENTIAL,
	VIEW_3D_FORMAT_SIDE_BY_SIDE,
	VIEW_3D_FORMAT_TOP_AND_BOTTOM,
	VIEW_3D_FORMAT_COUNT,
	VIEW_3D_FORMAT_FIRST = VIEW_3D_FORMAT_FRAME_SEQUENTIAL
};

enum plane_stereo_format {
	PLANE_STEREO_FORMAT_NONE = 0,
	PLANE_STEREO_FORMAT_SIDE_BY_SIDE = 1,
	PLANE_STEREO_FORMAT_TOP_AND_BOTTOM = 2,
	PLANE_STEREO_FORMAT_FRAME_ALTERNATE = 3,
	PLANE_STEREO_FORMAT_ROW_INTERLEAVED = 5,
	PLANE_STEREO_FORMAT_COLUMN_INTERLEAVED = 6,
	PLANE_STEREO_FORMAT_CHECKER_BOARD = 7
};

 

enum dc_edid_connector_type {
	DC_EDID_CONNECTOR_UNKNOWN = 0,
	DC_EDID_CONNECTOR_ANALOG = 1,
	DC_EDID_CONNECTOR_DIGITAL = 10,
	DC_EDID_CONNECTOR_DVI = 11,
	DC_EDID_CONNECTOR_HDMIA = 12,
	DC_EDID_CONNECTOR_MDDI = 14,
	DC_EDID_CONNECTOR_DISPLAYPORT = 15
};

enum dc_edid_status {
	EDID_OK,
	EDID_BAD_INPUT,
	EDID_NO_RESPONSE,
	EDID_BAD_CHECKSUM,
	EDID_THE_SAME,
	EDID_FALL_BACK,
	EDID_PARTIAL_VALID,
};

enum act_return_status {
	ACT_SUCCESS,
	ACT_LINK_LOST,
	ACT_FAILED
};

 
struct dc_cea_audio_mode {
	uint8_t format_code;  
	uint8_t channel_count;  
	uint8_t sample_rate;  
	union {
		uint8_t sample_size;  
		 
		uint8_t max_bit_rate;
		uint8_t audio_codec_vendor_specific;  
	};
};

struct dc_edid {
	uint32_t length;
	uint8_t raw_edid[DC_MAX_EDID_BUFFER_SIZE];
};

 
#define DEFAULT_SPEAKER_LOCATION 5

#define DC_MAX_AUDIO_DESC_COUNT 16

#define AUDIO_INFO_DISPLAY_NAME_SIZE_IN_CHARS 20

union display_content_support {
	unsigned int raw;
	struct {
		unsigned int valid_content_type :1;
		unsigned int game_content :1;
		unsigned int cinema_content :1;
		unsigned int photo_content :1;
		unsigned int graphics_content :1;
		unsigned int reserved :27;
	} bits;
};

struct dc_panel_patch {
	unsigned int dppowerup_delay;
	unsigned int extra_t12_ms;
	unsigned int extra_delay_backlight_off;
	unsigned int extra_t7_ms;
	unsigned int skip_scdc_overwrite;
	unsigned int delay_ignore_msa;
	unsigned int disable_fec;
	unsigned int extra_t3_ms;
	unsigned int max_dsc_target_bpp_limit;
	unsigned int embedded_tiled_slave;
	unsigned int disable_fams;
	unsigned int skip_avmute;
	unsigned int mst_start_top_delay;
	unsigned int remove_sink_ext_caps;
};

struct dc_edid_caps {
	 
	uint16_t manufacturer_id;
	uint16_t product_id;
	uint32_t serial_number;
	uint8_t manufacture_week;
	uint8_t manufacture_year;
	uint8_t display_name[AUDIO_INFO_DISPLAY_NAME_SIZE_IN_CHARS];

	 
	uint8_t speaker_flags;
	uint32_t audio_mode_count;
	struct dc_cea_audio_mode audio_modes[DC_MAX_AUDIO_DESC_COUNT];
	uint32_t audio_latency;
	uint32_t video_latency;

	union display_content_support content_support;

	uint8_t qs_bit;
	uint8_t qy_bit;

	uint32_t max_tmds_clk_mhz;

	 
	bool lte_340mcsc_scramble;

	bool edid_hdmi;
	bool hdr_supported;

	struct dc_panel_patch panel_patch;
};

struct dc_mode_flags {
	 
	uint32_t INTERLACE :1;
	 
	uint32_t NATIVE :1;
	 
	uint32_t PREFERRED :1;
	 
	uint32_t REDUCED_BLANKING :1;
	 
	uint32_t VIDEO_OPTIMIZED_RATE :1;
	 
	uint32_t PACKED_PIXEL_FORMAT :1;
	 
	uint32_t PREFERRED_VIEW :1;
	 
	uint32_t TILED_MODE :1;
	uint32_t DSE_MODE :1;
	 
	uint32_t MIRACAST_REFRESH_DIVIDER;
};


enum dc_timing_source {
	TIMING_SOURCE_UNDEFINED,

	 
	TIMING_SOURCE_USER_FORCED,
	TIMING_SOURCE_USER_OVERRIDE,
	TIMING_SOURCE_CUSTOM,
	TIMING_SOURCE_EXPLICIT,

	 
	TIMING_SOURCE_EDID_CEA_SVD_3D,
	TIMING_SOURCE_EDID_CEA_SVD_PREFERRED,
	TIMING_SOURCE_EDID_CEA_SVD_420,
	TIMING_SOURCE_EDID_DETAILED,
	TIMING_SOURCE_EDID_ESTABLISHED,
	TIMING_SOURCE_EDID_STANDARD,
	TIMING_SOURCE_EDID_CEA_SVD,
	TIMING_SOURCE_EDID_CVT_3BYTE,
	TIMING_SOURCE_EDID_4BYTE,
	TIMING_SOURCE_EDID_CEA_DISPLAYID_VTDB,
	TIMING_SOURCE_EDID_CEA_RID,
	TIMING_SOURCE_VBIOS,
	TIMING_SOURCE_CV,
	TIMING_SOURCE_TV,
	TIMING_SOURCE_HDMI_VIC,

	 
	TIMING_SOURCE_DEFAULT,

	 
	TIMING_SOURCE_CUSTOM_BASE,

	 
	TIMING_SOURCE_RANGELIMIT,
	TIMING_SOURCE_OS_FORCED,
	TIMING_SOURCE_IMPLICIT,

	 
	TIMING_SOURCE_BASICMODE,

	TIMING_SOURCE_COUNT
};


struct stereo_3d_features {
	bool supported			;
	bool allTimings			;
	bool cloneMode			;
	bool scaling			;
	bool singleFrameSWPacked;
};

enum dc_timing_support_method {
	TIMING_SUPPORT_METHOD_UNDEFINED,
	TIMING_SUPPORT_METHOD_EXPLICIT,
	TIMING_SUPPORT_METHOD_IMPLICIT,
	TIMING_SUPPORT_METHOD_NATIVE
};

struct dc_mode_info {
	uint32_t pixel_width;
	uint32_t pixel_height;
	uint32_t field_rate;
	 

	enum dc_timing_standard timing_standard;
	enum dc_timing_source timing_source;
	struct dc_mode_flags flags;
};

enum dc_power_state {
	DC_POWER_STATE_ON = 1,
	DC_POWER_STATE_STANDBY,
	DC_POWER_STATE_SUSPEND,
	DC_POWER_STATE_OFF
};

 
enum dc_video_power_state {
	DC_VIDEO_POWER_UNSPECIFIED = 0,
	DC_VIDEO_POWER_ON = 1,
	DC_VIDEO_POWER_STANDBY,
	DC_VIDEO_POWER_SUSPEND,
	DC_VIDEO_POWER_OFF,
	DC_VIDEO_POWER_HIBERNATE,
	DC_VIDEO_POWER_SHUTDOWN,
	DC_VIDEO_POWER_ULPS,	 
	DC_VIDEO_POWER_AFTER_RESET,
	DC_VIDEO_POWER_MAXIMUM
};

enum dc_acpi_cm_power_state {
	DC_ACPI_CM_POWER_STATE_D0 = 1,
	DC_ACPI_CM_POWER_STATE_D1 = 2,
	DC_ACPI_CM_POWER_STATE_D2 = 4,
	DC_ACPI_CM_POWER_STATE_D3 = 8
};

enum dc_connection_type {
	dc_connection_none,
	dc_connection_single,
	dc_connection_mst_branch,
	dc_connection_sst_branch
};

struct dc_csc_adjustments {
	struct fixed31_32 contrast;
	struct fixed31_32 saturation;
	struct fixed31_32 brightness;
	struct fixed31_32 hue;
};

 
enum scaling_transformation {
	SCALING_TRANSFORMATION_UNINITIALIZED,
	SCALING_TRANSFORMATION_IDENTITY = 0x0001,
	SCALING_TRANSFORMATION_CENTER_TIMING = 0x0002,
	SCALING_TRANSFORMATION_FULL_SCREEN_SCALE = 0x0004,
	SCALING_TRANSFORMATION_PRESERVE_ASPECT_RATIO_SCALE = 0x0008,
	SCALING_TRANSFORMATION_DAL_DECIDE = 0x0010,
	SCALING_TRANSFORMATION_INVALID = 0x80000000,

	 
	SCALING_TRANSFORMATION_BEGING = SCALING_TRANSFORMATION_IDENTITY,
	SCALING_TRANSFORMATION_END =
		SCALING_TRANSFORMATION_PRESERVE_ASPECT_RATIO_SCALE
};

enum display_content_type {
	DISPLAY_CONTENT_TYPE_NO_DATA = 0,
	DISPLAY_CONTENT_TYPE_GRAPHICS = 1,
	DISPLAY_CONTENT_TYPE_PHOTO = 2,
	DISPLAY_CONTENT_TYPE_CINEMA = 4,
	DISPLAY_CONTENT_TYPE_GAME = 8
};

enum cm_gamut_adjust_type {
	CM_GAMUT_ADJUST_TYPE_BYPASS = 0,
	CM_GAMUT_ADJUST_TYPE_HW,  
	CM_GAMUT_ADJUST_TYPE_SW  
};

struct cm_grph_csc_adjustment {
	struct fixed31_32 temperature_matrix[12];
	enum cm_gamut_adjust_type gamut_adjust_type;
	enum cm_gamut_coef_format gamut_coef_format;
};

 
struct dwb_stereo_params {
	bool				stereo_enabled;		 
	enum dwb_stereo_type		stereo_type;		 
	bool				stereo_polarity;	 
	enum dwb_stereo_eye_select	stereo_eye_select;	 
};

struct dc_dwb_cnv_params {
	unsigned int		src_width;	 
	unsigned int		src_height;	 
	unsigned int		crop_width;	 
	bool			crop_en;	 
	unsigned int		crop_height;	 
	unsigned int		crop_x;		 
	unsigned int		crop_y;		 
	enum dwb_cnv_out_bpc cnv_out_bpc;	 
	enum dwb_out_format	fc_out_format;	 
	enum dwb_out_denorm	out_denorm_mode; 
	unsigned int		out_max_pix_val; 
	unsigned int		out_min_pix_val; 
};

struct dc_dwb_params {
	unsigned int			dwbscl_black_color;  
	unsigned int			hdr_mult;	 
	struct cm_grph_csc_adjustment	csc_params;
	struct dwb_stereo_params	stereo_params;
	struct dc_dwb_cnv_params	cnv_params;	 
	unsigned int			dest_width;	 
	unsigned int			dest_height;	 
	enum dwb_scaler_mode		out_format;	 
	enum dwb_output_depth		output_depth;	 
	enum dwb_capture_rate		capture_rate;	 
	struct scaling_taps 		scaler_taps;	 
	enum dwb_subsample_position	subsample_position;
	struct dc_transfer_func *out_transfer_func;
};

 

union audio_sample_rates {
	struct sample_rates {
		uint8_t RATE_32:1;
		uint8_t RATE_44_1:1;
		uint8_t RATE_48:1;
		uint8_t RATE_88_2:1;
		uint8_t RATE_96:1;
		uint8_t RATE_176_4:1;
		uint8_t RATE_192:1;
	} rate;

	uint8_t all;
};

struct audio_speaker_flags {
	uint32_t FL_FR:1;
	uint32_t LFE:1;
	uint32_t FC:1;
	uint32_t RL_RR:1;
	uint32_t RC:1;
	uint32_t FLC_FRC:1;
	uint32_t RLC_RRC:1;
	uint32_t SUPPORT_AI:1;
};

struct audio_speaker_info {
	uint32_t ALLSPEAKERS:7;
	uint32_t SUPPORT_AI:1;
};


struct audio_info_flags {

	union {

		struct audio_speaker_flags speaker_flags;
		struct audio_speaker_info   info;

		uint8_t all;
	};
};

enum audio_format_code {
	AUDIO_FORMAT_CODE_FIRST = 1,
	AUDIO_FORMAT_CODE_LINEARPCM = AUDIO_FORMAT_CODE_FIRST,

	AUDIO_FORMAT_CODE_AC3,
	 
	AUDIO_FORMAT_CODE_MPEG1,
	 
	AUDIO_FORMAT_CODE_MP3,
	 
	AUDIO_FORMAT_CODE_MPEG2,
	AUDIO_FORMAT_CODE_AAC,
	AUDIO_FORMAT_CODE_DTS,
	AUDIO_FORMAT_CODE_ATRAC,
	AUDIO_FORMAT_CODE_1BITAUDIO,
	AUDIO_FORMAT_CODE_DOLBYDIGITALPLUS,
	AUDIO_FORMAT_CODE_DTS_HD,
	AUDIO_FORMAT_CODE_MAT_MLP,
	AUDIO_FORMAT_CODE_DST,
	AUDIO_FORMAT_CODE_WMAPRO,
	AUDIO_FORMAT_CODE_LAST,
	AUDIO_FORMAT_CODE_COUNT =
		AUDIO_FORMAT_CODE_LAST - AUDIO_FORMAT_CODE_FIRST
};

struct audio_mode {
	  
	enum audio_format_code format_code;
	 
	uint8_t channel_count;
	 
	union audio_sample_rates sample_rates;
	union {
		 
		uint8_t sample_size;
		 
		uint8_t max_bit_rate;
		 
		uint8_t vendor_specific;
	};
};

struct audio_info {
	struct audio_info_flags flags;
	uint32_t video_latency;
	uint32_t audio_latency;
	uint32_t display_index;
	uint8_t display_name[AUDIO_INFO_DISPLAY_NAME_SIZE_IN_CHARS];
	uint32_t manufacture_id;
	uint32_t product_id;
	 
	uint32_t port_id[2];
	uint32_t mode_count;
	 
	struct audio_mode modes[DC_MAX_AUDIO_DESC_COUNT];
};
struct audio_check {
	unsigned int audio_packet_type;
	unsigned int max_audiosample_rate;
	unsigned int acat;
};
enum dc_infoframe_type {
	DC_HDMI_INFOFRAME_TYPE_VENDOR = 0x81,
	DC_HDMI_INFOFRAME_TYPE_AVI = 0x82,
	DC_HDMI_INFOFRAME_TYPE_SPD = 0x83,
	DC_HDMI_INFOFRAME_TYPE_AUDIO = 0x84,
	DC_DP_INFOFRAME_TYPE_PPS = 0x10,
};

struct dc_info_packet {
	bool valid;
	uint8_t hb0;
	uint8_t hb1;
	uint8_t hb2;
	uint8_t hb3;
	uint8_t sb[32];
};

struct dc_info_packet_128 {
	bool valid;
	uint8_t hb0;
	uint8_t hb1;
	uint8_t hb2;
	uint8_t hb3;
	uint8_t sb[128];
};

#define DC_PLANE_UPDATE_TIMES_MAX 10

struct dc_plane_flip_time {
	unsigned int time_elapsed_in_us[DC_PLANE_UPDATE_TIMES_MAX];
	unsigned int index;
	unsigned int prev_update_time_in_us;
};

enum dc_psr_state {
	PSR_STATE0 = 0x0,
	PSR_STATE1,
	PSR_STATE1a,
	PSR_STATE2,
	PSR_STATE2a,
	PSR_STATE2b,
	PSR_STATE3,
	PSR_STATE3Init,
	PSR_STATE4,
	PSR_STATE4a,
	PSR_STATE4b,
	PSR_STATE4c,
	PSR_STATE4d,
	PSR_STATE4_FULL_FRAME,
	PSR_STATE4a_FULL_FRAME,
	PSR_STATE4b_FULL_FRAME,
	PSR_STATE4c_FULL_FRAME,
	PSR_STATE4_FULL_FRAME_POWERUP,
	PSR_STATE4_FULL_FRAME_HW_LOCK,
	PSR_STATE5,
	PSR_STATE5a,
	PSR_STATE5b,
	PSR_STATE5c,
	PSR_STATE_HWLOCK_MGR,
	PSR_STATE_POLLVUPDATE,
	PSR_STATE_INVALID = 0xFF
};

struct psr_config {
	unsigned char psr_version;
	unsigned int psr_rfb_setup_time;
	bool psr_exit_link_training_required;
	bool psr_frame_capture_indication_req;
	unsigned int psr_sdp_transmit_line_num_deadline;
	bool allow_smu_optimizations;
	bool allow_multi_disp_optimizations;
	 
	bool su_granularity_required;
	 
	uint8_t su_y_granularity;
	unsigned int line_time_in_us;
	uint8_t rate_control_caps;
	uint16_t dsc_slice_height;
};

union dmcu_psr_level {
	struct {
		unsigned int SKIP_CRC:1;
		unsigned int SKIP_DP_VID_STREAM_DISABLE:1;
		unsigned int SKIP_PHY_POWER_DOWN:1;
		unsigned int SKIP_AUX_ACK_CHECK:1;
		unsigned int SKIP_CRTC_DISABLE:1;
		unsigned int SKIP_AUX_RFB_CAPTURE_CHECK:1;
		unsigned int SKIP_SMU_NOTIFICATION:1;
		unsigned int SKIP_AUTO_STATE_ADVANCE:1;
		unsigned int DISABLE_PSR_ENTRY_ABORT:1;
		unsigned int SKIP_SINGLE_OTG_DISABLE:1;
		unsigned int DISABLE_ALPM:1;
		unsigned int ALPM_DEFAULT_PD_MODE:1;
		unsigned int RESERVED:20;
	} bits;
	unsigned int u32all;
};

enum physical_phy_id {
	PHYLD_0,
	PHYLD_1,
	PHYLD_2,
	PHYLD_3,
	PHYLD_4,
	PHYLD_5,
	PHYLD_6,
	PHYLD_7,
	PHYLD_8,
	PHYLD_9,
	PHYLD_COUNT,
	PHYLD_UNKNOWN = (-1L)
};

enum phy_type {
	PHY_TYPE_UNKNOWN  = 1,
	PHY_TYPE_PCIE_PHY = 2,
	PHY_TYPE_UNIPHY = 3,
};

struct psr_context {
	 
	enum channel_id channel;
	 
	enum transmitter transmitterId;
	 
	enum engine_id engineId;
	 
	enum controller_id controllerId;
	 
	enum phy_type phyType;
	 
	enum physical_phy_id smuPhyId;
	 
	unsigned int crtcTimingVerticalTotal;
	 
	bool psrSupportedDisplayConfig;
	 
	bool psrExitLinkTrainingRequired;
	 
	bool psrFrameCaptureIndicationReq;
	 
	unsigned int sdpTransmitLineNumDeadline;
	 
	unsigned int vsync_rate_hz;
	unsigned int skipPsrWaitForPllLock;
	unsigned int numberOfControllers;
	 
	bool rfb_update_auto_en;
	 
	unsigned int timehyst_frames;
	 
	unsigned int hyst_lines;
	 
	unsigned int aux_repeats;
	 
	union dmcu_psr_level psr_level;
	 
	unsigned int frame_delay;
	bool allow_smu_optimizations;
	bool allow_multi_disp_optimizations;
	 
	bool su_granularity_required;
	 
	uint8_t su_y_granularity;
	unsigned int line_time_in_us;
	uint8_t rate_control_caps;
	uint16_t dsc_slice_height;
};

struct colorspace_transform {
	struct fixed31_32 matrix[12];
	bool enable_remap;
};

enum i2c_mot_mode {
	I2C_MOT_UNDEF,
	I2C_MOT_TRUE,
	I2C_MOT_FALSE
};

struct AsicStateEx {
	unsigned int memoryClock;
	unsigned int displayClock;
	unsigned int engineClock;
	unsigned int maxSupportedDppClock;
	unsigned int dppClock;
	unsigned int socClock;
	unsigned int dcfClockDeepSleep;
	unsigned int fClock;
	unsigned int phyClock;
};


enum dc_clock_type {
	DC_CLOCK_TYPE_DISPCLK = 0,
	DC_CLOCK_TYPE_DPPCLK        = 1,
};

struct dc_clock_config {
	uint32_t max_clock_khz;
	uint32_t min_clock_khz;
	uint32_t bw_requirequired_clock_khz;
	uint32_t current_clock_khz; 
};

struct hw_asic_id {
	uint32_t chip_id;
	uint32_t chip_family;
	uint32_t pci_revision_id;
	uint32_t hw_internal_rev;
	uint32_t vram_type;
	uint32_t vram_width;
	uint32_t feature_flags;
	uint32_t fake_paths_num;
	void *atombios_base_address;
};

struct dc_context {
	struct dc *dc;

	void *driver_context;  
	struct dc_perf_trace *perf_trace;
	void *cgs_device;

	enum dce_environment dce_environment;
	struct hw_asic_id asic_id;

	 
	enum dce_version dce_version;
	struct dc_bios *dc_bios;
	bool created_bios;
	struct gpio_service *gpio_service;
	uint32_t dc_sink_id_count;
	uint32_t dc_stream_id_count;
	uint32_t dc_edp_id_count;
	uint64_t fbc_gpu_addr;
	struct dc_dmub_srv *dmub_srv;
	struct cp_psp cp_psp;
	uint32_t *dcn_reg_offsets;
	uint32_t *nbio_reg_offsets;
};

 
union dsc_slice_caps1 {
	struct {
		uint8_t NUM_SLICES_1 : 1;
		uint8_t NUM_SLICES_2 : 1;
		uint8_t RESERVED : 1;
		uint8_t NUM_SLICES_4 : 1;
		uint8_t NUM_SLICES_6 : 1;
		uint8_t NUM_SLICES_8 : 1;
		uint8_t NUM_SLICES_10 : 1;
		uint8_t NUM_SLICES_12 : 1;
	} bits;
	uint8_t raw;
};

union dsc_slice_caps2 {
	struct {
		uint8_t NUM_SLICES_16 : 1;
		uint8_t NUM_SLICES_20 : 1;
		uint8_t NUM_SLICES_24 : 1;
		uint8_t RESERVED : 5;
	} bits;
	uint8_t raw;
};

union dsc_color_formats {
	struct {
		uint8_t RGB : 1;
		uint8_t YCBCR_444 : 1;
		uint8_t YCBCR_SIMPLE_422 : 1;
		uint8_t YCBCR_NATIVE_422 : 1;
		uint8_t YCBCR_NATIVE_420 : 1;
		uint8_t RESERVED : 3;
	} bits;
	uint8_t raw;
};

union dsc_color_depth {
	struct {
		uint8_t RESERVED1 : 1;
		uint8_t COLOR_DEPTH_8_BPC : 1;
		uint8_t COLOR_DEPTH_10_BPC : 1;
		uint8_t COLOR_DEPTH_12_BPC : 1;
		uint8_t RESERVED2 : 3;
	} bits;
	uint8_t raw;
};

struct dsc_dec_dpcd_caps {
	bool is_dsc_supported;
	uint8_t dsc_version;
	int32_t rc_buffer_size;  
	union dsc_slice_caps1 slice_caps1;
	union dsc_slice_caps2 slice_caps2;
	int32_t lb_bit_depth;
	bool is_block_pred_supported;
	int32_t edp_max_bits_per_pixel;  
	union dsc_color_formats color_formats;
	union dsc_color_depth color_depth;
	int32_t throughput_mode_0_mps;  
	int32_t throughput_mode_1_mps;  
	int32_t max_slice_width;
	uint32_t bpp_increment_div;  

	 
	uint32_t branch_overall_throughput_0_mps;  
	uint32_t branch_overall_throughput_1_mps;  
	uint32_t branch_max_line_width;
	bool is_dp;  
};

struct dc_golden_table {
	uint16_t dc_golden_table_ver;
	uint32_t aux_dphy_rx_control0_val;
	uint32_t aux_dphy_tx_control_val;
	uint32_t aux_dphy_rx_control1_val;
	uint32_t dc_gpio_aux_ctrl_0_val;
	uint32_t dc_gpio_aux_ctrl_1_val;
	uint32_t dc_gpio_aux_ctrl_2_val;
	uint32_t dc_gpio_aux_ctrl_3_val;
	uint32_t dc_gpio_aux_ctrl_4_val;
	uint32_t dc_gpio_aux_ctrl_5_val;
};

enum dc_gpu_mem_alloc_type {
	DC_MEM_ALLOC_TYPE_GART,
	DC_MEM_ALLOC_TYPE_FRAME_BUFFER,
	DC_MEM_ALLOC_TYPE_INVISIBLE_FRAME_BUFFER,
	DC_MEM_ALLOC_TYPE_AGP
};

enum dc_link_encoding_format {
	DC_LINK_ENCODING_UNSPECIFIED = 0,
	DC_LINK_ENCODING_DP_8b_10b,
	DC_LINK_ENCODING_DP_128b_132b,
	DC_LINK_ENCODING_HDMI_TMDS,
	DC_LINK_ENCODING_HDMI_FRL
};

enum dc_psr_version {
	DC_PSR_VERSION_1			= 0,
	DC_PSR_VERSION_SU_1			= 1,
	DC_PSR_VERSION_UNSUPPORTED		= 0xFFFFFFFF,
};

 
enum display_endpoint_type {
	DISPLAY_ENDPOINT_PHY = 0,  
	DISPLAY_ENDPOINT_USB4_DPIA,  
	DISPLAY_ENDPOINT_UNKNOWN = -1
};

 
struct display_endpoint_id {
	struct graphics_object_id link_id;
	enum display_endpoint_type ep_type;
};

#if defined(CONFIG_DRM_AMD_SECURE_DISPLAY)
struct otg_phy_mux {
	uint8_t phy_output_num;
	uint8_t otg_output_num;
};
#endif

enum dc_detect_reason {
	DETECT_REASON_BOOT,
	DETECT_REASON_RESUMEFROMS3S4,
	DETECT_REASON_HPD,
	DETECT_REASON_HPDRX,
	DETECT_REASON_FALLBACK,
	DETECT_REASON_RETRAIN,
	DETECT_REASON_TDR,
};

struct dc_link_status {
	bool link_active;
	struct dpcd_caps *dpcd_caps;
};

union hdcp_rx_caps {
	struct {
		uint8_t version;
		uint8_t reserved;
		struct {
			uint8_t repeater	: 1;
			uint8_t hdcp_capable	: 1;
			uint8_t reserved	: 6;
		} byte0;
	} fields;
	uint8_t raw[3];
};

union hdcp_bcaps {
	struct {
		uint8_t HDCP_CAPABLE:1;
		uint8_t REPEATER:1;
		uint8_t RESERVED:6;
	} bits;
	uint8_t raw;
};

struct hdcp_caps {
	union hdcp_rx_caps rx_caps;
	union hdcp_bcaps bcaps;
};

 
struct link_mst_stream_allocation {
	 
	const struct stream_encoder *stream_enc;
	 
	const struct hpo_dp_stream_encoder *hpo_dp_stream_enc;
	 
	uint8_t vcp_id;
	 
	uint8_t slot_count;
};

#define MAX_CONTROLLER_NUM 6

 
struct link_mst_stream_allocation_table {
	 
	int stream_count;
	 
	struct link_mst_stream_allocation stream_allocations[MAX_CONTROLLER_NUM];
};

 
struct psr_settings {
	bool psr_feature_enabled;		
	bool psr_allow_active;			
	enum dc_psr_version psr_version;		
	bool psr_vtotal_control_support;	
	unsigned long long psr_dirty_rects_change_timestamp_ns;	

	 
	bool psr_frame_capture_indication_req;
	unsigned int psr_sdp_transmit_line_num_deadline;
	uint8_t force_ffu_mode;
	unsigned int psr_power_opt;
};

enum replay_coasting_vtotal_type {
	PR_COASTING_TYPE_NOM = 0,
	PR_COASTING_TYPE_STATIC,
	PR_COASTING_TYPE_FULL_SCREEN_VIDEO,
	PR_COASTING_TYPE_TEST_HARNESS,
	PR_COASTING_TYPE_NUM,
};

union replay_error_status {
	struct {
		unsigned char STATE_TRANSITION_ERROR    :1;
		unsigned char LINK_CRC_ERROR            :1;
		unsigned char DESYNC_ERROR              :1;
		unsigned char RESERVED                  :5;
	} bits;
	unsigned char raw;
};

struct replay_config {
	bool replay_supported;                          
	unsigned int replay_power_opt_supported;        
	bool replay_smu_opt_supported;                  
	unsigned int replay_enable_option;              
	uint32_t debug_flags;                           
	bool replay_timing_sync_supported;             
	union replay_error_status replay_error_status; 
};

 
struct replay_settings {
	struct replay_config config;            
	bool replay_feature_enabled;            
	bool replay_allow_active;               
	unsigned int replay_power_opt_active;   
	bool replay_smu_opt_enable;             
	uint16_t coasting_vtotal;               
	uint16_t coasting_vtotal_table[PR_COASTING_TYPE_NUM]; 
};

 
struct dc_panel_config {
	 
	struct pps {
		unsigned int extra_t3_ms;
		unsigned int extra_t7_ms;
		unsigned int extra_delay_backlight_off;
		unsigned int extra_post_t7_ms;
		unsigned int extra_pre_t11_ms;
		unsigned int extra_t12_ms;
		unsigned int extra_post_OUI_ms;
	} pps;
	 
	struct nits_brightness {
		unsigned int peak;  
		unsigned int max_avg;  
		unsigned int min;  
		unsigned int max_nonboost_brightness_millinits;
		unsigned int min_brightness_millinits;
	} nits_brightness;
	 
	struct psr {
		bool disable_psr;
		bool disallow_psrsu;
		bool disallow_replay;
		bool rc_disable;
		bool rc_allow_static_screen;
		bool rc_allow_fullscreen_VPB;
		unsigned int replay_enable_option;
	} psr;
	 
	struct varib {
		unsigned int varibright_feature_enable;
		unsigned int def_varibright_level;
		unsigned int abm_config_setting;
	} varib;
	 
	struct dsc {
		bool disable_dsc_edp;
		unsigned int force_dsc_edp_policy;
	} dsc;
	 
	struct ilr {
		bool optimize_edp_link_rate;  
	} ilr;
};

 
struct dc_dpia_bw_alloc {
	int sink_verified_bw;  
	int sink_allocated_bw; 
	int sink_max_bw;       
	int estimated_bw;      
	int bw_granularity;    
	bool bw_alloc_enabled; 
	bool response_ready;   
};

#define MAX_SINKS_PER_LINK 4

enum dc_hpd_enable_select {
	HPD_EN_FOR_ALL_EDP = 0,
	HPD_EN_FOR_PRIMARY_EDP_ONLY,
	HPD_EN_FOR_SECONDARY_EDP_ONLY,
};

#endif  
