#ifndef __DAL_IPP_H__
#define __DAL_IPP_H__
#include "hw_shared.h"
#include "dc_hw_types.h"
#define MAXTRIX_COEFFICIENTS_NUMBER 12
#define MAXTRIX_COEFFICIENTS_WRAP_NUMBER (MAXTRIX_COEFFICIENTS_NUMBER + 4)
#define MAX_OVL_MATRIX_COUNT 12
struct input_pixel_processor {
	struct  dc_context *ctx;
	unsigned int inst;
	const struct ipp_funcs *funcs;
};
enum ipp_prescale_mode {
	IPP_PRESCALE_MODE_BYPASS,
	IPP_PRESCALE_MODE_FIXED_SIGNED,
	IPP_PRESCALE_MODE_FLOAT_SIGNED,
	IPP_PRESCALE_MODE_FIXED_UNSIGNED,
	IPP_PRESCALE_MODE_FLOAT_UNSIGNED
};
struct ipp_prescale_params {
	enum ipp_prescale_mode mode;
	uint16_t bias;
	uint16_t scale;
};
enum ovl_color_space {
	OVL_COLOR_SPACE_UNKNOWN = 0,
	OVL_COLOR_SPACE_RGB,
	OVL_COLOR_SPACE_YUV601,
	OVL_COLOR_SPACE_YUV709
};
struct ipp_funcs {
	void (*ipp_cursor_set_position)(
		struct input_pixel_processor *ipp,
		const struct dc_cursor_position *position,
		const struct dc_cursor_mi_param *param);
	void (*ipp_cursor_set_attributes)(
		struct input_pixel_processor *ipp,
		const struct dc_cursor_attributes *attributes);
	void (*ipp_full_bypass)(
			struct input_pixel_processor *ipp);
	void (*ipp_setup)(
		struct input_pixel_processor *ipp,
		enum surface_pixel_format format,
		enum expansion_mode mode,
		struct dc_csc_transform input_csc_color_matrix,
		enum dc_color_space input_color_space);
	void (*ipp_program_prescale)(
			struct input_pixel_processor *ipp,
			struct ipp_prescale_params *params);
	void (*ipp_program_input_lut)(
			struct input_pixel_processor *ipp,
			const struct dc_gamma *gamma);
	void (*ipp_set_degamma)(
		struct input_pixel_processor *ipp,
		enum ipp_degamma_mode mode);
	void (*ipp_program_degamma_pwl)(
		struct input_pixel_processor *ipp,
		const struct pwl_params *params);
	void (*ipp_destroy)(struct input_pixel_processor **ipp);
};
#endif  
