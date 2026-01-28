#ifndef __AMDGPU_LSDMA_H__
#define __AMDGPU_LSDMA_H__
struct amdgpu_lsdma {
	const struct amdgpu_lsdma_funcs      *funcs;
};
struct amdgpu_lsdma_funcs {
	int (*copy_mem)(struct amdgpu_device *adev, uint64_t src_addr,
			uint64_t dst_addr, uint64_t size);
	int (*fill_mem)(struct amdgpu_device *adev, uint64_t dst_addr,
			uint32_t data, uint64_t size);
	void (*update_memory_power_gating)(struct amdgpu_device *adev, bool enable);
};
int amdgpu_lsdma_copy_mem(struct amdgpu_device *adev, uint64_t src_addr,
			  uint64_t dst_addr, uint64_t mem_size);
int amdgpu_lsdma_fill_mem(struct amdgpu_device *adev, uint64_t dst_addr,
			  uint32_t data, uint64_t mem_size);
int amdgpu_lsdma_wait_for(struct amdgpu_device *adev, uint32_t reg_index,
			  uint32_t reg_val, uint32_t mask);
#endif
