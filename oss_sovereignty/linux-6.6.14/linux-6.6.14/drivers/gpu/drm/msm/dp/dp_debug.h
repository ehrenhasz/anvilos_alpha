#ifndef _DP_DEBUG_H_
#define _DP_DEBUG_H_
#include "dp_panel.h"
#include "dp_link.h"
struct dp_debug {
	bool debug_en;
	int aspect_ratio;
	int vdisplay;
	int hdisplay;
	int vrefresh;
};
#if defined(CONFIG_DEBUG_FS)
struct dp_debug *dp_debug_get(struct device *dev, struct dp_panel *panel,
		struct dp_link *link,
		struct drm_connector *connector,
		struct drm_minor *minor);
void dp_debug_put(struct dp_debug *dp_debug);
#else
static inline
struct dp_debug *dp_debug_get(struct device *dev, struct dp_panel *panel,
		struct dp_link *link,
		struct drm_connector *connector, struct drm_minor *minor)
{
	return ERR_PTR(-EINVAL);
}
static inline void dp_debug_put(struct dp_debug *dp_debug)
{
}
#endif  
#endif  
