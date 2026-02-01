 
 

#ifndef _EXYNOS_DRM_CRTC_H_
#define _EXYNOS_DRM_CRTC_H_


#include "exynos_drm_drv.h"

struct exynos_drm_crtc *exynos_drm_crtc_create(struct drm_device *drm_dev,
					struct drm_plane *plane,
					enum exynos_drm_output_type out_type,
					const struct exynos_drm_crtc_ops *ops,
					void *context);
void exynos_drm_crtc_wait_pending_update(struct exynos_drm_crtc *exynos_crtc);
void exynos_drm_crtc_finish_update(struct exynos_drm_crtc *exynos_crtc,
				   struct exynos_drm_plane *exynos_plane);

 
struct exynos_drm_crtc *exynos_drm_crtc_get_by_type(struct drm_device *drm_dev,
				       enum exynos_drm_output_type out_type);

int exynos_drm_set_possible_crtcs(struct drm_encoder *encoder,
		enum exynos_drm_output_type out_type);

 
void exynos_drm_crtc_te_handler(struct drm_crtc *crtc);

void exynos_crtc_handle_event(struct exynos_drm_crtc *exynos_crtc);

#endif
