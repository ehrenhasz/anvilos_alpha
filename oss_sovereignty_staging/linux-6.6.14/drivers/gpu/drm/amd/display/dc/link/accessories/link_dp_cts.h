 
#ifndef __LINK_DP_CTS_H__
#define __LINK_DP_CTS_H__
#include "link.h"
void dp_handle_automated_test(struct dc_link *link);
bool dp_set_test_pattern(
		struct dc_link *link,
		enum dp_test_pattern test_pattern,
		enum dp_test_pattern_color_space test_pattern_color_space,
		const struct link_training_settings *p_link_settings,
		const unsigned char *p_custom_pattern,
		unsigned int cust_pattern_size);
void dp_set_preferred_link_settings(struct dc *dc,
		struct dc_link_settings *link_setting,
		struct dc_link *link);
void dp_set_preferred_training_settings(struct dc *dc,
		struct dc_link_settings *link_setting,
		struct dc_link_training_overrides *lt_overrides,
		struct dc_link *link,
		bool skip_immediate_retrain);
#endif  
