 


#ifndef __DC_LINK_DP_TRAINING_8B_10B_H__
#define __DC_LINK_DP_TRAINING_8B_10B_H__
#include "link_dp_training.h"

 
#define LINK_TRAINING_MAX_CR_RETRY 100
#define LINK_TRAINING_MAX_RETRY_COUNT 5

enum link_training_result dp_perform_8b_10b_link_training(
		struct dc_link *link,
		const struct link_resource *link_res,
		struct link_training_settings *lt_settings);

enum link_training_result perform_8b_10b_clock_recovery_sequence(
	struct dc_link *link,
	const struct link_resource *link_res,
	struct link_training_settings *lt_settings,
	uint32_t offset);

enum link_training_result perform_8b_10b_channel_equalization_sequence(
	struct dc_link *link,
	const struct link_resource *link_res,
	struct link_training_settings *lt_settings,
	uint32_t offset);

enum lttpr_mode dp_decide_8b_10b_lttpr_mode(struct dc_link *link);

void decide_8b_10b_training_settings(
	 struct dc_link *link,
	const struct dc_link_settings *link_setting,
	struct link_training_settings *lt_settings);

#endif  
