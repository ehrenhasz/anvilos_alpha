 
#ifndef __AMDGPU_SMUIO_H__
#define __AMDGPU_SMUIO_H__

struct amdgpu_smuio_funcs {
	u32 (*get_rom_index_offset)(struct amdgpu_device *adev);
	u32 (*get_rom_data_offset)(struct amdgpu_device *adev);
	void (*update_rom_clock_gating)(struct amdgpu_device *adev, bool enable);
	void (*get_clock_gating_state)(struct amdgpu_device *adev, u64 *flags);
	u32 (*get_die_id)(struct amdgpu_device *adev);
	u32 (*get_socket_id)(struct amdgpu_device *adev);
	enum amdgpu_pkg_type (*get_pkg_type)(struct amdgpu_device *adev);
	bool (*is_host_gpu_xgmi_supported)(struct amdgpu_device *adev);
};

struct amdgpu_smuio {
	const struct amdgpu_smuio_funcs		*funcs;
};

#endif  
