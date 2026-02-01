 

#ifndef _DCN_CALC_AUTO_H_
#define _DCN_CALC_AUTO_H_

#include "dc.h"
#include "dcn_calcs.h"

void scaler_settings_calculation(struct dcn_bw_internal_vars *v);
void mode_support_and_system_configuration(struct dcn_bw_internal_vars *v);
void display_pipe_configuration(struct dcn_bw_internal_vars *v);
void dispclkdppclkdcfclk_deep_sleep_prefetch_parameters_watermarks_and_performance_calculation(
		struct dcn_bw_internal_vars *v);

#endif  
