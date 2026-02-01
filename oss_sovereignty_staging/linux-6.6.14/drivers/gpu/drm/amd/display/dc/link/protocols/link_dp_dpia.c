
 

#include "dc.h"
#include "inc/core_status.h"
#include "dpcd_defs.h"

#include "link_dp_dpia.h"
#include "link_hwss.h"
#include "dm_helpers.h"
#include "dmub/inc/dmub_cmd.h"
#include "link_dpcd.h"
#include "link_dp_training.h"
#include "dc_dmub_srv.h"

#define DC_LOGGER \
	link->ctx->logger

 
 
#define DP_TUNNELING_CAPABILITIES_SUPPORT 0xe000d
#define DP_IN_ADAPTER_INFO                0xe000e
#define DP_USB4_DRIVER_ID                 0xe000f
#define DP_USB4_ROUTER_TOPOLOGY_ID        0xe001b

enum dc_status dpcd_get_tunneling_device_data(struct dc_link *link)
{
	enum dc_status status = DC_OK;
	uint8_t dpcd_dp_tun_data[3] = {0};
	uint8_t dpcd_topology_data[DPCD_USB4_TOPOLOGY_ID_LEN] = {0};
	uint8_t i = 0;

	status = core_link_read_dpcd(
			link,
			DP_TUNNELING_CAPABILITIES_SUPPORT,
			dpcd_dp_tun_data,
			sizeof(dpcd_dp_tun_data));

	status = core_link_read_dpcd(
			link,
			DP_USB4_ROUTER_TOPOLOGY_ID,
			dpcd_topology_data,
			sizeof(dpcd_topology_data));

	link->dpcd_caps.usb4_dp_tun_info.dp_tun_cap.raw =
			dpcd_dp_tun_data[DP_TUNNELING_CAPABILITIES_SUPPORT - DP_TUNNELING_CAPABILITIES_SUPPORT];
	link->dpcd_caps.usb4_dp_tun_info.dpia_info.raw =
			dpcd_dp_tun_data[DP_IN_ADAPTER_INFO - DP_TUNNELING_CAPABILITIES_SUPPORT];
	link->dpcd_caps.usb4_dp_tun_info.usb4_driver_id =
			dpcd_dp_tun_data[DP_USB4_DRIVER_ID - DP_TUNNELING_CAPABILITIES_SUPPORT];

	for (i = 0; i < DPCD_USB4_TOPOLOGY_ID_LEN; i++)
		link->dpcd_caps.usb4_dp_tun_info.usb4_topology_id[i] = dpcd_topology_data[i];

	return status;
}

bool dpia_query_hpd_status(struct dc_link *link)
{
	union dmub_rb_cmd cmd = {0};
	struct dc_dmub_srv *dmub_srv = link->ctx->dmub_srv;
	bool is_hpd_high = false;

	 
	cmd.query_hpd.header.type = DMUB_CMD__QUERY_HPD_STATE;
	cmd.query_hpd.data.instance = link->link_id.enum_id - ENUM_ID_1;
	cmd.query_hpd.data.ch_type = AUX_CHANNEL_DPIA;

	 
	if (dm_execute_dmub_cmd(dmub_srv->ctx, &cmd, DM_DMUB_WAIT_TYPE_WAIT_WITH_REPLY) && cmd.query_hpd.data.status == AUX_RET_SUCCESS)
		is_hpd_high = cmd.query_hpd.data.result;

	DC_LOG_DEBUG("%s: link(%d) dpia(%d) cmd_status(%d) result(%d)\n",
		__func__,
		link->link_index,
		link->link_id.enum_id - ENUM_ID_1,
		cmd.query_hpd.data.status,
		cmd.query_hpd.data.result);

	return is_hpd_high;
}

