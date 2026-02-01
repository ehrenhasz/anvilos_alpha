 

#ifndef DC_HW_TYPES_H
#define DC_HW_TYPES_H

#include "os_types.h"
#include "fixed31_32.h"
#include "signal_types.h"

 

union large_integer {
	struct {
		uint32_t low_part;
		int32_t high_part;
	};

	struct {
		uint32_t low_part;
		int32_t high_part;
	} u;

	int64_t quad_part;
};

#define PHYSICAL_ADDRESS_LOC union large_integer

enum dc_plane_addr_type {
	PLN_ADDR_TYPE_GRAPHICS = 0,
	PLN_ADDR_TYPE_GRPH_STEREO,
	PLN_ADDR_TYPE_VIDEO_PROGRESSIVE,
	PLN_ADDR_TYPE_RGBEA
};

struct dc_plane_address {
	enum dc_plane_addr_type type;
	bool tmz_surface;
	union {
		struct{
			PHYSICAL_ADDRESS_LOC addr;
			PHYSICAL_ADDRESS_LOC cursor_cache_addr;
			PHYSICAL_ADDRESS_LOC meta_addr;
			union large_integer dcc_const_color;
		} grph;

		 
		struct {
			PHYSICAL_ADDRESS_LOC left_addr;
			PHYSICAL_ADDRESS_LOC left_meta_addr;
			union large_integer left_dcc_const_color;

			PHYSICAL_ADDRESS_LOC right_addr;
			PHYSICAL_ADDRESS_LOC right_meta_addr;
			union large_integer right_dcc_const_color;

			PHYSICAL_ADDRESS_LOC left_alpha_addr;
			PHYSICAL_ADDRESS_LOC left_alpha_meta_addr;
			union large_integer left_alpha_dcc_const_color;

			PHYSICAL_ADDRESS_LOC right_alpha_addr;
			PHYSICAL_ADDRESS_LOC right_alpha_meta_addr;
			union large_integer right_alpha_dcc_const_color;

		} grph_stereo;

		 
		struct {
			PHYSICAL_ADDRESS_LOC luma_addr;
			PHYSICAL_ADDRESS_LOC luma_meta_addr;
			union large_integer luma_dcc_const_color;

			PHYSICAL_ADDRESS_LOC chroma_addr;
			PHYSICAL_ADDRESS_LOC chroma_meta_addr;
			union large_integer chroma_dcc_const_color;
		} video_progressive;

		struct {
			PHYSICAL_ADDRESS_LOC addr;
			PHYSICAL_ADDRESS_LOC meta_addr;
			union large_integer dcc_const_color;

			PHYSICAL_ADDRESS_LOC alpha_addr;
			PHYSICAL_ADDRESS_LOC alpha_meta_addr;
			union large_integer alpha_dcc_const_color;
		} rgbea;
	};

	union large_integer page_table_base;

	uint8_t vmid;
};

struct dc_size {
	int width;
	int height;
};

struct rect {
	int x;
	int y;
	int width;
	int height;
};

struct plane_size {
	 
	int surface_pitch;
	int chroma_pitch;
	struct rect surface_size;
	struct rect chroma_size;
};

struct dc_plane_dcc_param {
	bool enable;

	int meta_pitch;
	bool independent_64b_blks;
	uint8_t dcc_ind_blk;

	int meta_pitch_c;
	bool independent_64b_blks_c;
	uint8_t dcc_ind_blk_c;
};

 
enum surface_pixel_format {
	SURFACE_PIXEL_FORMAT_GRPH_BEGIN = 0,
	 
	SURFACE_PIXEL_FORMAT_GRPH_PALETA_256_COLORS =
		SURFACE_PIXEL_FORMAT_GRPH_BEGIN,
	 
	SURFACE_PIXEL_FORMAT_GRPH_ARGB1555,
	 
	SURFACE_PIXEL_FORMAT_GRPH_RGB565,
	 
