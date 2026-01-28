#ifndef __TIDSS_DRV_H__
#define __TIDSS_DRV_H__
#include <linux/spinlock.h>
#define TIDSS_MAX_PORTS 4
#define TIDSS_MAX_PLANES 4
typedef u32 dispc_irq_t;
struct tidss_device {
	struct drm_device ddev;		 
	struct device *dev;		 
	const struct dispc_features *feat;
	struct dispc_device *dispc;
	unsigned int num_crtcs;
	struct drm_crtc *crtcs[TIDSS_MAX_PORTS];
	unsigned int num_planes;
	struct drm_plane *planes[TIDSS_MAX_PLANES];
	unsigned int irq;
	spinlock_t wait_lock;	 
	dispc_irq_t irq_mask;	 
};
#define to_tidss(__dev) container_of(__dev, struct tidss_device, ddev)
int tidss_runtime_get(struct tidss_device *tidss);
void tidss_runtime_put(struct tidss_device *tidss);
#endif
