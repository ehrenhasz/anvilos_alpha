 

#include <linux/dma-fence-array.h>

#include "gt/intel_engine.h"

#include "i915_gem_ioctls.h"
#include "i915_gem_object.h"

static __always_inline u32 __busy_read_flag(u16 id)
{
	if (id == (u16)I915_ENGINE_CLASS_INVALID)
		return 0xffff0000u;

	GEM_BUG_ON(id >= 16);
	return 0x10000u << id;
}

static __always_inline u32 __busy_write_id(u16 id)
{
	 
	if (id == (u16)I915_ENGINE_CLASS_INVALID)
		return 0xffffffffu;

	return (id + 1) | __busy_read_flag(id);
}

static __always_inline unsigned int
__busy_set_if_active(struct dma_fence *fence, u32 (*flag)(u16 id))
{
	const struct i915_request *rq;

	 
	if (dma_fence_is_array(fence)) {
		struct dma_fence_array *array = to_dma_fence_array(fence);
		struct dma_fence **child = array->fences;
		unsigned int nchild = array->num_fences;

		do {
			struct dma_fence *current_fence = *child++;

			 
			if (!dma_fence_is_i915(current_fence) ||
			    !test_bit(I915_FENCE_FLAG_COMPOSITE,
				      &current_fence->flags)) {
				return 0;
			}

			rq = to_request(current_fence);
			if (!i915_request_completed(rq))
				return flag(rq->engine->uabi_class);
		} while (--nchild);

		 
		return 0;
	} else {
		if (!dma_fence_is_i915(fence))
			return 0;

		rq = to_request(fence);
		if (i915_request_completed(rq))
			return 0;

		 
		BUILD_BUG_ON(!typecheck(u16, rq->engine->uabi_class));
		return flag(rq->engine->uabi_class);
	}
}

static __always_inline unsigned int
busy_check_reader(struct dma_fence *fence)
{
	return __busy_set_if_active(fence, __busy_read_flag);
}

static __always_inline unsigned int
busy_check_writer(struct dma_fence *fence)
{
	if (!fence)
		return 0;

	return __busy_set_if_active(fence, __busy_write_id);
}

int
i915_gem_busy_ioctl(struct drm_device *dev, void *data,
		    struct drm_file *file)
{
	struct drm_i915_gem_busy *args = data;
	struct drm_i915_gem_object *obj;
	struct dma_resv_iter cursor;
	struct dma_fence *fence;
	int err;

	err = -ENOENT;
	rcu_read_lock();
	obj = i915_gem_object_lookup_rcu(file, args->handle);
	if (!obj)
		goto out;

	 
	args->busy = 0;
	dma_resv_iter_begin(&cursor, obj->base.resv, DMA_RESV_USAGE_READ);
	dma_resv_for_each_fence_unlocked(&cursor, fence) {
		if (dma_resv_iter_is_restarted(&cursor))
			args->busy = 0;

		if (dma_resv_iter_usage(&cursor) <= DMA_RESV_USAGE_WRITE)
			 
			args->busy |= busy_check_writer(fence);
		else
			 
			args->busy |= busy_check_reader(fence);
	}
	dma_resv_iter_end(&cursor);

	err = 0;
out:
	rcu_read_unlock();
	return err;
}
