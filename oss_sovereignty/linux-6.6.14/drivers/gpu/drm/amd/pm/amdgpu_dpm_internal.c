 

#include "amdgpu.h"
#include "amdgpu_display.h"
#include "hwmgr.h"
#include "amdgpu_smu.h"
#include "amdgpu_dpm_internal.h"

void amdgpu_dpm_get_active_displays(struct amdgpu_device *adev)
{
	struct drm_device *ddev = adev_to_drm(adev);
	struct drm_crtc *crtc;
	struct amdgpu_crtc *amdgpu_crtc;

	adev->pm.dpm.new_active_crtcs = 0;
	adev->pm.dpm.new_active_crtc_count = 0;
	if (adev->mode_info.num_crtc && adev->mode_info.mode_config_initialized) {
		list_for_each_entry(crtc,
				    &ddev->mode_config.crtc_list, head) {
			amdgpu_crtc = to_amdgpu_crtc(crtc);
			if (amdgpu_crtc->enabled) {
				adev->pm.dpm.new_active_crtcs |= (1 << amdgpu_crtc->crtc_id);
				adev->pm.dpm.new_active_crtc_count++;
			}
		}
	}
}

u32 amdgpu_dpm_get_vblank_time(struct amdgpu_device *adev)
{
	struct drm_device *dev = adev_to_drm(adev);
	struct drm_crtc *crtc;
	struct amdgpu_crtc *amdgpu_crtc;
	u32 vblank_in_pixels;
	u32 vblank_time_us = 0xffffffff;  

	if (adev->mode_info.num_crtc && adev->mode_info.mode_config_initialized) {
		list_for_each_entry(crtc, &dev->mode_config.crtc_list, head) {
			amdgpu_crtc = to_amdgpu_crtc(crtc);
			if (crtc->enabled && amdgpu_crtc->enabled && amdgpu_crtc->hw_mode.clock) {
				vblank_in_pixels =
					amdgpu_crtc->hw_mode.crtc_htotal *
					(amdgpu_crtc->hw_mode.crtc_vblank_end -
					amdgpu_crtc->hw_mode.crtc_vdisplay +
					(amdgpu_crtc->v_border * 2));

				vblank_time_us = vblank_in_pixels * 1000 / amdgpu_crtc->hw_mode.clock;
				break;
			}
		}
	}

	return vblank_time_us;
}

u32 amdgpu_dpm_get_vrefresh(struct amdgpu_device *adev)
{
	struct drm_device *dev = adev_to_drm(adev);
	struct drm_crtc *crtc;
	struct amdgpu_crtc *amdgpu_crtc;
	u32 vrefresh = 0;

	if (adev->mode_info.num_crtc && adev->mode_info.mode_config_initialized) {
		list_for_each_entry(crtc, &dev->mode_config.crtc_list, head) {
			amdgpu_crtc = to_amdgpu_crtc(crtc);
			if (crtc->enabled && amdgpu_crtc->enabled && amdgpu_crtc->hw_mode.clock) {
				vrefresh = drm_mode_vrefresh(&amdgpu_crtc->hw_mode);
				break;
			}
		}
	}

	return vrefresh;
}
