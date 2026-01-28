#ifndef _I915_GEM_TTM_H_
#define _I915_GEM_TTM_H_
#include <drm/ttm/ttm_placement.h>
#include "gem/i915_gem_object_types.h"
static inline struct ttm_buffer_object *
i915_gem_to_ttm(struct drm_i915_gem_object *obj)
{
	return &obj->__do_not_access;
}
void i915_ttm_bo_destroy(struct ttm_buffer_object *bo);
static inline bool i915_ttm_is_ghost_object(struct ttm_buffer_object *bo)
{
	return bo->destroy != i915_ttm_bo_destroy;
}
static inline struct drm_i915_gem_object *
i915_ttm_to_gem(struct ttm_buffer_object *bo)
{
	return container_of(bo, struct drm_i915_gem_object, __do_not_access);
}
int __i915_gem_ttm_object_init(struct intel_memory_region *mem,
			       struct drm_i915_gem_object *obj,
			       resource_size_t offset,
			       resource_size_t size,
			       resource_size_t page_size,
			       unsigned int flags);
#define I915_PL_LMEM0 TTM_PL_PRIV
#define I915_PL_SYSTEM TTM_PL_SYSTEM
#define I915_PL_STOLEN TTM_PL_VRAM
#define I915_PL_GGTT TTM_PL_TT
struct ttm_placement *i915_ttm_sys_placement(void);
void i915_ttm_free_cached_io_rsgt(struct drm_i915_gem_object *obj);
struct i915_refct_sgt *
i915_ttm_resource_get_st(struct drm_i915_gem_object *obj,
			 struct ttm_resource *res);
void i915_ttm_adjust_lru(struct drm_i915_gem_object *obj);
int i915_ttm_purge(struct drm_i915_gem_object *obj);
static inline bool i915_ttm_gtt_binds_lmem(struct ttm_resource *mem)
{
	return mem->mem_type != I915_PL_SYSTEM;
}
static inline bool i915_ttm_cpu_maps_iomem(struct ttm_resource *mem)
{
	return mem && mem->mem_type != I915_PL_SYSTEM;
}
bool i915_ttm_resource_mappable(struct ttm_resource *res);
#endif
