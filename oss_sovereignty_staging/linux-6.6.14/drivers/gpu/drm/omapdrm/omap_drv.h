 
 

#ifndef __OMAPDRM_DRV_H__
#define __OMAPDRM_DRV_H__

#include <linux/module.h>
#include <linux/types.h>
#include <linux/workqueue.h>

#include "dss/omapdss.h"
#include "dss/dss.h"

#include <drm/drm_atomic.h>
#include <drm/drm_gem.h>
#include <drm/omap_drm.h>

#include "omap_crtc.h"
#include "omap_encoder.h"
#include "omap_fb.h"
#include "omap_gem.h"
#include "omap_irq.h"
#include "omap_plane.h"
#include "omap_overlay.h"

#define DBG(fmt, ...) DRM_DEBUG_DRIVER(fmt"\n", ##__VA_ARGS__)
#define VERB(fmt, ...) if (0) DRM_DEBUG_DRIVER(fmt, ##__VA_ARGS__)  

#define MODULE_NAME     "omapdrm"

struct omap_drm_usergart;

struct omap_drm_pipeline {
	struct drm_crtc *crtc;
	struct drm_encoder *encoder;
	struct drm_connector *connector;
	struct omap_dss_device *output;
	unsigned int alias_id;
};

 
#define to_omap_global_state(x) container_of(x, struct omap_global_state, base)

struct omap_global_state {
	struct drm_private_state base;

	 
	struct drm_plane *hwoverlay_to_plane[8];
};

struct omap_drm_private {
	struct drm_device *ddev;
	struct device *dev;
	u32 omaprev;

	struct dss_device *dss;
	struct dispc_device *dispc;

	bool irq_enabled;

	unsigned int num_pipes;
	struct omap_drm_pipeline pipes[8];
	struct omap_drm_pipeline *channels[8];

	unsigned int num_planes;
	struct drm_plane *planes[8];

	unsigned int num_ovls;
	struct omap_hw_overlay *overlays[8];

	struct drm_private_obj glob_obj;

	struct workqueue_struct *wq;

	 
	struct mutex list_lock;

	 
	struct list_head obj_list;

	struct omap_drm_usergart *usergart;
	bool has_dmm;

	 
	struct drm_property *zorder_prop;

	 
	spinlock_t wait_lock;		 
	struct list_head wait_list;	 
	u32 irq_mask;			 

	 
	unsigned int max_bandwidth;
};


void omap_debugfs_init(struct drm_minor *minor);

struct omap_global_state * __must_check omap_get_global_state(struct drm_atomic_state *s);

struct omap_global_state *omap_get_existing_global_state(struct omap_drm_private *priv);

#endif  
