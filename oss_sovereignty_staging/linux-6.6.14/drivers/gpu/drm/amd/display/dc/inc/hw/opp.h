 

#ifndef __DAL_OPP_H__
#define __DAL_OPP_H__

#include "hw_shared.h"
#include "dc_hw_types.h"
#include "transform.h"
#include "mpc.h"

struct fixed31_32;

 
enum clamping_range {
	CLAMPING_FULL_RANGE = 0,	    
	CLAMPING_LIMITED_RANGE_8BPC,    
	CLAMPING_LIMITED_RANGE_10BPC,  
	CLAMPING_LIMITED_RANGE_12BPC,  
	 
	CLAMPING_LIMITED_RANGE_PROGRAMMABLE
};

struct clamping_and_pixel_encoding_params {
	enum dc_pixel_encoding pixel_encoding;  
	enum clamping_range clamping_level;  
	enum dc_color_depth c_depth;  
};

struct bit_depth_reduction_params {
	struct {
		 
		 
		uint32_t TRUNCATE_ENABLED:1;
		 
		uint32_t TRUNCATE_DEPTH:2;
		 
		uint32_t TRUNCATE_MODE:1;

		 
		 
		uint32_t SPATIAL_DITHER_ENABLED:1;
		 
		uint32_t SPATIAL_DITHER_DEPTH:2;
		 
		uint32_t SPATIAL_DITHER_MODE:2;
		 
		uint32_t RGB_RANDOM:1;
		 
		uint32_t FRAME_RANDOM:1;
		 
		uint32_t HIGHPASS_RANDOM:1;

		 
		  
		uint32_t FRAME_MODULATION_ENABLED:1;
		 
		uint32_t FRAME_MODULATION_DEPTH:2;
		 
		uint32_t TEMPORAL_LEVEL:1;
		uint32_t FRC25:2;
		uint32_t FRC50:2;
		uint32_t FRC75:2;
	} flags;

	uint32_t r_seed_value;
	uint32_t b_seed_value;
	uint32_t g_seed_value;
	enum dc_pixel_encoding pixel_encoding;
};

enum wide_gamut_regamma_mode {
	 
	WIDE_GAMUT_REGAMMA_MODE_GRAPHICS_BYPASS,
	 
	WIDE_GAMUT_REGAMMA_MODE_GRAPHICS_SRGB24,
	 
	WIDE_GAMUT_REGAMMA_MODE_GRAPHICS_XYYCC22,
	 
	WIDE_GAMUT_REGAMMA_MODE_GRAPHICS_MATRIX_A,
	 
	WIDE_GAMUT_REGAMMA_MODE_GRAPHICS_MATRIX_B,
	 
	WIDE_GAMUT_REGAMMA_MODE_OVL_BYPASS,
	 
	WIDE_GAMUT_REGAMMA_MODE_OVL_SRGB24,
	 
	WIDE_GAMUT_REGAMMA_MODE_OVL_XYYCC22,
	 
	WIDE_GAMUT_REGAMMA_MODE_OVL_MATRIX_A,
	 
	WIDE_GAMUT_REGAMMA_MODE_OVL_MATRIX_B
};

struct gamma_pixel {
	struct fixed31_32 r;
	struct fixed31_32 g;
	struct fixed31_32 b;
};

enum channel_name {
	CHANNEL_NAME_RED,
	CHANNEL_NAME_GREEN,
	CHANNEL_NAME_BLUE
};

struct custom_float_format {
	uint32_t mantissa_bits;
	uint32_t exponenta_bits;
	bool sign;
};

struct custom_float_value {
	uint32_t mantissa;
	uint32_t exponenta;
	uint32_t value;
	bool negative;
};

struct hw_x_point {
	uint32_t custom_float_x;
	struct fixed31_32 x;
	struct fixed31_32 regamma_y_red;
	struct fixed31_32 regamma_y_green;
	struct fixed31_32 regamma_y_blue;

};

struct pwl_float_data_ex {
	struct fixed31_32 r;
	struct fixed31_32 g;
	struct fixed31_32 b;
	struct fixed31_32 delta_r;
	struct fixed31_32 delta_g;
	struct fixed31_32 delta_b;
};

enum hw_point_position {
	 
	HW_POINT_POSITION_MIDDLE,
	 
	HW_POINT_POSITION_LEFT,
	 
	HW_POINT_POSITION_RIGHT
};

struct gamma_point {
	int32_t left_index;
	int32_t right_index;
	enum hw_point_position pos;
	struct fixed31_32 coeff;
};

struct pixel_gamma_point {
	struct gamma_point r;
	struct gamma_point g;
	struct gamma_point b;
};

