 

#ifndef __DAL_HW_SHARED_H__
#define __DAL_HW_SHARED_H__

#include "os_types.h"
#include "fixed31_32.h"
#include "dc_hw_types.h"

 

#define MAX_AUDIOS 7

 
#define MAX_PIPES 6
#define MAX_DIG_LINK_ENCODERS 7
#define MAX_DWB_PIPES	1
#define MAX_HPO_DP2_ENCODERS	4
#define MAX_HPO_DP2_LINK_ENCODERS	2

struct gamma_curve {
	uint32_t offset;
	uint32_t segments_num;
};

struct curve_points {
	struct fixed31_32 x;
	struct fixed31_32 y;
	struct fixed31_32 offset;
	struct fixed31_32 slope;

	uint32_t custom_float_x;
	uint32_t custom_float_y;
	uint32_t custom_float_offset;
	uint32_t custom_float_slope;
};

struct curve_points3 {
	struct curve_points red;
	struct curve_points green;
	struct curve_points blue;
};

struct pwl_result_data {
	struct fixed31_32 red;
	struct fixed31_32 green;
	struct fixed31_32 blue;

	struct fixed31_32 delta_red;
	struct fixed31_32 delta_green;
	struct fixed31_32 delta_blue;

	uint32_t red_reg;
	uint32_t green_reg;
	uint32_t blue_reg;

	uint32_t delta_red_reg;
	uint32_t delta_green_reg;
	uint32_t delta_blue_reg;
};

struct dc_rgb {
	uint32_t red;
	uint32_t green;
	uint32_t blue;
};

struct tetrahedral_17x17x17 {
	struct dc_rgb lut0[1229];
	struct dc_rgb lut1[1228];
	struct dc_rgb lut2[1228];
	struct dc_rgb lut3[1228];
};
struct tetrahedral_9x9x9 {
	struct dc_rgb lut0[183];
	struct dc_rgb lut1[182];
	struct dc_rgb lut2[182];
	struct dc_rgb lut3[182];
};

struct tetrahedral_params {
	union {
		struct tetrahedral_17x17x17 tetrahedral_17;
		struct tetrahedral_9x9x9 tetrahedral_9;
	};
	bool use_tetrahedral_9;
	bool use_12bits;

};

 
struct pwl_params {
	struct gamma_curve arr_curve_points[34];
	union {
		struct curve_points arr_points[2];
		struct curve_points3 corner_points[2];
	};
	struct pwl_result_data rgb_resulted[256 + 3];
	uint32_t hw_points_num;
};

 

enum lb_pixel_depth {
	 
	LB_PIXEL_DEPTH_18BPP = 1,
	LB_PIXEL_DEPTH_24BPP = 2,
	LB_PIXEL_DEPTH_30BPP = 4,
	LB_PIXEL_DEPTH_36BPP = 8
};

enum graphics_csc_adjust_type {
	GRAPHICS_CSC_ADJUST_TYPE_BYPASS = 0,
	GRAPHICS_CSC_ADJUST_TYPE_HW,  
	GRAPHICS_CSC_ADJUST_TYPE_SW   
};

enum ipp_degamma_mode {
	IPP_DEGAMMA_MODE_BYPASS,
	IPP_DEGAMMA_MODE_HW_sRGB,
	IPP_DEGAMMA_MODE_HW_xvYCC,
	IPP_DEGAMMA_MODE_USER_PWL
};

enum gamcor_mode {
	GAMCOR_MODE_BYPASS,
	GAMCOR_MODE_RESERVED_1,
	GAMCOR_MODE_USER_PWL,
	GAMCOR_MODE_RESERVED_3
};

enum ipp_output_format {
	IPP_OUTPUT_FORMAT_12_BIT_FIX,
	IPP_OUTPUT_FORMAT_16_BIT_BYPASS,
	IPP_OUTPUT_FORMAT_FLOAT
};

enum expansion_mode {
	EXPANSION_MODE_DYNAMIC,
	EXPANSION_MODE_ZERO
};

struct default_adjustment {
	enum lb_pixel_depth lb_color_depth;
	enum dc_color_space out_color_space;
	enum dc_color_space in_color_space;
	enum dc_color_depth color_depth;
	enum pixel_format surface_pixel_format;
	enum graphics_csc_adjust_type csc_adjust_type;
	bool force_hw_default;
};


