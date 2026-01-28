#ifndef __DC_LINK_DETECTION_H__
#define __DC_LINK_DETECTION_H__
#include "link.h"
bool link_detect(struct dc_link *link, enum dc_detect_reason reason);
bool link_detect_connection_type(struct dc_link *link,
		enum dc_connection_type *type);
struct dc_sink *link_add_remote_sink(
		struct dc_link *link,
		const uint8_t *edid,
		int len,
		struct dc_sink_init_data *init_data);
void link_remove_remote_sink(struct dc_link *link, struct dc_sink *sink);
bool link_reset_cur_dp_mst_topology(struct dc_link *link);
const struct dc_link_status *link_get_status(const struct dc_link *link);
bool link_is_hdcp14(struct dc_link *link, enum signal_type signal);
bool link_is_hdcp22(struct dc_link *link, enum signal_type signal);
void link_clear_dprx_states(struct dc_link *link);
#endif  
