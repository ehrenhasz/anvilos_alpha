
 
 

 
#include "link_dp_dpia_bw.h"
#include "link_dpcd.h"
#include "dc_dmub_srv.h"

#define DC_LOGGER \
	link->ctx->logger

#define Kbps_TO_Gbps (1000 * 1000)




 
static bool get_bw_alloc_proceed_flag(struct dc_link *tmp)
{
	return (tmp && DISPLAY_ENDPOINT_USB4_DPIA == tmp->ep_type
			&& tmp->hpd_status
			&& tmp->dpia_bw_alloc_config.bw_alloc_enabled);
}
static void reset_bw_alloc_struct(struct dc_link *link)
{
	link->dpia_bw_alloc_config.bw_alloc_enabled = false;
	link->dpia_bw_alloc_config.sink_verified_bw = 0;
	link->dpia_bw_alloc_config.sink_max_bw = 0;
	link->dpia_bw_alloc_config.estimated_bw = 0;
	link->dpia_bw_alloc_config.bw_granularity = 0;
	link->dpia_bw_alloc_config.response_ready = false;
}
static uint8_t get_bw_granularity(struct dc_link *link)
{
	uint8_t bw_granularity = 0;

	core_link_read_dpcd(
			link,
			DP_BW_GRANULALITY,
			&bw_granularity,
			sizeof(uint8_t));

	switch (bw_granularity & 0x3) {
	case 0:
		bw_granularity = 4;
		break;
	case 1:
	default:
		bw_granularity = 2;
		break;
	}

	return bw_granularity;
}
static int get_estimated_bw(struct dc_link *link)
{
	uint8_t bw_estimated_bw = 0;

	core_link_read_dpcd(
			link,
			ESTIMATED_BW,
			&bw_estimated_bw,
			sizeof(uint8_t));

	return bw_estimated_bw * (Kbps_TO_Gbps / link->dpia_bw_alloc_config.bw_granularity);
}
static bool allocate_usb4_bw(int *stream_allocated_bw, int bw_needed, struct dc_link *link)
{
	if (bw_needed > 0)
		*stream_allocated_bw += bw_needed;

	return true;
}
static bool deallocate_usb4_bw(int *stream_allocated_bw, int bw_to_dealloc, struct dc_link *link)
{
	bool ret = false;

	if (*stream_allocated_bw > 0) {
		*stream_allocated_bw -= bw_to_dealloc;
		ret = true;
	} else {
		
		ret = true;
	}

	
	if (!link->hpd_status)
		reset_bw_alloc_struct(link);

	return ret;
}
 
static void init_usb4_bw_struct(struct dc_link *link)
{
	
	link->dpia_bw_alloc_config.bw_granularity = get_bw_granularity(link);
	link->dpia_bw_alloc_config.estimated_bw = get_estimated_bw(link);
}
static uint8_t get_lowest_dpia_index(struct dc_link *link)
{
	const struct dc *dc_struct = link->dc;
	uint8_t idx = 0xFF;
	int i;

	for (i = 0; i < MAX_PIPES * 2; ++i) {

		if (!dc_struct->links[i] ||
				dc_struct->links[i]->ep_type != DISPLAY_ENDPOINT_USB4_DPIA)
			continue;

		if (idx > dc_struct->links[i]->link_index)
			idx = dc_struct->links[i]->link_index;
	}

	return idx;
}
 
static int get_host_router_total_bw(struct dc_link *link, uint8_t type)
{
	const struct dc *dc_struct = link->dc;
	uint8_t lowest_dpia_index = get_lowest_dpia_index(link);
	uint8_t idx = (link->link_index - lowest_dpia_index) / 2, idx_temp = 0;
	struct dc_link *link_temp;
	int total_bw = 0;
	int i;

	for (i = 0; i < MAX_PIPES * 2; ++i) {

		if (!dc_struct->links[i] || dc_struct->links[i]->ep_type != DISPLAY_ENDPOINT_USB4_DPIA)
			continue;

		link_temp = dc_struct->links[i];
		if (!link_temp || !link_temp->hpd_status)
			continue;

		idx_temp = (link_temp->link_index - lowest_dpia_index) / 2;

		if (idx_temp == idx) {

			if (type == HOST_ROUTER_BW_ESTIMATED)
				total_bw += link_temp->dpia_bw_alloc_config.estimated_bw;
			else if (type == HOST_ROUTER_BW_ALLOCATED)
				total_bw += link_temp->dpia_bw_alloc_config.sink_allocated_bw;
		}
	}

	return total_bw;
}
 
