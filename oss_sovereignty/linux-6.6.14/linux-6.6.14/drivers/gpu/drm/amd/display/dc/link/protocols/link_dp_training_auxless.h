#ifndef __DC_LINK_DP_TRAINING_AUXLESS_H__
#define __DC_LINK_DP_TRAINING_AUXLESS_H__
#include "link_dp_training.h"
bool dp_perform_link_training_skip_aux(
	struct dc_link *link,
	const struct link_resource *link_res,
	const struct dc_link_settings *link_setting);
#endif  