struct out_csc_color_matrix {
	enum dc_color_space color_space;
	uint16_t regval[12];
};

enum gamut_remap_select {
	GAMUT_REMAP_BYPASS = 0,
	GAMUT_REMAP_COEFF,
	GAMUT_REMAP_COMA_COEFF,
	GAMUT_REMAP_COMB_COEFF
};

enum opp_regamma {
	OPP_REGAMMA_BYPASS = 0,
	OPP_REGAMMA_SRGB,
	OPP_REGAMMA_XVYCC,
	OPP_REGAMMA_USER
};

enum optc_dsc_mode {
	OPTC_DSC_DISABLED = 0,
	OPTC_DSC_ENABLED_444 = 1,  
	OPTC_DSC_ENABLED_NATIVE_SUBSAMPLED = 2  
};

struct dc_bias_and_scale {
	uint16_t scale_red;
	uint16_t bias_red;
	uint16_t scale_green;
	uint16_t bias_green;
	uint16_t scale_blue;
	uint16_t bias_blue;
};

enum test_pattern_dyn_range {
	TEST_PATTERN_DYN_RANGE_VESA = 0,
	TEST_PATTERN_DYN_RANGE_CEA
};

enum test_pattern_mode {
	TEST_PATTERN_MODE_COLORSQUARES_RGB = 0,
	TEST_PATTERN_MODE_COLORSQUARES_YCBCR601,
	TEST_PATTERN_MODE_COLORSQUARES_YCBCR709,
	TEST_PATTERN_MODE_VERTICALBARS,
	TEST_PATTERN_MODE_HORIZONTALBARS,
	TEST_PATTERN_MODE_SINGLERAMP_RGB,
	TEST_PATTERN_MODE_DUALRAMP_RGB,
	TEST_PATTERN_MODE_XR_BIAS_RGB
};

enum test_pattern_color_format {
	TEST_PATTERN_COLOR_FORMAT_BPC_6 = 0,
	TEST_PATTERN_COLOR_FORMAT_BPC_8,
	TEST_PATTERN_COLOR_FORMAT_BPC_10,
	TEST_PATTERN_COLOR_FORMAT_BPC_12
};

enum controller_dp_test_pattern {
	CONTROLLER_DP_TEST_PATTERN_D102 = 0,
	CONTROLLER_DP_TEST_PATTERN_SYMBOLERROR,
	CONTROLLER_DP_TEST_PATTERN_PRBS7,
	CONTROLLER_DP_TEST_PATTERN_COLORSQUARES,
	CONTROLLER_DP_TEST_PATTERN_VERTICALBARS,
	CONTROLLER_DP_TEST_PATTERN_HORIZONTALBARS,
	CONTROLLER_DP_TEST_PATTERN_COLORRAMP,
	CONTROLLER_DP_TEST_PATTERN_VIDEOMODE,
	CONTROLLER_DP_TEST_PATTERN_RESERVED_8,
	CONTROLLER_DP_TEST_PATTERN_RESERVED_9,
	CONTROLLER_DP_TEST_PATTERN_RESERVED_A,
	CONTROLLER_DP_TEST_PATTERN_COLORSQUARES_CEA,
	CONTROLLER_DP_TEST_PATTERN_SOLID_COLOR
};

enum controller_dp_color_space {
	CONTROLLER_DP_COLOR_SPACE_RGB,
	CONTROLLER_DP_COLOR_SPACE_YCBCR601,
	CONTROLLER_DP_COLOR_SPACE_YCBCR709,
	CONTROLLER_DP_COLOR_SPACE_UDEFINED
};

enum dc_lut_mode {
	LUT_BYPASS,
	LUT_RAM_A,
	LUT_RAM_B
};

 
 

union audio_cea_channels {
	uint8_t all;
	struct audio_cea_channels_bits {
		uint32_t FL:1;
		uint32_t FR:1;
		uint32_t LFE:1;
		uint32_t FC:1;
		uint32_t RL_RC:1;
		uint32_t RR:1;
		uint32_t RC_RLC_FLC:1;
		uint32_t RRC_FRC:1;
	} channels;
};

#endif  