struct gamma_coefficients {
	struct fixed31_32 a0[3];
	struct fixed31_32 a1[3];
	struct fixed31_32 a2[3];
	struct fixed31_32 a3[3];
	struct fixed31_32 user_gamma[3];
	struct fixed31_32 user_contrast;
	struct fixed31_32 user_brightness;
};

struct pwl_float_data {
	struct fixed31_32 r;
	struct fixed31_32 g;
	struct fixed31_32 b;
};

struct mpc_tree_cfg {
	int num_pipes;
	int dpp[MAX_PIPES];
	int mpcc[MAX_PIPES];
};

struct output_pixel_processor {
	struct dc_context *ctx;
	uint32_t inst;
	struct pwl_params regamma_params;
	struct mpc_tree mpc_tree_params;
	bool mpcc_disconnect_pending[MAX_PIPES];
	const struct opp_funcs *funcs;
	uint32_t dyn_expansion;
};

enum fmt_stereo_action {
	FMT_STEREO_ACTION_ENABLE = 0,
	FMT_STEREO_ACTION_DISABLE,
	FMT_STEREO_ACTION_UPDATE_POLARITY
};

struct opp_grph_csc_adjustment {
	
	enum dc_color_space c_space;
	enum dc_color_depth color_depth;  
	enum graphics_csc_adjust_type   csc_adjust_type;
	int32_t adjust_divider;
	int32_t grph_cont;
	int32_t grph_sat;
	int32_t grph_bright;
	int32_t grph_hue;
};

 

struct hw_adjustment_range {
	int32_t hw_default;
	int32_t min;
	int32_t max;
	int32_t step;
	uint32_t divider;  
};

enum ovl_csc_adjust_item {
	OVERLAY_BRIGHTNESS = 0,
	OVERLAY_GAMMA,
	OVERLAY_CONTRAST,
	OVERLAY_SATURATION,
	OVERLAY_HUE,
	OVERLAY_ALPHA,
	OVERLAY_ALPHA_PER_PIX,
	OVERLAY_COLOR_TEMPERATURE
};

enum oppbuf_display_segmentation {
	OPPBUF_DISPLAY_SEGMENTATION_1_SEGMENT = 0,
	OPPBUF_DISPLAY_SEGMENTATION_2_SEGMENT = 1,
	OPPBUF_DISPLAY_SEGMENTATION_4_SEGMENT = 2,
	OPPBUF_DISPLAY_SEGMENTATION_4_SEGMENT_SPLIT_LEFT = 3,
	OPPBUF_DISPLAY_SEGMENTATION_4_SEGMENT_SPLIT_RIGHT = 4
};

struct oppbuf_params {
	uint32_t active_width;
	enum oppbuf_display_segmentation mso_segmentation;
	uint32_t mso_overlap_pixel_num;
	uint32_t pixel_repetition;
	uint32_t num_segment_padded_pixels;
};

struct opp_funcs {


	 

	void (*opp_program_fmt)(
			struct output_pixel_processor *opp,
			struct bit_depth_reduction_params *fmt_bit_depth,
			struct clamping_and_pixel_encoding_params *clamping);

	void (*opp_set_dyn_expansion)(
		struct output_pixel_processor *opp,
		enum dc_color_space color_sp,
		enum dc_color_depth color_dpth,
		enum signal_type signal);

	void (*opp_program_bit_depth_reduction)(
		struct output_pixel_processor *opp,
		const struct bit_depth_reduction_params *params);

	 
	void (*opp_get_underlay_adjustment_range)(
			struct output_pixel_processor *opp,
			enum ovl_csc_adjust_item overlay_adjust_item,
			struct hw_adjustment_range *range);

	void (*opp_destroy)(struct output_pixel_processor **opp);

	void (*opp_program_stereo)(
		struct output_pixel_processor *opp,
		bool enable,
		const struct dc_crtc_timing *timing);

	void (*opp_pipe_clock_control)(
			struct output_pixel_processor *opp,
			bool enable);

	void (*opp_set_disp_pattern_generator)(
			struct output_pixel_processor *opp,
			enum controller_dp_test_pattern test_pattern,
			enum controller_dp_color_space color_space,
			enum dc_color_depth color_depth,
			const struct tg_color *solid_color,
			int width,
			int height,
			int offset);

	void (*opp_program_dpg_dimensions)(
				struct output_pixel_processor *opp,
				int width,
				int height);

	bool (*dpg_is_blanked)(
			struct output_pixel_processor *opp);

	void (*opp_dpg_set_blank_color)(
			struct output_pixel_processor *opp,
			const struct tg_color *color);

	void (*opp_program_left_edge_extra_pixel)(
			struct output_pixel_processor *opp,
			bool count);

};

#endif