static bool dpia_bw_alloc_unplug(struct dc_link *link)
{
	if (!link)
		return true;

	return deallocate_usb4_bw(&link->dpia_bw_alloc_config.sink_allocated_bw,
			link->dpia_bw_alloc_config.sink_allocated_bw, link);
}
static void set_usb4_req_bw_req(struct dc_link *link, int req_bw)
{
	uint8_t requested_bw;
	uint32_t temp;

	
	if (req_bw > link->dpia_bw_alloc_config.estimated_bw)
		req_bw = link->dpia_bw_alloc_config.estimated_bw;

	temp = req_bw * link->dpia_bw_alloc_config.bw_granularity;
	requested_bw = temp / Kbps_TO_Gbps;

	
	if (temp % Kbps_TO_Gbps)
		++requested_bw;

	
	req_bw = requested_bw * (Kbps_TO_Gbps / link->dpia_bw_alloc_config.bw_granularity);
	if (req_bw == link->dpia_bw_alloc_config.sink_allocated_bw)
		return;

	if (core_link_write_dpcd(
		link,
		REQUESTED_BW,
		&requested_bw,
		sizeof(uint8_t)) == DC_OK)
		link->dpia_bw_alloc_config.response_ready = false; 
}
 
static bool get_cm_response_ready_flag(struct dc_link *link)
{
	return link->dpia_bw_alloc_config.response_ready;
}



