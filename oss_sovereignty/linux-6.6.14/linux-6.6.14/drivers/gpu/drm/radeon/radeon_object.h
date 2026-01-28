#ifndef __RADEON_OBJECT_H__
#define __RADEON_OBJECT_H__
#include <drm/radeon_drm.h>
#include "radeon.h"
static inline unsigned radeon_mem_type_to_domain(u32 mem_type)
{
	switch (mem_type) {
	case TTM_PL_VRAM:
		return RADEON_GEM_DOMAIN_VRAM;
	case TTM_PL_TT:
		return RADEON_GEM_DOMAIN_GTT;
	case TTM_PL_SYSTEM:
		return RADEON_GEM_DOMAIN_CPU;
	default:
		break;
	}
	return 0;
}
static inline int radeon_bo_reserve(struct radeon_bo *bo, bool no_intr)
{
	int r;
	r = ttm_bo_reserve(&bo->tbo, !no_intr, false, NULL);
	if (unlikely(r != 0)) {
		if (r != -ERESTARTSYS)
			dev_err(bo->rdev->dev, "%p reserve failed\n", bo);
		return r;
	}
	return 0;
}
static inline void radeon_bo_unreserve(struct radeon_bo *bo)
{
	ttm_bo_unreserve(&bo->tbo);
}
static inline u64 radeon_bo_gpu_offset(struct radeon_bo *bo)
{
	struct radeon_device *rdev;
	u64 start = 0;
	rdev = radeon_get_rdev(bo->tbo.bdev);
	switch (bo->tbo.resource->mem_type) {
	case TTM_PL_TT:
		start = rdev->mc.gtt_start;
		break;
	case TTM_PL_VRAM:
		start = rdev->mc.vram_start;
		break;
	}
	return (bo->tbo.resource->start << PAGE_SHIFT) + start;
}
static inline unsigned long radeon_bo_size(struct radeon_bo *bo)
{
	return bo->tbo.base.size;
}
static inline unsigned radeon_bo_ngpu_pages(struct radeon_bo *bo)
{
	return bo->tbo.base.size / RADEON_GPU_PAGE_SIZE;
}
static inline unsigned radeon_bo_gpu_page_alignment(struct radeon_bo *bo)
{
	return (bo->tbo.page_alignment << PAGE_SHIFT) / RADEON_GPU_PAGE_SIZE;
}
static inline u64 radeon_bo_mmap_offset(struct radeon_bo *bo)
{
	return drm_vma_node_offset_addr(&bo->tbo.base.vma_node);
}
extern int radeon_bo_create(struct radeon_device *rdev,
			    unsigned long size, int byte_align,
			    bool kernel, u32 domain, u32 flags,
			    struct sg_table *sg,
			    struct dma_resv *resv,
			    struct radeon_bo **bo_ptr);
extern int radeon_bo_kmap(struct radeon_bo *bo, void **ptr);
extern void radeon_bo_kunmap(struct radeon_bo *bo);
extern struct radeon_bo *radeon_bo_ref(struct radeon_bo *bo);
extern void radeon_bo_unref(struct radeon_bo **bo);
extern int radeon_bo_pin(struct radeon_bo *bo, u32 domain, u64 *gpu_addr);
extern int radeon_bo_pin_restricted(struct radeon_bo *bo, u32 domain,
				    u64 max_offset, u64 *gpu_addr);
extern void radeon_bo_unpin(struct radeon_bo *bo);
extern int radeon_bo_evict_vram(struct radeon_device *rdev);
extern void radeon_bo_force_delete(struct radeon_device *rdev);
extern int radeon_bo_init(struct radeon_device *rdev);
extern void radeon_bo_fini(struct radeon_device *rdev);
extern int radeon_bo_list_validate(struct radeon_device *rdev,
				   struct ww_acquire_ctx *ticket,
				   struct list_head *head, int ring);
extern int radeon_bo_set_tiling_flags(struct radeon_bo *bo,
				u32 tiling_flags, u32 pitch);
extern void radeon_bo_get_tiling_flags(struct radeon_bo *bo,
				u32 *tiling_flags, u32 *pitch);
extern int radeon_bo_check_tiling(struct radeon_bo *bo, bool has_moved,
				bool force_drop);
extern void radeon_bo_move_notify(struct ttm_buffer_object *bo);
extern vm_fault_t radeon_bo_fault_reserve_notify(struct ttm_buffer_object *bo);
extern int radeon_bo_get_surface_reg(struct radeon_bo *bo);
extern void radeon_bo_fence(struct radeon_bo *bo, struct radeon_fence *fence,
			    bool shared);
static inline struct radeon_sa_manager *
to_radeon_sa_manager(struct drm_suballoc_manager *manager)
{
	return container_of(manager, struct radeon_sa_manager, base);
}
static inline uint64_t radeon_sa_bo_gpu_addr(struct drm_suballoc *sa_bo)
{
	return to_radeon_sa_manager(sa_bo->manager)->gpu_addr +
		drm_suballoc_soffset(sa_bo);
}
static inline void *radeon_sa_bo_cpu_addr(struct drm_suballoc *sa_bo)
{
	return to_radeon_sa_manager(sa_bo->manager)->cpu_ptr +
		drm_suballoc_soffset(sa_bo);
}
extern int radeon_sa_bo_manager_init(struct radeon_device *rdev,
				     struct radeon_sa_manager *sa_manager,
				     unsigned size, u32 align, u32 domain,
				     u32 flags);
extern void radeon_sa_bo_manager_fini(struct radeon_device *rdev,
				      struct radeon_sa_manager *sa_manager);
extern int radeon_sa_bo_manager_start(struct radeon_device *rdev,
				      struct radeon_sa_manager *sa_manager);
extern int radeon_sa_bo_manager_suspend(struct radeon_device *rdev,
					struct radeon_sa_manager *sa_manager);
extern int radeon_sa_bo_new(struct radeon_sa_manager *sa_manager,
			    struct drm_suballoc **sa_bo,
			    unsigned int size, unsigned int align);
extern void radeon_sa_bo_free(struct drm_suballoc **sa_bo,
			      struct radeon_fence *fence);
#if defined(CONFIG_DEBUG_FS)
extern void radeon_sa_bo_dump_debug_info(struct radeon_sa_manager *sa_manager,
					 struct seq_file *m);
#endif
#endif
