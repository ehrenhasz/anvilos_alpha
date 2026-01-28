#ifndef __AMDGPU_DPM_INTERNAL_H__
#define __AMDGPU_DPM_INTERNAL_H__
void amdgpu_dpm_get_active_displays(struct amdgpu_device *adev);
u32 amdgpu_dpm_get_vblank_time(struct amdgpu_device *adev);
u32 amdgpu_dpm_get_vrefresh(struct amdgpu_device *adev);
#endif
