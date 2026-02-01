 

#ifndef __DC_HWSS_DCN21_H__
#define __DC_HWSS_DCN21_H__

#include "hw_sequencer_private.h"

struct dc;

int dcn21_init_sys_ctx(struct dce_hwseq *hws,
		struct dc *dc,
		struct dc_phy_addr_space_config *pa_config);

bool dcn21_s0i3_golden_init_wa(struct dc *dc);

void dcn21_exit_optimized_pwr_state(
		const struct dc *dc,
		struct dc_state *context);

void dcn21_optimize_pwr_state(
		const struct dc *dc,
		struct dc_state *context);

void dcn21_PLAT_58856_wa(struct dc_state *context,
		struct pipe_ctx *pipe_ctx);

void dcn21_set_pipe(struct pipe_ctx *pipe_ctx);
void dcn21_set_abm_immediate_disable(struct pipe_ctx *pipe_ctx);
bool dcn21_set_backlight_level(struct pipe_ctx *pipe_ctx,
		uint32_t backlight_pwm_u16_16,
		uint32_t frame_ramp);
bool dcn21_is_abm_supported(struct dc *dc,
		struct dc_state *context, struct dc_stream_state *stream);

#endif  
