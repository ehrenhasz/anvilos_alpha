#ifndef __MALIDP_MW_H__
#define __MALIDP_MW_H__
int malidp_mw_connector_init(struct drm_device *drm);
void malidp_mw_atomic_commit(struct drm_device *drm,
			     struct drm_atomic_state *old_state);
#endif
