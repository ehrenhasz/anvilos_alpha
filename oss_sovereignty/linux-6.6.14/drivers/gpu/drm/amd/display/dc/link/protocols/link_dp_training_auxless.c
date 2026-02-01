 

 
#include "link_dp_training_auxless.h"
#include "link_dp_phy.h"
#define DC_LOGGER \
	link->ctx->logger
bool dp_perform_link_training_skip_aux(
	struct dc_link *link,
	const struct link_resource *link_res,
	const struct dc_link_settings *link_setting)
{
	struct link_training_settings lt_settings = {0};

	dp_decide_training_settings(
			link,
			link_setting,
			&lt_settings);
	override_training_settings(
			link,
			&link->preferred_training_settings,
			&lt_settings);

	 

	 
	dp_set_hw_training_pattern(link, link_res, lt_settings.pattern_for_cr, DPRX);

	 
	dp_set_hw_lane_settings(link, link_res, &lt_settings, DPRX);

	 
	dp_wait_for_training_aux_rd_interval(link, lt_settings.cr_pattern_time);

	 

	 
	dp_set_hw_training_pattern(link, link_res, lt_settings.pattern_for_eq, DPRX);

	 
	dp_set_hw_lane_settings(link, link_res, &lt_settings, DPRX);

	 
	dp_wait_for_training_aux_rd_interval(link, lt_settings.eq_pattern_time);

	 

	 
	dp_set_hw_test_pattern(link, link_res, DP_TEST_PATTERN_VIDEO_MODE, NULL, 0);

	dp_log_training_result(link, &lt_settings, LINK_TRAINING_SUCCESS);

	return true;
}