	SURFACE_PIXEL_FORMAT_GRPH_ARGB8888,
	 
	SURFACE_PIXEL_FORMAT_GRPH_ABGR8888,

	SURFACE_PIXEL_FORMAT_GRPH_ARGB2101010,
	 
	SURFACE_PIXEL_FORMAT_GRPH_ABGR2101010,
	 
	SURFACE_PIXEL_FORMAT_GRPH_ABGR2101010_XR_BIAS,
	 
	SURFACE_PIXEL_FORMAT_GRPH_ARGB16161616,
	 
	SURFACE_PIXEL_FORMAT_GRPH_ABGR16161616,
	 
	SURFACE_PIXEL_FORMAT_GRPH_ARGB16161616F,
	 
	SURFACE_PIXEL_FORMAT_GRPH_ABGR16161616F,
	 
	SURFACE_PIXEL_FORMAT_GRPH_RGB111110_FIX,
	SURFACE_PIXEL_FORMAT_GRPH_BGR101111_FIX,
	SURFACE_PIXEL_FORMAT_GRPH_RGB111110_FLOAT,
	SURFACE_PIXEL_FORMAT_GRPH_BGR101111_FLOAT,
	SURFACE_PIXEL_FORMAT_GRPH_RGBE,
	SURFACE_PIXEL_FORMAT_GRPH_RGBE_ALPHA,
	SURFACE_PIXEL_FORMAT_VIDEO_BEGIN,
	SURFACE_PIXEL_FORMAT_VIDEO_420_YCbCr =
		SURFACE_PIXEL_FORMAT_VIDEO_BEGIN,
	SURFACE_PIXEL_FORMAT_VIDEO_420_YCrCb,
	SURFACE_PIXEL_FORMAT_VIDEO_420_10bpc_YCbCr,
	SURFACE_PIXEL_FORMAT_VIDEO_420_10bpc_YCrCb,
	SURFACE_PIXEL_FORMAT_SUBSAMPLE_END,
	SURFACE_PIXEL_FORMAT_VIDEO_ACrYCb2101010 =
		SURFACE_PIXEL_FORMAT_SUBSAMPLE_END,
	SURFACE_PIXEL_FORMAT_VIDEO_CrYCbA1010102,
	SURFACE_PIXEL_FORMAT_VIDEO_AYCrCb8888,
	SURFACE_PIXEL_FORMAT_INVALID

	 
};



 
enum pixel_format {
	 
	PIXEL_FORMAT_UNINITIALIZED,
	PIXEL_FORMAT_INDEX8,
	PIXEL_FORMAT_RGB565,
	PIXEL_FORMAT_ARGB8888,
	PIXEL_FORMAT_ARGB2101010,
	PIXEL_FORMAT_ARGB2101010_XRBIAS,
	PIXEL_FORMAT_FP16,
	 
	PIXEL_FORMAT_420BPP8,
	PIXEL_FORMAT_420BPP10,
	 
	PIXEL_FORMAT_INVALID,

	PIXEL_FORMAT_GRPH_BEGIN = PIXEL_FORMAT_INDEX8,
	PIXEL_FORMAT_GRPH_END = PIXEL_FORMAT_FP16,
	PIXEL_FORMAT_VIDEO_BEGIN = PIXEL_FORMAT_420BPP8,
	PIXEL_FORMAT_VIDEO_END = PIXEL_FORMAT_420BPP10,
	PIXEL_FORMAT_UNKNOWN
};

 
#define DC_MAX_DIRTY_RECTS 3
struct dc_flip_addrs {
	struct dc_plane_address address;
	unsigned int flip_timestamp_in_us;
	bool flip_immediate;
	 
	bool triplebuffer_flips;
	unsigned int dirty_rect_count;
	struct rect dirty_rects[DC_MAX_DIRTY_RECTS];
};

