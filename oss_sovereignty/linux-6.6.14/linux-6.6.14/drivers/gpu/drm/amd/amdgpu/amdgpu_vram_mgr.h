#ifndef __AMDGPU_VRAM_MGR_H__
#define __AMDGPU_VRAM_MGR_H__
#include <drm/drm_buddy.h>
struct amdgpu_vram_mgr {
	struct ttm_resource_manager manager;
	struct drm_buddy mm;
	struct mutex lock;
	struct list_head reservations_pending;
	struct list_head reserved_pages;
	atomic64_t vis_usage;
	u64 default_page_size;
};
struct amdgpu_vram_mgr_resource {
	struct ttm_resource base;
	struct list_head blocks;
	unsigned long flags;
};
static inline u64 amdgpu_vram_mgr_block_start(struct drm_buddy_block *block)
{
	return drm_buddy_block_offset(block);
}
static inline u64 amdgpu_vram_mgr_block_size(struct drm_buddy_block *block)
{
	return (u64)PAGE_SIZE << drm_buddy_block_order(block);
}
static inline struct amdgpu_vram_mgr_resource *
to_amdgpu_vram_mgr_resource(struct ttm_resource *res)
{
	return container_of(res, struct amdgpu_vram_mgr_resource, base);
}
#endif
