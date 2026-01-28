#ifndef DC_DSC_H_
#define DC_DSC_H_
#define DP_DSC_BRANCH_OVERALL_THROUGHPUT_0  0x0a0    
#define DP_DSC_BRANCH_OVERALL_THROUGHPUT_1  0x0a1
#define DP_DSC_BRANCH_MAX_LINE_WIDTH        0x0a2
#include "dc_types.h"
struct dc_dsc_bw_range {
	uint32_t min_kbps;  
	uint32_t min_target_bpp_x16;
	uint32_t max_kbps;  
	uint32_t max_target_bpp_x16;
	uint32_t stream_kbps;  
};
struct display_stream_compressor {
	const struct dsc_funcs *funcs;
	struct dc_context *ctx;
	int inst;
};
struct dc_dsc_policy {
	bool use_min_slices_h;
	int max_slices_h;  
	int min_slice_height;  
	uint32_t max_target_bpp;
	uint32_t min_target_bpp;
	bool enable_dsc_when_not_needed;
};
struct dc_dsc_config_options {
	uint32_t dsc_min_slice_height_override;
	uint32_t max_target_bpp_limit_override_x16;
	uint32_t slice_height_granularity;
	uint32_t dsc_force_odm_hslice_override;
};
bool dc_dsc_parse_dsc_dpcd(const struct dc *dc,
		const uint8_t *dpcd_dsc_basic_data,
		const uint8_t *dpcd_dsc_ext_data,
		struct dsc_dec_dpcd_caps *dsc_sink_caps);
bool dc_dsc_compute_bandwidth_range(
		const struct display_stream_compressor *dsc,
		uint32_t dsc_min_slice_height_override,
		uint32_t min_bpp_x16,
		uint32_t max_bpp_x16,
		const struct dsc_dec_dpcd_caps *dsc_sink_caps,
		const struct dc_crtc_timing *timing,
		const enum dc_link_encoding_format link_encoding,
		struct dc_dsc_bw_range *range);
bool dc_dsc_compute_config(
		const struct display_stream_compressor *dsc,
		const struct dsc_dec_dpcd_caps *dsc_sink_caps,
		const struct dc_dsc_config_options *options,
		uint32_t target_bandwidth_kbps,
		const struct dc_crtc_timing *timing,
		const enum dc_link_encoding_format link_encoding,
		struct dc_dsc_config *dsc_cfg);
uint32_t dc_dsc_stream_bandwidth_in_kbps(const struct dc_crtc_timing *timing,
		uint32_t bpp_x16, uint32_t num_slices_h, bool is_dp);
uint32_t dc_dsc_stream_bandwidth_overhead_in_kbps(
		const struct dc_crtc_timing *timing,
		const int num_slices_h,
		const bool is_dp);
void dc_dsc_get_policy_for_timing(const struct dc_crtc_timing *timing,
		uint32_t max_target_bpp_limit_override_x16,
		struct dc_dsc_policy *policy);
void dc_dsc_policy_set_max_target_bpp_limit(uint32_t limit);
void dc_dsc_policy_set_enable_dsc_when_not_needed(bool enable);
void dc_dsc_policy_set_disable_dsc_stream_overhead(bool disable);
void dc_dsc_get_default_config_option(const struct dc *dc, struct dc_dsc_config_options *options);
#endif