enum tile_split_values {
	DC_DISPLAY_MICRO_TILING = 0x0,
	DC_THIN_MICRO_TILING = 0x1,
	DC_DEPTH_MICRO_TILING = 0x2,
	DC_ROTATED_MICRO_TILING = 0x3,
};

enum tripleBuffer_enable {
	DC_TRIPLEBUFFER_DISABLE = 0x0,
	DC_TRIPLEBUFFER_ENABLE = 0x1,
};

 
enum array_mode_values {
	DC_ARRAY_LINEAR_GENERAL = 0,
	DC_ARRAY_LINEAR_ALLIGNED,
	DC_ARRAY_1D_TILED_THIN1,
	DC_ARRAY_1D_TILED_THICK,
	DC_ARRAY_2D_TILED_THIN1,
	DC_ARRAY_PRT_TILED_THIN1,
	DC_ARRAY_PRT_2D_TILED_THIN1,
	DC_ARRAY_2D_TILED_THICK,
	DC_ARRAY_2D_TILED_X_THICK,
	DC_ARRAY_PRT_TILED_THICK,
	DC_ARRAY_PRT_2D_TILED_THICK,
	DC_ARRAY_PRT_3D_TILED_THIN1,
	DC_ARRAY_3D_TILED_THIN1,
	DC_ARRAY_3D_TILED_THICK,
	DC_ARRAY_3D_TILED_X_THICK,
	DC_ARRAY_PRT_3D_TILED_THICK,
};

enum tile_mode_values {
	DC_ADDR_SURF_MICRO_TILING_DISPLAY = 0x0,
	DC_ADDR_SURF_MICRO_TILING_NON_DISPLAY = 0x1,
};

enum swizzle_mode_values {
	DC_SW_LINEAR = 0,
	DC_SW_256B_S = 1,
	DC_SW_256_D = 2,
	DC_SW_256_R = 3,
	DC_SW_4KB_S = 5,
	DC_SW_4KB_D = 6,
	DC_SW_4KB_R = 7,
	DC_SW_64KB_S = 9,
	DC_SW_64KB_D = 10,
	DC_SW_64KB_R = 11,
	DC_SW_VAR_S = 13,
	DC_SW_VAR_D = 14,
	DC_SW_VAR_R = 15,
	DC_SW_64KB_S_T = 17,
	DC_SW_64KB_D_T = 18,
	DC_SW_4KB_S_X = 21,
	DC_SW_4KB_D_X = 22,
	DC_SW_4KB_R_X = 23,
	DC_SW_64KB_S_X = 25,
	DC_SW_64KB_D_X = 26,
	DC_SW_64KB_R_X = 27,
	DC_SW_VAR_S_X = 29,
	DC_SW_VAR_D_X = 30,
	DC_SW_VAR_R_X = 31,
	DC_SW_MAX = 32,
	DC_SW_UNKNOWN = DC_SW_MAX
};

union dc_tiling_info {

	struct {
		 
		unsigned int num_banks;
		 
		unsigned int bank_width;
		unsigned int bank_width_c;
		 
		unsigned int bank_height;
		unsigned int bank_height_c;
		 
		unsigned int tile_aspect;
		unsigned int tile_aspect_c;
		 
		enum tile_split_values tile_split;
		enum tile_split_values tile_split_c;
		 
		enum tile_mode_values tile_mode;
		enum tile_mode_values tile_mode_c;
		 
		unsigned int pipe_config;
		 
		enum array_mode_values array_mode;
	} gfx8;

	struct {
		enum swizzle_mode_values swizzle;
		unsigned int num_pipes;
		unsigned int max_compressed_frags;
		unsigned int pipe_interleave;

		unsigned int num_banks;
		unsigned int num_shader_engines;
		unsigned int num_rb_per_se;
		bool shaderEnable;