bool link_dp_dpia_set_dptx_usb4_bw_alloc_support(struct dc_link *link)
{
	bool ret = false;
	uint8_t response = 0,
			bw_support_dpia = 0,
			bw_support_cm = 0;

	if (!(link->ep_type == DISPLAY_ENDPOINT_USB4_DPIA && link->hpd_status))
		goto out;

	if (core_link_read_dpcd(
			link,
			DP_TUNNELING_CAPABILITIES,
			&response,
			sizeof(uint8_t)) == DC_OK)
		bw_support_dpia = (response >> 7) & 1;

	if (core_link_read_dpcd(
		link,
		USB4_DRIVER_BW_CAPABILITY,
		&response,
		sizeof(uint8_t)) == DC_OK)
		bw_support_cm = (response >> 7) & 1;

	 
	if (bw_support_cm && bw_support_dpia) {

		response = 0x80;
		if (core_link_write_dpcd(
				link,
				DPTX_BW_ALLOCATION_MODE_CONTROL,
				&response,
				sizeof(uint8_t)) != DC_OK) {
			DC_LOG_DEBUG("%s: **** FAILURE Enabling DPtx BW Allocation Mode Support ***\n",
					__func__);
		} else {
			
			link->dpia_bw_alloc_config.bw_alloc_enabled = true;
			DC_LOG_DEBUG("%s: **** SUCCESS Enabling DPtx BW Allocation Mode Support ***\n",
					__func__);

			ret = true;
			init_usb4_bw_struct(link);
		}
	}

out:
	return ret;
}
void dpia_handle_bw_alloc_response(struct dc_link *link, uint8_t bw, uint8_t result)
{
	int bw_needed = 0;
	int estimated = 0;
	int host_router_total_estimated_bw = 0;

	if (!get_bw_alloc_proceed_flag((link)))
		return;

	switch (result) {

	case DPIA_BW_REQ_FAILED:

		DC_LOG_DEBUG("%s: *** *** BW REQ FAILURE for DP-TX Request *** ***\n", __func__);

		
		link->dpia_bw_alloc_config.estimated_bw =
				bw * (Kbps_TO_Gbps / link->dpia_bw_alloc_config.bw_granularity);

		set_usb4_req_bw_req(link, link->dpia_bw_alloc_config.estimated_bw);
		link->dpia_bw_alloc_config.response_ready = false;

		 
		break;

	case DPIA_BW_REQ_SUCCESS:

		DC_LOG_DEBUG("%s: *** BW REQ SUCCESS for DP-TX Request ***\n", __func__);

		
		
		

		bw_needed = bw * (Kbps_TO_Gbps / link->dpia_bw_alloc_config.bw_granularity);

		
		if (!link->dpia_bw_alloc_config.sink_allocated_bw) {

			allocate_usb4_bw(&link->dpia_bw_alloc_config.sink_allocated_bw, bw_needed, link);
			link->dpia_bw_alloc_config.sink_verified_bw =
					link->dpia_bw_alloc_config.sink_allocated_bw;

			
			if (link->dpia_bw_alloc_config.sink_allocated_bw >
			link->dpia_bw_alloc_config.sink_max_bw)
				link->dpia_bw_alloc_config.sink_verified_bw =
						link->dpia_bw_alloc_config.sink_max_bw;
		}
		
		else if (link->dpia_bw_alloc_config.sink_allocated_bw) {

			
			if (link->dpia_bw_alloc_config.sink_allocated_bw > bw_needed)
				deallocate_usb4_bw(&link->dpia_bw_alloc_config.sink_allocated_bw,
						link->dpia_bw_alloc_config.sink_allocated_bw - bw_needed, link);
			else
				allocate_usb4_bw(&link->dpia_bw_alloc_config.sink_allocated_bw,
						bw_needed - link->dpia_bw_alloc_config.sink_allocated_bw, link);
		}

		
		

		link->dpia_bw_alloc_config.response_ready = true;
		break;

	case DPIA_EST_BW_CHANGED:

		DC_LOG_DEBUG("%s: *** ESTIMATED BW CHANGED for DP-TX Request ***\n", __func__);

		estimated = bw * (Kbps_TO_Gbps / link->dpia_bw_alloc_config.bw_granularity);
		host_router_total_estimated_bw = get_host_router_total_bw(link, HOST_ROUTER_BW_ESTIMATED);

		
		if (estimated == host_router_total_estimated_bw) {
			
			if (link->dpia_bw_alloc_config.estimated_bw < estimated)
				link->dpia_bw_alloc_config.estimated_bw = estimated;
		}
		
		else {
			
			link->dpia_bw_alloc_config.estimated_bw = estimated;
		}
		break;

	case DPIA_BW_ALLOC_CAPS_CHANGED:

		DC_LOG_DEBUG("%s: *** BW ALLOC CAPABILITY CHANGED for DP-TX Request ***\n", __func__);
		link->dpia_bw_alloc_config.bw_alloc_enabled = false;
		break;
	}
}
int dpia_handle_usb4_bandwidth_allocation_for_link(struct dc_link *link, int peak_bw)
{
	int ret = 0;
	uint8_t timeout = 10;

	if (!(link && DISPLAY_ENDPOINT_USB4_DPIA == link->ep_type
			&& link->dpia_bw_alloc_config.bw_alloc_enabled))
		goto out;

	
	if (link->hpd_status && peak_bw > 0) {

		
		link->dpia_bw_alloc_config.sink_max_bw = peak_bw;
		set_usb4_req_bw_req(link, link->dpia_bw_alloc_config.sink_max_bw);

		do {
			if (!(timeout > 0))
				timeout--;
			else
				break;
			fsleep(10 * 1000);
		} while (!get_cm_response_ready_flag(link));

		if (!timeout)
			ret = 0;
		else if (link->dpia_bw_alloc_config.sink_allocated_bw > 0)
			ret = get_host_router_total_bw(link, HOST_ROUTER_BW_ALLOCATED);
	}
	
	else if (!link->hpd_status)
		dpia_bw_alloc_unplug(link);

out:
	return ret;
}
int link_dp_dpia_allocate_usb4_bandwidth_for_stream(struct dc_link *link, int req_bw)
{
	int ret = 0;
	uint8_t timeout = 10;

	if (!get_bw_alloc_proceed_flag(link))
		goto out;

	 
	if (req_bw != link->dpia_bw_alloc_config.sink_allocated_bw) {
		set_usb4_req_bw_req(link, req_bw);
		do {
			if (!(timeout > 0))
				timeout--;
			else
				break;
			udelay(10 * 1000);
		} while (!get_cm_response_ready_flag(link));

		if (!timeout)
			ret = 0;
		else if (link->dpia_bw_alloc_config.sink_allocated_bw > 0)
			ret = get_host_router_total_bw(link, HOST_ROUTER_BW_ALLOCATED);
	}

out:
	return ret;
}
bool dpia_validate_usb4_bw(struct dc_link **link, int *bw_needed_per_dpia, const unsigned int num_dpias)
{
	bool ret = true;
	int bw_needed_per_hr[MAX_HR_NUM] = { 0, 0 };
	uint8_t lowest_dpia_index = 0, dpia_index = 0;
	uint8_t i;

	if (!num_dpias || num_dpias > MAX_DPIA_NUM)
		return ret;

	
	for (i = 0; i < num_dpias; ++i) {

		if (!link[i]->dpia_bw_alloc_config.bw_alloc_enabled)
			continue;

		lowest_dpia_index = get_lowest_dpia_index(link[i]);
		if (link[i]->link_index < lowest_dpia_index)
			continue;

		dpia_index = (link[i]->link_index - lowest_dpia_index) / 2;
		bw_needed_per_hr[dpia_index] += bw_needed_per_dpia[i];
		if (bw_needed_per_hr[dpia_index] > get_host_router_total_bw(link[i], HOST_ROUTER_BW_ALLOCATED)) {

			ret = false;
			break;
		}
	}

	return ret;
}
