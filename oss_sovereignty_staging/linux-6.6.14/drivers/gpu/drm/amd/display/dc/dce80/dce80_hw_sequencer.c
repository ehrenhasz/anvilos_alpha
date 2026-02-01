 

#include "dm_services.h"
#include "dc.h"
#include "core_types.h"
#include "dce80_hw_sequencer.h"

#include "dce/dce_hwseq.h"
#include "dce110/dce110_hw_sequencer.h"
#include "dce100/dce100_hw_sequencer.h"

 
#include "dce/dce_8_0_d.h"
#include "dce/dce_8_0_sh_mask.h"

 

 

void dce80_hw_sequencer_construct(struct dc *dc)
{
	dce110_hw_sequencer_construct(dc);

	dc->hwseq->funcs.enable_display_power_gating = dce100_enable_display_power_gating;
	dc->hwss.pipe_control_lock = dce_pipe_control_lock;
	dc->hwss.prepare_bandwidth = dce100_prepare_bandwidth;
	dc->hwss.optimize_bandwidth = dce100_optimize_bandwidth;
}

