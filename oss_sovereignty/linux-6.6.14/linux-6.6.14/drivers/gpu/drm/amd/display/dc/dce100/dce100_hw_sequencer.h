#ifndef __DC_HWSS_DCE100_H__
#define __DC_HWSS_DCE100_H__
#include "core_types.h"
#include "hw_sequencer_private.h"
struct dc;
struct dc_state;
void dce100_hw_sequencer_construct(struct dc *dc);
void dce100_prepare_bandwidth(
		struct dc *dc,
		struct dc_state *context);
void dce100_optimize_bandwidth(
		struct dc *dc,
		struct dc_state *context);
bool dce100_enable_display_power_gating(struct dc *dc, uint8_t controller_id,
					struct dc_bios *dcb,
					enum pipe_gating_control power_gating);
#endif  
