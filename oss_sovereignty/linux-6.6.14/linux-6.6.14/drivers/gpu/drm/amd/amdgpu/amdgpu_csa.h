#ifndef AMDGPU_CSA_MANAGER_H
#define AMDGPU_CSA_MANAGER_H
#define AMDGPU_CSA_SIZE		(128 * 1024)
uint32_t amdgpu_get_total_csa_size(struct amdgpu_device *adev);
uint64_t amdgpu_csa_vaddr(struct amdgpu_device *adev);
int amdgpu_allocate_static_csa(struct amdgpu_device *adev, struct amdgpu_bo **bo,
				u32 domain, uint32_t size);
int amdgpu_map_static_csa(struct amdgpu_device *adev, struct amdgpu_vm *vm,
			  struct amdgpu_bo *bo, struct amdgpu_bo_va **bo_va,
			  uint64_t csa_addr, uint32_t size);
int amdgpu_unmap_static_csa(struct amdgpu_device *adev, struct amdgpu_vm *vm,
			    struct amdgpu_bo *bo, struct amdgpu_bo_va *bo_va,
			    uint64_t csa_addr);
void amdgpu_free_static_csa(struct amdgpu_bo **bo);
#endif
