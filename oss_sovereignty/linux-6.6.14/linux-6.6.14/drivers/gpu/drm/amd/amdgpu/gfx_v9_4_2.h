#ifndef __GFX_V9_4_2_H__
#define __GFX_V9_4_2_H__
void gfx_v9_4_2_debug_trap_config_init(struct amdgpu_device *adev,
				uint32_t first_vmid, uint32_t last_vmid);
void gfx_v9_4_2_init_golden_registers(struct amdgpu_device *adev,
				      uint32_t die_id);
void gfx_v9_4_2_set_power_brake_sequence(struct amdgpu_device *adev);
int gfx_v9_4_2_do_edc_gpr_workarounds(struct amdgpu_device *adev);
extern struct amdgpu_gfx_ras gfx_v9_4_2_ras;
#endif  
