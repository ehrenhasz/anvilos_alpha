 

#ifndef _DRM_DP_MST_HELPER_INTERNAL_H_
#define _DRM_DP_MST_HELPER_INTERNAL_H_

#include <drm/display/drm_dp_mst_helper.h>

void
drm_dp_encode_sideband_req(const struct drm_dp_sideband_msg_req_body *req,
			   struct drm_dp_sideband_msg_tx *raw);
int drm_dp_decode_sideband_req(const struct drm_dp_sideband_msg_tx *raw,
			       struct drm_dp_sideband_msg_req_body *req);
void
drm_dp_dump_sideband_msg_req_body(const struct drm_dp_sideband_msg_req_body *req,
				  int indent, struct drm_printer *printer);

#endif  
