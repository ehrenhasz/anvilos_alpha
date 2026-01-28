#ifndef __LEGACY_DPM_H__
#define __LEGACY_DPM_H__
void amdgpu_dpm_print_class_info(u32 class, u32 class2);
void amdgpu_dpm_print_cap_info(u32 caps);
void amdgpu_dpm_print_ps_status(struct amdgpu_device *adev,
				struct amdgpu_ps *rps);
int amdgpu_get_platform_caps(struct amdgpu_device *adev);
int amdgpu_parse_extended_power_table(struct amdgpu_device *adev);
void amdgpu_free_extended_power_table(struct amdgpu_device *adev);
void amdgpu_add_thermal_controller(struct amdgpu_device *adev);
struct amd_vce_state* amdgpu_get_vce_clock_state(void *handle, u32 idx);
void amdgpu_pm_print_power_states(struct amdgpu_device *adev);
void amdgpu_legacy_dpm_compute_clocks(void *handle);
void amdgpu_dpm_thermal_work_handler(struct work_struct *work);
#endif
