 

#include "dcn302_hwseq.h"

#include "dcn30/dcn30_init.h"

#include "dc.h"

#include "dcn302_init.h"

void dcn302_hw_sequencer_construct(struct dc *dc)
{
	dcn30_hw_sequencer_construct(dc);

	dc->hwseq->funcs.dpp_pg_control = dcn302_dpp_pg_control;
	dc->hwseq->funcs.hubp_pg_control = dcn302_hubp_pg_control;
	dc->hwseq->funcs.dsc_pg_control = dcn302_dsc_pg_control;
}
