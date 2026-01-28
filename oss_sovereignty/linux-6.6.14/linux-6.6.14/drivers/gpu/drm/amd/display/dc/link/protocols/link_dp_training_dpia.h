#ifndef __DC_LINK_DP_TRAINING_DPIA_H__
#define __DC_LINK_DP_TRAINING_DPIA_H__
#include "link_dp_training.h"
enum link_training_result dpia_perform_link_training(
	struct dc_link *link,
	const struct link_resource *link_res,
	const struct dc_link_settings *link_setting,
	bool skip_video_pattern);
#endif  