		bool meta_linear;
		bool rb_aligned;
		bool pipe_aligned;
		unsigned int num_pkrs;
	} gfx9;
};

 
enum dc_rotation_angle {
	ROTATION_ANGLE_0 = 0,
	ROTATION_ANGLE_90,
	ROTATION_ANGLE_180,
	ROTATION_ANGLE_270,
	ROTATION_ANGLE_COUNT
};

enum dc_scan_direction {
	SCAN_DIRECTION_UNKNOWN = 0,
	SCAN_DIRECTION_HORIZONTAL = 1,   
	SCAN_DIRECTION_VERTICAL = 2,     
};

 
struct dc_cursor_position {
	 
	uint32_t x;

	 
	uint32_t y;

	 
	uint32_t x_hotspot;

	 
	uint32_t y_hotspot;

	 
	bool enable;

	 
	bool translate_by_source;
};

struct dc_cursor_mi_param {
	unsigned int pixel_clk_khz;
	unsigned int ref_clk_khz;
	struct rect viewport;
	struct fixed31_32 h_scale_ratio;
	struct fixed31_32 v_scale_ratio;
	enum dc_rotation_angle rotation;
	bool mirror;
	struct dc_stream_state *stream;
};

 

enum {
	GAMMA_RGB_256_ENTRIES = 256,
	GAMMA_RGB_FLOAT_1024_ENTRIES = 1024,
	GAMMA_CS_TFM_1D_ENTRIES = 4096,
	GAMMA_CUSTOM_ENTRIES = 4096,
	GAMMA_MAX_ENTRIES = 4096
};

enum dc_gamma_type {
	GAMMA_RGB_256 = 1,
	GAMMA_RGB_FLOAT_1024 = 2,
	GAMMA_CS_TFM_1D = 3,
	GAMMA_CUSTOM = 4,
};

struct dc_csc_transform {
	uint16_t matrix[12];
	bool enable_adjustment;
};

struct dc_rgb_fixed {
	struct fixed31_32 red;
	struct fixed31_32 green;
	struct fixed31_32 blue;
};

struct dc_gamma {
	struct kref refcount;
	enum dc_gamma_type type;
	unsigned int num_entries;

	struct dc_gamma_entries {
		struct fixed31_32 red[GAMMA_MAX_ENTRIES];
		struct fixed31_32 green[GAMMA_MAX_ENTRIES];
		struct fixed31_32 blue[GAMMA_MAX_ENTRIES];
	} entries;

	 
	struct dc_context *ctx;

	 
	bool is_identity;
};

 
 

 
enum dc_cursor_color_format {
	CURSOR_MODE_MONO,
	CURSOR_MODE_COLOR_1BIT_AND,
	CURSOR_MODE_COLOR_PRE_MULTIPLIED_ALPHA,
	CURSOR_MODE_COLOR_UN_PRE_MULTIPLIED_ALPHA,
	CURSOR_MODE_COLOR_64BIT_FP_PRE_MULTIPLIED,
	CURSOR_MODE_COLOR_64BIT_FP_UN_PRE_MULTIPLIED
};

 

union dc_cursor_attribute_flags {
	struct {
		uint32_t ENABLE_MAGNIFICATION:1;
		uint32_t INVERSE_TRANSPARENT_CLAMPING:1;
		uint32_t HORIZONTAL_MIRROR:1;
		uint32_t VERTICAL_MIRROR:1;
		uint32_t INVERT_PIXEL_DATA:1;
		uint32_t ZERO_EXPANSION:1;
		uint32_t MIN_MAX_INVERT:1;
		uint32_t ENABLE_CURSOR_DEGAMMA:1;
		uint32_t RESERVED:24;
	} bits;
	uint32_t value;
};

struct dc_cursor_attributes {
	 
	PHYSICAL_ADDRESS_LOC address;

	 
	uint32_t pitch;

	 
	uint32_t width;
	 
	uint32_t height;

	 
	enum dc_cursor_color_format color_format;
	 
	uint32_t sdr_white_level;

	 
	enum dc_rotation_angle rotation_angle;

