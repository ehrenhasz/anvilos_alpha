 

 

#ifndef __XEN_DRM_FRONT_CONN_H_
#define __XEN_DRM_FRONT_CONN_H_

#include <linux/types.h>

struct drm_connector;
struct xen_drm_front_drm_info;


int xen_drm_front_conn_init(struct xen_drm_front_drm_info *drm_info,
			    struct drm_connector *connector);

const u32 *xen_drm_front_conn_get_formats(int *format_count);

#endif  
