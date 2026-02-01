
 

#include <linux/slab.h>  

#include <linux/fault-inject.h>
#include <linux/log2.h>
#include <linux/random.h>
#include <linux/seq_file.h>
#include <linux/stop_machine.h>

#include <asm/set_memory.h>
#include <asm/smp.h>

#include "display/intel_frontbuffer.h"
#include "gt/intel_gt.h"
#include "gt/intel_gt_requests.h"

#include "i915_drv.h"
#include "i915_gem_evict.h"
#include "i915_scatterlist.h"
#include "i915_trace.h"
#include "i915_vgpu.h"

int i915_gem_gtt_prepare_pages(struct drm_i915_gem_object *obj,
			       struct sg_table *pages)
{
	do {
		if (dma_map_sg_attrs(obj->base.dev->dev,
				     pages->sgl, pages->nents,
				     DMA_BIDIRECTIONAL,
				     DMA_ATTR_SKIP_CPU_SYNC |
				     DMA_ATTR_NO_KERNEL_MAPPING |
				     DMA_ATTR_NO_WARN))
			return 0;

		 
		GEM_BUG_ON(obj->mm.pages == pages);
	} while (i915_gem_shrink(NULL, to_i915(obj->base.dev),
				 obj->base.size >> PAGE_SHIFT, NULL,
				 I915_SHRINK_BOUND |
				 I915_SHRINK_UNBOUND));

	return -ENOSPC;
}

void i915_gem_gtt_finish_pages(struct drm_i915_gem_object *obj,
			       struct sg_table *pages)
{
	struct drm_i915_private *i915 = to_i915(obj->base.dev);
	struct i915_ggtt *ggtt = to_gt(i915)->ggtt;

	 
	if (unlikely(ggtt->do_idle_maps))
		 
		usleep_range(100, 250);

	dma_unmap_sg(i915->drm.dev, pages->sgl, pages->nents,
		     DMA_BIDIRECTIONAL);
}

 
int i915_gem_gtt_reserve(struct i915_address_space *vm,
			 struct i915_gem_ww_ctx *ww,
			 struct drm_mm_node *node,
			 u64 size, u64 offset, unsigned long color,
			 unsigned int flags)
{
	int err;

	GEM_BUG_ON(!size);
	GEM_BUG_ON(!IS_ALIGNED(size, I915_GTT_PAGE_SIZE));
	GEM_BUG_ON(!IS_ALIGNED(offset, I915_GTT_MIN_ALIGNMENT));
	GEM_BUG_ON(range_overflows(offset, size, vm->total));
	GEM_BUG_ON(vm == &to_gt(vm->i915)->ggtt->alias->vm);
	GEM_BUG_ON(drm_mm_node_allocated(node));

	node->size = size;
	node->start = offset;
	node->color = color;

	err = drm_mm_reserve_node(&vm->mm, node);
	if (err != -ENOSPC)
		return err;

	if (flags & PIN_NOEVICT)
		return -ENOSPC;

	err = i915_gem_evict_for_node(vm, ww, node, flags);
	if (err == 0)
		err = drm_mm_reserve_node(&vm->mm, node);

	return err;
}

static u64 random_offset(u64 start, u64 end, u64 len, u64 align)
{
	u64 range, addr;

	GEM_BUG_ON(range_overflows(start, len, end));
	GEM_BUG_ON(round_up(start, align) > round_down(end - len, align));

	range = round_down(end - len, align) - round_up(start, align);
	if (range) {
		if (sizeof(unsigned long) == sizeof(u64)) {
			addr = get_random_u64();
		} else {
			addr = get_random_u32();
			if (range > U32_MAX) {
				addr <<= 32;
				addr |= get_random_u32();
			}
		}
		div64_u64_rem(addr, range, &addr);
		start += addr;
	}

	return round_up(start, align);
}

 
int i915_gem_gtt_insert(struct i915_address_space *vm,
			struct i915_gem_ww_ctx *ww,
			struct drm_mm_node *node,
			u64 size, u64 alignment, unsigned long color,
			u64 start, u64 end, unsigned int flags)
{
	enum drm_mm_insert_mode mode;
	u64 offset;
	int err;

	lockdep_assert_held(&vm->mutex);

	GEM_BUG_ON(!size);
	GEM_BUG_ON(!IS_ALIGNED(size, I915_GTT_PAGE_SIZE));
	GEM_BUG_ON(alignment && !is_power_of_2(alignment));
	GEM_BUG_ON(alignment && !IS_ALIGNED(alignment, I915_GTT_MIN_ALIGNMENT));
	GEM_BUG_ON(start >= end);
	GEM_BUG_ON(start > 0  && !IS_ALIGNED(start, I915_GTT_PAGE_SIZE));
	GEM_BUG_ON(end < U64_MAX && !IS_ALIGNED(end, I915_GTT_PAGE_SIZE));
	GEM_BUG_ON(vm == &to_gt(vm->i915)->ggtt->alias->vm);
	GEM_BUG_ON(drm_mm_node_allocated(node));

	if (unlikely(range_overflows(start, size, end)))
		return -ENOSPC;

	if (unlikely(round_up(start, alignment) > round_down(end - size, alignment)))
		return -ENOSPC;

	mode = DRM_MM_INSERT_BEST;
	if (flags & PIN_HIGH)
		mode = DRM_MM_INSERT_HIGHEST;
	if (flags & PIN_MAPPABLE)
		mode = DRM_MM_INSERT_LOW;

	 
	BUILD_BUG_ON(I915_GTT_MIN_ALIGNMENT > I915_GTT_PAGE_SIZE);
	if (alignment <= I915_GTT_MIN_ALIGNMENT)
		alignment = 0;

	err = drm_mm_insert_node_in_range(&vm->mm, node,
					  size, alignment, color,
					  start, end, mode);
	if (err != -ENOSPC)
		return err;

	if (mode & DRM_MM_INSERT_ONCE) {
		err = drm_mm_insert_node_in_range(&vm->mm, node,
						  size, alignment, color,
						  start, end,
						  DRM_MM_INSERT_BEST);
		if (err != -ENOSPC)
			return err;
	}

	if (flags & PIN_NOEVICT)
		return -ENOSPC;

	 
	offset = random_offset(start, end,
			       size, alignment ?: I915_GTT_MIN_ALIGNMENT);
	err = i915_gem_gtt_reserve(vm, ww, node, size, offset, color, flags);
	if (err != -ENOSPC)
		return err;

	if (flags & PIN_NOSEARCH)
		return -ENOSPC;

	 
	err = i915_gem_evict_something(vm, ww, size, alignment, color,
				       start, end, flags);
	if (err)
		return err;

	return drm_mm_insert_node_in_range(&vm->mm, node,
					   size, alignment, color,
					   start, end, DRM_MM_INSERT_EVICT);
}

#if IS_ENABLED(CONFIG_DRM_I915_SELFTEST)
#include "selftests/i915_gem_gtt.c"
#endif