	union dc_cursor_attribute_flags attribute_flags;
};

struct dpp_cursor_attributes {
	int bias;
	int scale;
};

 

enum dc_color_space {
	COLOR_SPACE_UNKNOWN,
	COLOR_SPACE_SRGB,
	COLOR_SPACE_XR_RGB,
	COLOR_SPACE_SRGB_LIMITED,
	COLOR_SPACE_MSREF_SCRGB,
	COLOR_SPACE_YCBCR601,
	COLOR_SPACE_YCBCR709,
	COLOR_SPACE_XV_YCC_709,
	COLOR_SPACE_XV_YCC_601,
	COLOR_SPACE_YCBCR601_LIMITED,
	COLOR_SPACE_YCBCR709_LIMITED,
	COLOR_SPACE_2020_RGB_FULLRANGE,
	COLOR_SPACE_2020_RGB_LIMITEDRANGE,
	COLOR_SPACE_2020_YCBCR,
	COLOR_SPACE_ADOBERGB,
	COLOR_SPACE_DCIP3,
	COLOR_SPACE_DISPLAYNATIVE,
	COLOR_SPACE_DOLBYVISION,
	COLOR_SPACE_APPCTRL,
	COLOR_SPACE_CUSTOMPOINTS,
	COLOR_SPACE_YCBCR709_BLACK,
};

enum dc_dither_option {
	DITHER_OPTION_DEFAULT,
	DITHER_OPTION_DISABLE,
	DITHER_OPTION_FM6,
	DITHER_OPTION_FM8,
	DITHER_OPTION_FM10,
	DITHER_OPTION_SPATIAL6_FRAME_RANDOM,
	DITHER_OPTION_SPATIAL8_FRAME_RANDOM,
	DITHER_OPTION_SPATIAL10_FRAME_RANDOM,
	DITHER_OPTION_SPATIAL6,
	DITHER_OPTION_SPATIAL8,
	DITHER_OPTION_SPATIAL10,
	DITHER_OPTION_TRUN6,
	DITHER_OPTION_TRUN8,
	DITHER_OPTION_TRUN10,
	DITHER_OPTION_TRUN10_SPATIAL8,
	DITHER_OPTION_TRUN10_SPATIAL6,
	DITHER_OPTION_TRUN10_FM8,
	DITHER_OPTION_TRUN10_FM6,
	DITHER_OPTION_TRUN10_SPATIAL8_FM6,
	DITHER_OPTION_SPATIAL10_FM8,
	DITHER_OPTION_SPATIAL10_FM6,
	DITHER_OPTION_TRUN8_SPATIAL6,
	DITHER_OPTION_TRUN8_FM6,
	DITHER_OPTION_SPATIAL8_FM6,
	DITHER_OPTION_MAX = DITHER_OPTION_SPATIAL8_FM6,
	DITHER_OPTION_INVALID
};

enum dc_quantization_range {
	QUANTIZATION_RANGE_UNKNOWN,
	QUANTIZATION_RANGE_FULL,
	QUANTIZATION_RANGE_LIMITED
};

enum dc_dynamic_expansion {
	DYN_EXPANSION_AUTO,
	DYN_EXPANSION_DISABLE
};

 

 
struct scaling_taps {
	uint32_t v_taps;
	uint32_t h_taps;
	uint32_t v_taps_c;
	uint32_t h_taps_c;
	bool integer_scaling;
};

enum dc_timing_standard {
	DC_TIMING_STANDARD_UNDEFINED,
	DC_TIMING_STANDARD_DMT,
	DC_TIMING_STANDARD_GTF,
	DC_TIMING_STANDARD_CVT,
	DC_TIMING_STANDARD_CVT_RB,
	DC_TIMING_STANDARD_CEA770,
	DC_TIMING_STANDARD_CEA861,
	DC_TIMING_STANDARD_HDMI,
	DC_TIMING_STANDARD_TV_NTSC,
	DC_TIMING_STANDARD_TV_NTSC_J,
	DC_TIMING_STANDARD_TV_PAL,
	DC_TIMING_STANDARD_TV_PAL_M,
	DC_TIMING_STANDARD_TV_PAL_CN,
	DC_TIMING_STANDARD_TV_SECAM,
	DC_TIMING_STANDARD_EXPLICIT,
	 
