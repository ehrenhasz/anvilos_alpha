#ifndef DC_INC_LINK_DP_DPIA_BW_H_
#define DC_INC_LINK_DP_DPIA_BW_H_
#include "link.h"
#define MAX_HR_NUM			2
#define MAX_DPIA_NUM		(MAX_HR_NUM * 2)
enum bw_type {
	HOST_ROUTER_BW_ESTIMATED,
	HOST_ROUTER_BW_ALLOCATED,
	HOST_ROUTER_BW_INVALID,
};
bool link_dp_dpia_set_dptx_usb4_bw_alloc_support(struct dc_link *link);
int link_dp_dpia_allocate_usb4_bandwidth_for_stream(struct dc_link *link, int req_bw);
int dpia_handle_usb4_bandwidth_allocation_for_link(struct dc_link *link, int peak_bw);
void dpia_handle_bw_alloc_response(struct dc_link *link, uint8_t bw, uint8_t result);
bool dpia_validate_usb4_bw(struct dc_link **link, int *bw_needed, const unsigned int num_dpias);
#endif  
