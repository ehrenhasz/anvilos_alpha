#ifndef __DC_HWSS_DCN201_H__
#define __DC_HWSS_DCN201_H__
#include "hw_sequencer_private.h"
void dcn201_set_dmdata_attributes(struct pipe_ctx *pipe_ctx);
void dcn201_init_hw(struct dc *dc);
void dcn201_unblank_stream(struct pipe_ctx *pipe_ctx,
		struct dc_link_settings *link_settings);
void dcn201_update_plane_addr(const struct dc *dc, struct pipe_ctx *pipe_ctx);
void dcn201_plane_atomic_disconnect(struct dc *dc, struct pipe_ctx *pipe_ctx);
void dcn201_update_mpcc(struct dc *dc, struct pipe_ctx *pipe_ctx);
void dcn201_set_cursor_attribute(struct pipe_ctx *pipe_ctx);
void dcn201_pipe_control_lock(
	struct dc *dc,
	struct pipe_ctx *pipe,
	bool lock);
void dcn201_init_blank(
		struct dc *dc,
		struct timing_generator *tg);
#endif  
