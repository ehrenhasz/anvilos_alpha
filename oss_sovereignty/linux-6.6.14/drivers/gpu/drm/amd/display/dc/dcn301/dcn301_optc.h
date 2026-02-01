 

#ifndef __DC_OPTC_DCN301_H__
#define __DC_OPTC_DCN301_H__

#include "dcn20/dcn20_optc.h"
#include "dcn30/dcn30_optc.h"

void dcn301_timing_generator_init(struct optc *optc1);
void optc301_setup_manual_trigger(struct timing_generator *optc);
void optc301_set_drr(struct timing_generator *optc, const struct drr_params *params);

#endif  
