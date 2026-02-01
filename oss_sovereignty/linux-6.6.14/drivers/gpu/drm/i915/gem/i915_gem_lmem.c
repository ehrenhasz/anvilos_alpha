
 

#include <uapi/drm/i915_drm.h>

#include "intel_memory_region.h"
#include "gem/i915_gem_region.h"
#include "gem/i915_gem_lmem.h"
#include "i915_drv.h"

void __iomem *
i915_gem_object_lmem_io_map(struct drm_i915_gem_object *obj,
			    unsigned long n,
			    unsigned long size)
{
	resource_size_t offset;

	GEM_BUG_ON(!i915_gem_object_is_contiguous(obj));

	offset = i915_gem_object_get_dma_address(obj, n);
	offset -= obj->mm.region->region.start;

	return io_mapping_map_wc(&obj->mm.region->iomap, offset, size);
}

 
bool i915_gem_object_is_lmem(struct drm_i915_gem_object *obj)
{
	struct intel_memory_region *mr = READ_ONCE(obj->mm.region);

#ifdef CONFIG_LOCKDEP
	if (i915_gem_object_migratable(obj) &&
	    i915_gem_object_evictable(obj))
		assert_object_held(obj);
#endif
	return mr && (mr->type == INTEL_MEMORY_LOCAL ||
		      mr->type == INTEL_MEMORY_STOLEN_LOCAL);
}

 
bool __i915_gem_object_is_lmem(struct drm_i915_gem_object *obj)
{
	struct intel_memory_region *mr = READ_ONCE(obj->mm.region);

#ifdef CONFIG_LOCKDEP
	GEM_WARN_ON(dma_resv_test_signaled(obj->base.resv, DMA_RESV_USAGE_BOOKKEEP) &&
		    i915_gem_object_evictable(obj));
#endif
	return mr && (mr->type == INTEL_MEMORY_LOCAL ||
		      mr->type == INTEL_MEMORY_STOLEN_LOCAL);
}

 
struct drm_i915_gem_object *
__i915_gem_object_create_lmem_with_ps(struct drm_i915_private *i915,
				      resource_size_t size,
				      resource_size_t page_size,
				      unsigned int flags)
{
	return i915_gem_object_create_region(i915->mm.regions[INTEL_REGION_LMEM_0],
					     size, page_size, flags);
}

struct drm_i915_gem_object *
i915_gem_object_create_lmem_from_data(struct drm_i915_private *i915,
				      const void *data, size_t size)
{
	struct drm_i915_gem_object *obj;
	void *map;

	obj = i915_gem_object_create_lmem(i915,
					  round_up(size, PAGE_SIZE),
					  I915_BO_ALLOC_CONTIGUOUS);
	if (IS_ERR(obj))
		return obj;

	map = i915_gem_object_pin_map_unlocked(obj, I915_MAP_WC);
	if (IS_ERR(map)) {
		i915_gem_object_put(obj);
		return map;
	}

	memcpy(map, data, size);

	i915_gem_object_flush_map(obj);
	__i915_gem_object_release_map(obj);

	return obj;
}

struct drm_i915_gem_object *
i915_gem_object_create_lmem(struct drm_i915_private *i915,
			    resource_size_t size,
			    unsigned int flags)
{
	return i915_gem_object_create_region(i915->mm.regions[INTEL_REGION_LMEM_0],
					     size, 0, flags);
}
