#ifndef __DC_CLOCK_SOURCE_H__
#define __DC_CLOCK_SOURCE_H__
#include "dc_types.h"
#include "include/grph_object_id.h"
#include "include/bios_parser_types.h"
struct clock_source;
struct spread_spectrum_data {
	uint32_t percentage;		 
	uint32_t percentage_divider;	 
	uint32_t freq_range_khz;
	uint32_t modulation_freq_hz;
	struct spread_spectrum_flags flags;
};
struct delta_sigma_data {
	uint32_t feedback_amount;
	uint32_t nfrac_amount;
	uint32_t ds_frac_size;
	uint32_t ds_frac_amount;
};
struct pixel_clk_flags {
	uint32_t ENABLE_SS:1;
	uint32_t DISPLAY_BLANKED:1;
	uint32_t PROGRAM_PIXEL_CLOCK:1;
	uint32_t PROGRAM_ID_CLOCK:1;
	uint32_t SUPPORT_YCBCR420:1;
};
struct csdp_ref_clk_ds_params {
	bool hw_dso_n_dp_ref_clk;
	uint32_t avg_dp_ref_clk_khz;
	uint32_t ss_percentage_on_dp_ref_clk;
	uint32_t ss_percentage_divider;
};
struct pixel_clk_params {
	uint32_t requested_pix_clk_100hz;
	uint32_t requested_sym_clk;  
	uint32_t dp_ref_clk;  
	struct graphics_object_id encoder_object_id;
	enum signal_type signal_type;
	enum controller_id controller_id;
	enum dc_color_depth color_depth;
	struct csdp_ref_clk_ds_params de_spread_params;
	enum dc_pixel_encoding pixel_encoding;
	struct pixel_clk_flags flags;
};
struct pll_settings {
	uint32_t actual_pix_clk_100hz;
	uint32_t adjusted_pix_clk_100hz;
	uint32_t calculated_pix_clk_100hz;
	uint32_t vco_freq;
	uint32_t reference_freq;
	uint32_t reference_divider;
	uint32_t feedback_divider;
	uint32_t fract_feedback_divider;
	uint32_t pix_clk_post_divider;
	uint32_t ss_percentage;
	bool use_external_clk;
};
struct calc_pll_clock_source_init_data {
	struct dc_bios *bp;
	uint32_t min_pix_clk_pll_post_divider;
	uint32_t max_pix_clk_pll_post_divider;
	uint32_t min_pll_ref_divider;
	uint32_t max_pll_ref_divider;
	uint32_t min_override_input_pxl_clk_pll_freq_khz;
	uint32_t max_override_input_pxl_clk_pll_freq_khz;
	uint32_t num_fract_fb_divider_decimal_point;
	uint32_t num_fract_fb_divider_decimal_point_precision;
	struct dc_context *ctx;
};
struct calc_pll_clock_source {
	uint32_t ref_freq_khz;
	uint32_t min_pix_clock_pll_post_divider;
	uint32_t max_pix_clock_pll_post_divider;
	uint32_t min_pll_ref_divider;
	uint32_t max_pll_ref_divider;
	uint32_t max_vco_khz;
	uint32_t min_vco_khz;
	uint32_t min_pll_input_freq_khz;
	uint32_t max_pll_input_freq_khz;
	uint32_t fract_fb_divider_decimal_points_num;
	uint32_t fract_fb_divider_factor;
	uint32_t fract_fb_divider_precision;
	uint32_t fract_fb_divider_precision_factor;
	struct dc_context *ctx;
};
struct clock_source_funcs {
	bool (*cs_power_down)(
			struct clock_source *);
	bool (*program_pix_clk)(
			struct clock_source *,
			struct pixel_clk_params *,
			enum dp_link_encoding encoding,
			struct pll_settings *);
	uint32_t (*get_pix_clk_dividers)(
			struct clock_source *,
			struct pixel_clk_params *,
			struct pll_settings *);
	bool (*get_pixel_clk_frequency_100hz)(
			const struct clock_source *clock_source,
			unsigned int inst,
			unsigned int *pixel_clk_khz);
	bool (*override_dp_pix_clk)(
			struct clock_source *clock_source,
			unsigned int inst,
			unsigned int pixel_clk,
			unsigned int ref_clk);
};
struct clock_source {
	const struct clock_source_funcs *funcs;
	struct dc_context *ctx;
	enum clock_source_id id;
	bool dp_clk_src;
};
#endif
