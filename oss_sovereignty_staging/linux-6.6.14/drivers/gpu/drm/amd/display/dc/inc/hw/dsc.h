 
#ifndef __DAL_DSC_H__
#define __DAL_DSC_H__

#include "dc_dsc.h"
#include "dc_hw_types.h"
#include "dc_types.h"
 


 
struct dsc_config {
	uint32_t pic_width;
	uint32_t pic_height;
	enum dc_pixel_encoding pixel_encoding;
	enum dc_color_depth color_depth;   
	bool is_odm;
	struct dc_dsc_config dc_dsc_cfg;
};


 
struct dsc_optc_config {
	uint32_t slice_width;  
	uint32_t bytes_per_pixel;  
	bool is_pixel_format_444;  
};


struct dcn_dsc_state {
	uint32_t dsc_clock_en;
	uint32_t dsc_slice_width;
	uint32_t dsc_bits_per_pixel;
	uint32_t dsc_slice_height;
	uint32_t dsc_pic_width;
	uint32_t dsc_pic_height;
	uint32_t dsc_slice_bpg_offset;
	uint32_t dsc_chunk_size;
	uint32_t dsc_fw_en;
	uint32_t dsc_opp_source;
};


 
union dsc_enc_slice_caps {
	struct {
		uint8_t NUM_SLICES_1 : 1;
		uint8_t NUM_SLICES_2 : 1;
		uint8_t NUM_SLICES_3 : 1;  
		uint8_t NUM_SLICES_4 : 1;
		uint8_t NUM_SLICES_8 : 1;
	} bits;
	uint8_t raw;
};

struct dsc_enc_caps {
	uint8_t dsc_version;
	union dsc_enc_slice_caps slice_caps;
	int32_t lb_bit_depth;
	bool is_block_pred_supported;
	union dsc_color_formats color_formats;
	union dsc_color_depth color_depth;
	int32_t max_total_throughput_mps;  
	int32_t max_slice_width;
	uint32_t bpp_increment_div;  
	uint32_t edp_sink_max_bits_per_pixel;
	bool is_dp;
};

struct dsc_funcs {
	void (*dsc_get_enc_caps)(struct dsc_enc_caps *dsc_enc_caps, int pixel_clock_100Hz);
	void (*dsc_read_state)(struct display_stream_compressor *dsc, struct dcn_dsc_state *s);
	bool (*dsc_validate_stream)(struct display_stream_compressor *dsc, const struct dsc_config *dsc_cfg);
	void (*dsc_set_config)(struct display_stream_compressor *dsc, const struct dsc_config *dsc_cfg,
			struct dsc_optc_config *dsc_optc_cfg);
	bool (*dsc_get_packed_pps)(struct display_stream_compressor *dsc, const struct dsc_config *dsc_cfg,
			uint8_t *dsc_packed_pps);
	void (*dsc_enable)(struct display_stream_compressor *dsc, int opp_pipe);
	void (*dsc_disable)(struct display_stream_compressor *dsc);
	void (*dsc_disconnect)(struct display_stream_compressor *dsc);
};

#endif
