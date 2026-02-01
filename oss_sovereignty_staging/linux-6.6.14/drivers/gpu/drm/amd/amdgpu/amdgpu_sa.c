 
 
 

#include "amdgpu.h"

int amdgpu_sa_bo_manager_init(struct amdgpu_device *adev,
			      struct amdgpu_sa_manager *sa_manager,
			      unsigned int size, u32 suballoc_align, u32 domain)
{
	int r;

	r = amdgpu_bo_create_kernel(adev, size, AMDGPU_GPU_PAGE_SIZE, domain,
				    &sa_manager->bo, &sa_manager->gpu_addr,
				    &sa_manager->cpu_ptr);
	if (r) {
		dev_err(adev->dev, "(%d) failed to allocate bo for manager\n", r);
		return r;
	}

	memset(sa_manager->cpu_ptr, 0, size);
	drm_suballoc_manager_init(&sa_manager->base, size, suballoc_align);
	return r;
}

void amdgpu_sa_bo_manager_fini(struct amdgpu_device *adev,
			       struct amdgpu_sa_manager *sa_manager)
{
	if (sa_manager->bo == NULL) {
		dev_err(adev->dev, "no bo for sa manager\n");
		return;
	}

	drm_suballoc_manager_fini(&sa_manager->base);

	amdgpu_bo_free_kernel(&sa_manager->bo, &sa_manager->gpu_addr, &sa_manager->cpu_ptr);
}

int amdgpu_sa_bo_new(struct amdgpu_sa_manager *sa_manager,
		     struct drm_suballoc **sa_bo,
		     unsigned int size)
{
	struct drm_suballoc *sa = drm_suballoc_new(&sa_manager->base, size,
						   GFP_KERNEL, false, 0);

	if (IS_ERR(sa)) {
		*sa_bo = NULL;

		return PTR_ERR(sa);
	}

	*sa_bo = sa;
	return 0;
}

void amdgpu_sa_bo_free(struct amdgpu_device *adev, struct drm_suballoc **sa_bo,
		       struct dma_fence *fence)
{
	if (sa_bo == NULL || *sa_bo == NULL) {
		return;
	}

	drm_suballoc_free(*sa_bo, fence);
	*sa_bo = NULL;
}

#if defined(CONFIG_DEBUG_FS)

void amdgpu_sa_bo_dump_debug_info(struct amdgpu_sa_manager *sa_manager,
				  struct seq_file *m)
{
	struct drm_printer p = drm_seq_file_printer(m);

	drm_suballoc_dump_debug_info(&sa_manager->base, &p, sa_manager->gpu_addr);
}
#endif
