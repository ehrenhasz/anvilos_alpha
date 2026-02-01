 

#ifndef __DC_LINK_DP_PHY_H__
#define __DC_LINK_DP_PHY_H__

#include "link.h"
void dp_enable_link_phy(
	struct dc_link *link,
	const struct link_resource *link_res,
	enum signal_type signal,
	enum clock_source_id clock_source,
	const struct dc_link_settings *link_settings);

void dp_disable_link_phy(struct dc_link *link,
		const struct link_resource *link_res,
		enum signal_type signal);

void dp_set_hw_lane_settings(
		struct dc_link *link,
		const struct link_resource *link_res,
		const struct link_training_settings *link_settings,
		uint32_t offset);

void dp_set_drive_settings(
	struct dc_link *link,
	const struct link_resource *link_res,
	struct link_training_settings *lt_settings);

enum dc_status dp_set_fec_ready(struct dc_link *link,
		const struct link_resource *link_res, bool ready);

void dp_set_fec_enable(struct dc_link *link, bool enable);

void dpcd_write_rx_power_ctrl(struct dc_link *link, bool on);

#endif  
