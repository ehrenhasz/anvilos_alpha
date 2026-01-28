#ifndef __DC_LINK_DPMS_H__
#define __DC_LINK_DPMS_H__
#include "link.h"
void link_set_dpms_on(
		struct dc_state *state,
		struct pipe_ctx *pipe_ctx);
void link_set_dpms_off(struct pipe_ctx *pipe_ctx);
void link_resume(struct dc_link *link);
void link_blank_all_dp_displays(struct dc *dc);
void link_blank_all_edp_displays(struct dc *dc);
void link_blank_dp_stream(struct dc_link *link, bool hw_init);
void link_set_all_streams_dpms_off_for_link(struct dc_link *link);
void link_get_master_pipes_with_dpms_on(const struct dc_link *link,
		struct dc_state *state,
		uint8_t *count,
		struct pipe_ctx *pipes[MAX_PIPES]);
enum dc_status link_increase_mst_payload(struct pipe_ctx *pipe_ctx, uint32_t req_pbn);
enum dc_status link_reduce_mst_payload(struct pipe_ctx *pipe_ctx, uint32_t req_pbn);
bool link_set_dsc_pps_packet(struct pipe_ctx *pipe_ctx,
		bool enable, bool immediate_update);
struct fixed31_32 link_calculate_sst_avg_time_slots_per_mtp(
		const struct dc_stream_state *stream,
		const struct dc_link *link);
void link_set_dsc_on_stream(struct pipe_ctx *pipe_ctx, bool enable);
bool link_set_dsc_enable(struct pipe_ctx *pipe_ctx, bool enable);
bool link_update_dsc_config(struct pipe_ctx *pipe_ctx);
#endif  
