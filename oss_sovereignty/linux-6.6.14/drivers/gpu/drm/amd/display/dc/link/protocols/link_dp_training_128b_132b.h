 


#ifndef __DC_LINK_DP_TRAINING_128B_132B_H__
#define __DC_LINK_DP_TRAINING_128B_132B_H__
#include "link_dp_training.h"

enum link_training_result dp_perform_128b_132b_link_training(
		struct dc_link *link,
		const struct link_resource *link_res,
		struct link_training_settings *lt_settings);

void decide_128b_132b_training_settings(struct dc_link *link,
		const struct dc_link_settings *link_settings,
		struct link_training_settings *lt_settings);

enum lttpr_mode dp_decide_128b_132b_lttpr_mode(struct dc_link *link);

#endif  
