uint32_t kgd_aldebaran_enable_debug_trap(struct amdgpu_device *adev,
					bool restore_dbg_registers,
					uint32_t vmid);
uint32_t kgd_aldebaran_set_wave_launch_mode(struct amdgpu_device *adev,
					uint8_t wave_launch_mode,
					uint32_t vmid);
