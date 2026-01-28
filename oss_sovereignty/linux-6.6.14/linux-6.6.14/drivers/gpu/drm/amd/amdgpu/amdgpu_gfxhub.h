#ifndef __AMDGPU_GFXHUB_H__
#define __AMDGPU_GFXHUB_H__
struct amdgpu_gfxhub_funcs {
	u64 (*get_fb_location)(struct amdgpu_device *adev);
	u64 (*get_mc_fb_offset)(struct amdgpu_device *adev);
	void (*setup_vm_pt_regs)(struct amdgpu_device *adev, uint32_t vmid,
			uint64_t page_table_base);
	int (*gart_enable)(struct amdgpu_device *adev);
	void (*gart_disable)(struct amdgpu_device *adev);
	void (*set_fault_enable_default)(struct amdgpu_device *adev, bool value);
	void (*init)(struct amdgpu_device *adev);
	int (*get_xgmi_info)(struct amdgpu_device *adev);
	void (*utcl2_harvest)(struct amdgpu_device *adev);
	void (*mode2_save_regs)(struct amdgpu_device *adev);
	void (*mode2_restore_regs)(struct amdgpu_device *adev);
	void (*halt)(struct amdgpu_device *adev);
};
struct amdgpu_gfxhub {
	const struct amdgpu_gfxhub_funcs *funcs;
};
#endif
