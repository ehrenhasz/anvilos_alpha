#ifndef __DC_LINK_DP_FIXED_VS_PE_RETIMER_H__
#define __DC_LINK_DP_FIXED_VS_PE_RETIMER_H__
#include "link_dp_training.h"
enum link_training_result dp_perform_fixed_vs_pe_training_sequence_legacy(
	struct dc_link *link,
	const struct link_resource *link_res,
	struct link_training_settings *lt_settings);
enum link_training_result dp_perform_fixed_vs_pe_training_sequence(
	struct dc_link *link,
	const struct link_resource *link_res,
	struct link_training_settings *lt_settings);
void dp_fixed_vs_pe_set_retimer_lane_settings(
	struct dc_link *link,
	const union dpcd_training_lane dpcd_lane_adjust[LANE_COUNT_DP_MAX],
	uint8_t lane_count);
void dp_fixed_vs_pe_read_lane_adjust(
	struct dc_link *link,
	union dpcd_training_lane dpcd_lane_adjust[LANE_COUNT_DP_MAX]);
#endif  
