#ifndef __LINK_VALIDATION_H__
#define __LINK_VALIDATION_H__
#include "link.h"
enum dc_status link_validate_mode_timing(
		const struct dc_stream_state *stream,
		struct dc_link *link,
		const struct dc_crtc_timing *timing);
bool link_validate_dpia_bandwidth(
		const struct dc_stream_state *stream,
		const unsigned int num_streams);
uint32_t dp_link_bandwidth_kbps(
	const struct dc_link *link,
	const struct dc_link_settings *link_settings);
#endif  
