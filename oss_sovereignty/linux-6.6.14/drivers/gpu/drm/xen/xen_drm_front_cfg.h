 

 

#ifndef __XEN_DRM_FRONT_CFG_H_
#define __XEN_DRM_FRONT_CFG_H_

#include <linux/types.h>

#define XEN_DRM_FRONT_MAX_CRTCS	4

struct xen_drm_front_cfg_connector {
	int width;
	int height;
	char *xenstore_path;
};

struct xen_drm_front_cfg {
	struct xen_drm_front_info *front_info;
	 
	int num_connectors;
	 
	struct xen_drm_front_cfg_connector connectors[XEN_DRM_FRONT_MAX_CRTCS];
	 
	bool be_alloc;
};

int xen_drm_front_cfg_card(struct xen_drm_front_info *front_info,
			   struct xen_drm_front_cfg *cfg);

#endif  
