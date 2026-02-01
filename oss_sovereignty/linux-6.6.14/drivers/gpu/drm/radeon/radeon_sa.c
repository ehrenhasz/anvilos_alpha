 
 
 

#include "radeon.h"

int radeon_sa_bo_manager_init(struct radeon_device *rdev,
			      struct radeon_sa_manager *sa_manager,
			      unsigned int size, u32 sa_align, u32 domain,
			      u32 flags)
{
	int r;

	r = radeon_bo_create(rdev, size, RADEON_GPU_PAGE_SIZE, true,
			     domain, flags, NULL, NULL, &sa_manager->bo);
	if (r) {
		dev_err(rdev->dev, "(%d) failed to allocate bo for manager\n", r);
		return r;
	}

	sa_manager->domain = domain;

	drm_suballoc_manager_init(&sa_manager->base, size, sa_align);

	return r;
}

void radeon_sa_bo_manager_fini(struct radeon_device *rdev,
			       struct radeon_sa_manager *sa_manager)
{
	drm_suballoc_manager_fini(&sa_manager->base);
	radeon_bo_unref(&sa_manager->bo);
}

int radeon_sa_bo_manager_start(struct radeon_device *rdev,
			       struct radeon_sa_manager *sa_manager)
{
	int r;

	if (sa_manager->bo == NULL) {
		dev_err(rdev->dev, "no bo for sa manager\n");
		return -EINVAL;
	}

	 
	r = radeon_bo_reserve(sa_manager->bo, false);
	if (r) {
		dev_err(rdev->dev, "(%d) failed to reserve manager bo\n", r);
		return r;
	}
	r = radeon_bo_pin(sa_manager->bo, sa_manager->domain, &sa_manager->gpu_addr);
	if (r) {
		radeon_bo_unreserve(sa_manager->bo);
		dev_err(rdev->dev, "(%d) failed to pin manager bo\n", r);
		return r;
	}
	r = radeon_bo_kmap(sa_manager->bo, &sa_manager->cpu_ptr);
	radeon_bo_unreserve(sa_manager->bo);
	return r;
}

int radeon_sa_bo_manager_suspend(struct radeon_device *rdev,
				 struct radeon_sa_manager *sa_manager)
{
	int r;

	if (sa_manager->bo == NULL) {
		dev_err(rdev->dev, "no bo for sa manager\n");
		return -EINVAL;
	}

	r = radeon_bo_reserve(sa_manager->bo, false);
	if (!r) {
		radeon_bo_kunmap(sa_manager->bo);
		radeon_bo_unpin(sa_manager->bo);
		radeon_bo_unreserve(sa_manager->bo);
	}
	return r;
}

int radeon_sa_bo_new(struct radeon_sa_manager *sa_manager,
		     struct drm_suballoc **sa_bo,
		     unsigned int size, unsigned int align)
{
	struct drm_suballoc *sa = drm_suballoc_new(&sa_manager->base, size,
						   GFP_KERNEL, false, align);

	if (IS_ERR(sa)) {
		*sa_bo = NULL;
		return PTR_ERR(sa);
	}

	*sa_bo = sa;
	return 0;
}

void radeon_sa_bo_free(struct drm_suballoc **sa_bo,
		       struct radeon_fence *fence)
{
	if (sa_bo == NULL || *sa_bo == NULL) {
		return;
	}

	if (fence)
		drm_suballoc_free(*sa_bo, &fence->base);
	else
		drm_suballoc_free(*sa_bo, NULL);

	*sa_bo = NULL;
}

#if defined(CONFIG_DEBUG_FS)
void radeon_sa_bo_dump_debug_info(struct radeon_sa_manager *sa_manager,
				  struct seq_file *m)
{
	struct drm_printer p = drm_seq_file_printer(m);

	drm_suballoc_dump_debug_info(&sa_manager->base, &p, sa_manager->gpu_addr);
}
#endif