	DC_TIMING_STANDARD_USER_OVERRIDE,
	 
	DC_TIMING_STANDARD_MAX
};

enum dc_color_depth {
	COLOR_DEPTH_UNDEFINED,
	COLOR_DEPTH_666,
	COLOR_DEPTH_888,
	COLOR_DEPTH_101010,
	COLOR_DEPTH_121212,
	COLOR_DEPTH_141414,
	COLOR_DEPTH_161616,
	COLOR_DEPTH_999,
	COLOR_DEPTH_111111,
	COLOR_DEPTH_COUNT
};

enum dc_pixel_encoding {
	PIXEL_ENCODING_UNDEFINED,
	PIXEL_ENCODING_RGB,
	PIXEL_ENCODING_YCBCR422,
	PIXEL_ENCODING_YCBCR444,
	PIXEL_ENCODING_YCBCR420,
	PIXEL_ENCODING_COUNT
};

enum dc_aspect_ratio {
	ASPECT_RATIO_NO_DATA,
	ASPECT_RATIO_4_3,
	ASPECT_RATIO_16_9,
	ASPECT_RATIO_64_27,
	ASPECT_RATIO_256_135,
	ASPECT_RATIO_FUTURE
};

enum scanning_type {
	SCANNING_TYPE_NODATA = 0,
	SCANNING_TYPE_OVERSCAN,
	SCANNING_TYPE_UNDERSCAN,
	SCANNING_TYPE_FUTURE,
	SCANNING_TYPE_UNDEFINED
};

struct dc_crtc_timing_flags {
	uint32_t INTERLACE :1;
	uint32_t HSYNC_POSITIVE_POLARITY :1;  
	uint32_t VSYNC_POSITIVE_POLARITY :1;  

	uint32_t HORZ_COUNT_BY_TWO:1;

	uint32_t EXCLUSIVE_3D :1;  
	uint32_t RIGHT_EYE_3D_POLARITY :1;  
	uint32_t SUB_SAMPLE_3D :1;  
	uint32_t USE_IN_3D_VIEW_ONLY :1;  
	uint32_t STEREO_3D_PREFERENCE :1;  
	uint32_t Y_ONLY :1;

	uint32_t YCBCR420 :1;  
	uint32_t DTD_COUNTER :5;  

	uint32_t FORCE_HDR :1;

	 
	uint32_t LTE_340MCSC_SCRAMBLE:1;

	uint32_t DSC : 1;  
#ifndef TRIM_FSFT
	uint32_t FAST_TRANSPORT: 1;
#endif
	uint32_t VBLANK_SYNCHRONIZABLE: 1;
};

enum dc_timing_3d_format {
	TIMING_3D_FORMAT_NONE,
	TIMING_3D_FORMAT_FRAME_ALTERNATE,  
	TIMING_3D_FORMAT_INBAND_FA,  
	TIMING_3D_FORMAT_DP_HDMI_INBAND_FA,  
	 
	TIMING_3D_FORMAT_SIDEBAND_FA,  
	TIMING_3D_FORMAT_HW_FRAME_PACKING,
	TIMING_3D_FORMAT_SW_FRAME_PACKING,
	TIMING_3D_FORMAT_ROW_INTERLEAVE,
	TIMING_3D_FORMAT_COLUMN_INTERLEAVE,
	TIMING_3D_FORMAT_PIXEL_INTERLEAVE,
	TIMING_3D_FORMAT_SIDE_BY_SIDE,
	TIMING_3D_FORMAT_TOP_AND_BOTTOM,
	TIMING_3D_FORMAT_SBS_SW_PACKED,
	 
