 
#ifndef __LINK_HWSS_DIO_H__
#define __LINK_HWSS_DIO_H__

#include "link_hwss.h"
#include "link.h"

const struct link_hwss *get_dio_link_hwss(void);
bool can_use_dio_link_hwss(const struct dc_link *link,
		const struct link_resource *link_res);
void set_dio_throttled_vcp_size(struct pipe_ctx *pipe_ctx,
		struct fixed31_32 throttled_vcp_size);
void setup_dio_stream_encoder(struct pipe_ctx *pipe_ctx);
void reset_dio_stream_encoder(struct pipe_ctx *pipe_ctx);
void setup_dio_stream_attribute(struct pipe_ctx *pipe_ctx);
void enable_dio_dp_link_output(struct dc_link *link,
		const struct link_resource *link_res,
		enum signal_type signal,
		enum clock_source_id clock_source,
		const struct dc_link_settings *link_settings);
void disable_dio_link_output(struct dc_link *link,
		const struct link_resource *link_res,
		enum signal_type signal);
void set_dio_dp_link_test_pattern(struct dc_link *link,
		const struct link_resource *link_res,
		struct encoder_set_dp_phy_pattern_param *tp_params);
void set_dio_dp_lane_settings(struct dc_link *link,
		const struct link_resource *link_res,
		const struct dc_link_settings *link_settings,
		const struct dc_lane_settings lane_settings[LANE_COUNT_DP_MAX]);
void setup_dio_audio_output(struct pipe_ctx *pipe_ctx,
		struct audio_output *audio_output, uint32_t audio_inst);
void enable_dio_audio_packet(struct pipe_ctx *pipe_ctx);
void disable_dio_audio_packet(struct pipe_ctx *pipe_ctx);
void update_dio_stream_allocation_table(struct dc_link *link,
		const struct link_resource *link_res,
		const struct link_mst_stream_allocation_table *table);

#endif  
