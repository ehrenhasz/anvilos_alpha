 

#include <drm/drm_exec.h>

#include "amdgpu.h"

uint64_t amdgpu_csa_vaddr(struct amdgpu_device *adev)
{
	uint64_t addr = adev->vm_manager.max_pfn << AMDGPU_GPU_PAGE_SHIFT;

	addr -= AMDGPU_VA_RESERVED_SIZE;
	addr = amdgpu_gmc_sign_extend(addr);

	return addr;
}

int amdgpu_allocate_static_csa(struct amdgpu_device *adev, struct amdgpu_bo **bo,
				u32 domain, uint32_t size)
{
	void *ptr;

	amdgpu_bo_create_kernel(adev, size, PAGE_SIZE,
				domain, bo,
				NULL, &ptr);
	if (!*bo)
		return -ENOMEM;

	memset(ptr, 0, size);
	adev->virt.csa_cpu_addr = ptr;
	return 0;
}

void amdgpu_free_static_csa(struct amdgpu_bo **bo)
{
	amdgpu_bo_free_kernel(bo, NULL, NULL);
}

 
int amdgpu_map_static_csa(struct amdgpu_device *adev, struct amdgpu_vm *vm,
			  struct amdgpu_bo *bo, struct amdgpu_bo_va **bo_va,
			  uint64_t csa_addr, uint32_t size)
{
	struct drm_exec exec;
	int r;

	drm_exec_init(&exec, DRM_EXEC_INTERRUPTIBLE_WAIT);
	drm_exec_until_all_locked(&exec) {
		r = amdgpu_vm_lock_pd(vm, &exec, 0);
		if (likely(!r))
			r = drm_exec_lock_obj(&exec, &bo->tbo.base);
		drm_exec_retry_on_contention(&exec);
		if (unlikely(r)) {
			DRM_ERROR("failed to reserve CSA,PD BOs: err=%d\n", r);
			goto error;
		}
	}

	*bo_va = amdgpu_vm_bo_add(adev, vm, bo);
	if (!*bo_va) {
		r = -ENOMEM;
		goto error;
	}

	r = amdgpu_vm_bo_map(adev, *bo_va, csa_addr, 0, size,
			     AMDGPU_PTE_READABLE | AMDGPU_PTE_WRITEABLE |
			     AMDGPU_PTE_EXECUTABLE);

	if (r) {
		DRM_ERROR("failed to do bo_map on static CSA, err=%d\n", r);
		amdgpu_vm_bo_del(adev, *bo_va);
		goto error;
	}

error:
	drm_exec_fini(&exec);
	return r;
}

int amdgpu_unmap_static_csa(struct amdgpu_device *adev, struct amdgpu_vm *vm,
			    struct amdgpu_bo *bo, struct amdgpu_bo_va *bo_va,
			    uint64_t csa_addr)
{
	struct drm_exec exec;
	int r;

	drm_exec_init(&exec, DRM_EXEC_INTERRUPTIBLE_WAIT);
	drm_exec_until_all_locked(&exec) {
		r = amdgpu_vm_lock_pd(vm, &exec, 0);
		if (likely(!r))
			r = drm_exec_lock_obj(&exec, &bo->tbo.base);
		drm_exec_retry_on_contention(&exec);
		if (unlikely(r)) {
			DRM_ERROR("failed to reserve CSA,PD BOs: err=%d\n", r);
			goto error;
		}
	}

	r = amdgpu_vm_bo_unmap(adev, bo_va, csa_addr);
	if (r) {
		DRM_ERROR("failed to do bo_unmap on static CSA, err=%d\n", r);
		goto error;
	}

	amdgpu_vm_bo_del(adev, bo_va);

error:
	drm_exec_fini(&exec);
	return r;
}
