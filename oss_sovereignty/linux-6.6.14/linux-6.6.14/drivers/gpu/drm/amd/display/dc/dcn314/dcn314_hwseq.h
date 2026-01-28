#ifndef __DC_HWSS_DCN314_H__
#define __DC_HWSS_DCN314_H__
#include "hw_sequencer_private.h"
struct dc;
void dcn314_update_odm(struct dc *dc, struct dc_state *context, struct pipe_ctx *pipe_ctx);
void dcn314_dsc_pg_control(struct dce_hwseq *hws, unsigned int dsc_inst, bool power_on);
void dcn314_enable_power_gating_plane(struct dce_hwseq *hws, bool enable);
unsigned int dcn314_calculate_dccg_k1_k2_values(struct pipe_ctx *pipe_ctx, unsigned int *k1_div, unsigned int *k2_div);
void dcn314_set_pixels_per_cycle(struct pipe_ctx *pipe_ctx);
void dcn314_resync_fifo_dccg_dio(struct dce_hwseq *hws, struct dc *dc, struct dc_state *context);
void dcn314_dpp_root_clock_control(struct dce_hwseq *hws, unsigned int dpp_inst, bool clock_on);
void dcn314_disable_link_output(struct dc_link *link, const struct link_resource *link_res, enum signal_type signal);
#endif  
