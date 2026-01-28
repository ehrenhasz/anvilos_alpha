#ifndef __AMDGPU_DM_DEBUGFS_H__
#define __AMDGPU_DM_DEBUGFS_H__
#include "amdgpu.h"
#include "amdgpu_dm.h"
void connector_debugfs_init(struct amdgpu_dm_connector *connector);
void dtn_debugfs_init(struct amdgpu_device *adev);
void crtc_debugfs_init(struct drm_crtc *crtc);
#endif
