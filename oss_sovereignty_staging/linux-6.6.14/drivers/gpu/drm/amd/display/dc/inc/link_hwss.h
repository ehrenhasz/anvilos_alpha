 

#ifndef __DC_LINK_HWSS_H__
#define __DC_LINK_HWSS_H__

 
#include "dc_dp_types.h"
#include "signal_types.h"
#include "grph_object_id.h"
#include "fixed31_32.h"

 
struct dc_link;
struct link_resource;
struct pipe_ctx;
struct encoder_set_dp_phy_pattern_param;
struct link_mst_stream_allocation_table;
struct audio_output;

struct link_hwss_ext {
	 
	void (*set_hblank_min_symbol_width)(struct pipe_ctx *pipe_ctx,
			const struct dc_link_settings *link_settings,
			struct fixed31_32 throttled_vcp_size);
	void (*set_throttled_vcp_size)(struct pipe_ctx *pipe_ctx,
			struct fixed31_32 throttled_vcp_size);
	void (*enable_dp_link_output)(struct dc_link *link,
			const struct link_resource *link_res,
			enum signal_type signal,
			enum clock_source_id clock_source,
			const struct dc_link_settings *link_settings);
	void (*set_dp_link_test_pattern)(struct dc_link *link,
			const struct link_resource *link_res,
			struct encoder_set_dp_phy_pattern_param *tp_params);
	void (*set_dp_lane_settings)(struct dc_link *link,
			const struct link_resource *link_res,
			const struct dc_link_settings *link_settings,
			const struct dc_lane_settings lane_settings[LANE_COUNT_DP_MAX]);
	void (*update_stream_allocation_table)(struct dc_link *link,
			const struct link_resource *link_res,
			const struct link_mst_stream_allocation_table *table);
};

struct link_hwss {
	struct link_hwss_ext ext;

	 
	void (*setup_stream_encoder)(struct pipe_ctx *pipe_ctx);
	void (*reset_stream_encoder)(struct pipe_ctx *pipe_ctx);
	void (*setup_stream_attribute)(struct pipe_ctx *pipe_ctx);
	void (*disable_link_output)(struct dc_link *link,
			const struct link_resource *link_res,
			enum signal_type signal);
	void (*setup_audio_output)(struct pipe_ctx *pipe_ctx,
			struct audio_output *audio_output, uint32_t audio_inst);
	void (*enable_audio_packet)(struct pipe_ctx *pipe_ctx);
	void (*disable_audio_packet)(struct pipe_ctx *pipe_ctx);
};
#endif  

