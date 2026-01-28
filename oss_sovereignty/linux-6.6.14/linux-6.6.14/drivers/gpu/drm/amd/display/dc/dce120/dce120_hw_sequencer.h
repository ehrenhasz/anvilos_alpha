#ifndef __DC_HWSS_DCE120_H__
#define __DC_HWSS_DCE120_H__
#include "core_types.h"
#include "hw_sequencer_private.h"
struct dc;
bool dce121_xgmi_enabled(struct dce_hwseq *hws);
void dce120_hw_sequencer_construct(struct dc *dc);
#endif  