	TIMING_3D_FORMAT_TB_SW_PACKED,
	 

	TIMING_3D_FORMAT_MAX,
};

#define DC_DSC_QP_SET_SIZE 15
#define DC_DSC_RC_BUF_THRESH_SIZE 14
struct dc_dsc_rc_params_override {
	int32_t rc_model_size;
	int32_t rc_buf_thresh[DC_DSC_RC_BUF_THRESH_SIZE];
	int32_t rc_minqp[DC_DSC_QP_SET_SIZE];
	int32_t rc_maxqp[DC_DSC_QP_SET_SIZE];
	int32_t rc_offset[DC_DSC_QP_SET_SIZE];

	int32_t rc_tgt_offset_hi;
	int32_t rc_tgt_offset_lo;
	int32_t rc_edge_factor;
	int32_t rc_quant_incr_limit0;
	int32_t rc_quant_incr_limit1;

	int32_t initial_fullness_offset;
	int32_t initial_delay;

	int32_t flatness_min_qp;
	int32_t flatness_max_qp;
	int32_t flatness_det_thresh;
};

struct dc_dsc_config {
	uint32_t num_slices_h;  
	uint32_t num_slices_v;  
	uint32_t bits_per_pixel;  
	bool block_pred_enable;  
	uint32_t linebuf_depth;  
	uint32_t version_minor;  
	bool ycbcr422_simple;  
	int32_t rc_buffer_size;  
#if defined(CONFIG_DRM_AMD_DC_FP)
	bool is_frl;  
#endif
	bool is_dp;  
	uint32_t mst_pbn;  
	const struct dc_dsc_rc_params_override *rc_params_ovrd;  
};

 
struct dc_crtc_timing {
	 
	uint32_t h_total;

	 
	uint32_t h_border_left;

	 
	uint32_t h_addressable;

	 
	uint32_t h_border_right;

	 
	uint32_t h_front_porch;

	 
	uint32_t h_sync_width;

	 
	uint32_t v_total;

	 
	uint32_t v_border_top;

	 
	uint32_t v_addressable;

	 
	uint32_t v_border_bottom;

	 
	uint32_t v_front_porch;

	 
	uint32_t v_sync_width;

	 
	uint32_t pix_clk_100hz;

	uint32_t min_refresh_in_uhz;

	uint32_t vic;
	uint32_t hdmi_vic;
	uint32_t rid;
	uint32_t fr_index;
	enum dc_timing_3d_format timing_3d_format;
	enum dc_color_depth display_color_depth;
	enum dc_pixel_encoding pixel_encoding;
	enum dc_aspect_ratio aspect_ratio;
	enum scanning_type scan_type;

#ifndef TRIM_FSFT
	uint32_t fast_transport_output_rate_100hz;
#endif

	struct dc_crtc_timing_flags flags;
	uint32_t dsc_fixed_bits_per_pixel_x16;  
	struct dc_dsc_config dsc_cfg;
};

enum trigger_delay {
	TRIGGER_DELAY_NEXT_PIXEL = 0,
	TRIGGER_DELAY_NEXT_LINE,
};

enum crtc_event {
	CRTC_EVENT_VSYNC_RISING = 0,
	CRTC_EVENT_VSYNC_FALLING
};

struct crtc_trigger_info {
	bool enabled;
	struct dc_stream_state *event_source;
	enum crtc_event event;
	enum trigger_delay delay;
};

struct dc_crtc_timing_adjust {
	uint32_t v_total_min;
	uint32_t v_total_max;
	uint32_t v_total_mid;
	uint32_t v_total_mid_frame_num;
};


 
enum vram_type {
	VIDEO_MEMORY_TYPE_GDDR5  = 2,
	VIDEO_MEMORY_TYPE_DDR3   = 3,
	VIDEO_MEMORY_TYPE_DDR4   = 4,
	VIDEO_MEMORY_TYPE_HBM    = 5,
	VIDEO_MEMORY_TYPE_GDDR6  = 6,
};

