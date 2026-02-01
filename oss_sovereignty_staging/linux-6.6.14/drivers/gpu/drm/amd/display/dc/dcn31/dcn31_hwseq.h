 

#ifndef __DC_HWSS_DCN31_H__
#define __DC_HWSS_DCN31_H__

#include "hw_sequencer_private.h"

struct dc;

void dcn31_init_hw(struct dc *dc);

void dcn31_dsc_pg_control(
		struct dce_hwseq *hws,
		unsigned int dsc_inst,
		bool power_on);

void dcn31_enable_power_gating_plane(
	struct dce_hwseq *hws,
	bool enable);

void dcn31_update_info_frame(struct pipe_ctx *pipe_ctx);

void dcn31_z10_restore(const struct dc *dc);
void dcn31_z10_save_init(struct dc *dc);

void dcn31_hubp_pg_control(struct dce_hwseq *hws, unsigned int hubp_inst, bool power_on);
int dcn31_init_sys_ctx(struct dce_hwseq *hws, struct dc *dc, struct dc_phy_addr_space_config *pa_config);
void dcn31_reset_hw_ctx_wrap(
		struct dc *dc,
		struct dc_state *context);
bool dcn31_is_abm_supported(struct dc *dc,
		struct dc_state *context, struct dc_stream_state *stream);
void dcn31_init_pipes(struct dc *dc, struct dc_state *context);
void dcn31_setup_hpo_hw_control(const struct dce_hwseq *hws, bool enable);

#endif  
