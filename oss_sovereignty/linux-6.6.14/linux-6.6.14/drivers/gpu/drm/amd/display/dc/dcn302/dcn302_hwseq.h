#ifndef __DC_HWSS_DCN302_H__
#define __DC_HWSS_DCN302_H__
#include "hw_sequencer_private.h"
void dcn302_dpp_pg_control(struct dce_hwseq *hws, unsigned int dpp_inst, bool power_on);
void dcn302_hubp_pg_control(struct dce_hwseq *hws, unsigned int hubp_inst, bool power_on);
void dcn302_dsc_pg_control(struct dce_hwseq *hws, unsigned int dsc_inst, bool power_on);
#endif  