enum dwb_cnv_out_bpc {
	DWB_CNV_OUT_BPC_8BPC  = 0,
	DWB_CNV_OUT_BPC_10BPC = 1,
};

enum dwb_output_depth {
	DWB_OUTPUT_PIXEL_DEPTH_8BPC = 0,
	DWB_OUTPUT_PIXEL_DEPTH_10BPC = 1,
};

enum dwb_capture_rate {
	dwb_capture_rate_0 = 0,	 
	dwb_capture_rate_1 = 1,	 
	dwb_capture_rate_2 = 2,	 
	dwb_capture_rate_3 = 3,	 
};

enum dwb_scaler_mode {
	dwb_scaler_mode_bypass444 = 0,
	dwb_scaler_mode_rgb444 = 1,
	dwb_scaler_mode_yuv444 = 2,
	dwb_scaler_mode_yuv420 = 3
};

enum dwb_subsample_position {
	DWB_INTERSTITIAL_SUBSAMPLING = 0,
	DWB_COSITED_SUBSAMPLING      = 1
};

enum dwb_stereo_eye_select {
	DWB_STEREO_EYE_LEFT  = 1,		 
	DWB_STEREO_EYE_RIGHT = 2,		 
};

enum dwb_stereo_type {
	DWB_STEREO_TYPE_FRAME_PACKING = 0,		 
	DWB_STEREO_TYPE_FRAME_SEQUENTIAL = 3,	 
};

enum dwb_out_format {
	DWB_OUT_FORMAT_32BPP_ARGB = 0,
	DWB_OUT_FORMAT_32BPP_RGBA = 1,
	DWB_OUT_FORMAT_64BPP_ARGB = 2,
	DWB_OUT_FORMAT_64BPP_RGBA = 3
};

enum dwb_out_denorm {
	DWB_OUT_DENORM_10BPC = 0,
	DWB_OUT_DENORM_8BPC = 1,
	DWB_OUT_DENORM_BYPASS = 2
};

enum cm_gamut_remap_select {
	CM_GAMUT_REMAP_MODE_BYPASS = 0,
	CM_GAMUT_REMAP_MODE_RAMA_COEFF,
	CM_GAMUT_REMAP_MODE_RAMB_COEFF,
	CM_GAMUT_REMAP_MODE_RESERVED
};

enum cm_gamut_coef_format {
	CM_GAMUT_REMAP_COEF_FORMAT_S2_13 = 0,
	CM_GAMUT_REMAP_COEF_FORMAT_S3_12 = 1
};

struct mcif_warmup_params {
	union large_integer	start_address;
	unsigned int		address_increment;
	unsigned int		region_size;
	unsigned int		p_vmid;
};

#define MCIF_BUF_COUNT	4

struct mcif_buf_params {
	unsigned long long	luma_address[MCIF_BUF_COUNT];
	unsigned long long	chroma_address[MCIF_BUF_COUNT];
	unsigned int		luma_pitch;
	unsigned int		chroma_pitch;
	unsigned int		warmup_pitch;
	unsigned int		swlock;
	unsigned int		p_vmid;
};


#define MAX_TG_COLOR_VALUE 0x3FF
struct tg_color {
	 
	uint16_t color_r_cr;
	uint16_t color_g_y;
	uint16_t color_b_cb;
};

enum symclk_state {
	SYMCLK_OFF_TX_OFF,
	SYMCLK_ON_TX_ON,
	SYMCLK_ON_TX_OFF,
};

struct phy_state {
	struct {
		uint8_t otg		: 1;
		uint8_t reserved	: 7;
	} symclk_ref_cnts;
	enum symclk_state symclk_state;
};

#endif  

