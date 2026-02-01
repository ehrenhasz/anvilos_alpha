
 

#include <linux/dma-fence.h>
#include <linux/slab.h>

#include <drm/ttm/ttm_bo.h>

#include "i915_deps.h"

 

 
#define I915_DEPS_MIN_ALLOC_CHUNK 8U

static void i915_deps_reset_fences(struct i915_deps *deps)
{
	if (deps->fences != &deps->single)
		kfree(deps->fences);
	deps->num_deps = 0;
	deps->fences_size = 1;
	deps->fences = &deps->single;
}

 
void i915_deps_init(struct i915_deps *deps, gfp_t gfp)
{
	deps->fences = NULL;
	deps->gfp = gfp;
	i915_deps_reset_fences(deps);
}

 
void i915_deps_fini(struct i915_deps *deps)
{
	unsigned int i;

	for (i = 0; i < deps->num_deps; ++i)
		dma_fence_put(deps->fences[i]);

	if (deps->fences != &deps->single)
		kfree(deps->fences);
}

static int i915_deps_grow(struct i915_deps *deps, struct dma_fence *fence,
			  const struct ttm_operation_ctx *ctx)
{
	int ret;

	if (deps->num_deps >= deps->fences_size) {
		unsigned int new_size = 2 * deps->fences_size;
		struct dma_fence **new_fences;

		new_size = max(new_size, I915_DEPS_MIN_ALLOC_CHUNK);
		new_fences = kmalloc_array(new_size, sizeof(*new_fences), deps->gfp);
		if (!new_fences)
			goto sync;

		memcpy(new_fences, deps->fences,
		       deps->fences_size * sizeof(*new_fences));
		swap(new_fences, deps->fences);
		if (new_fences != &deps->single)
			kfree(new_fences);
		deps->fences_size = new_size;
	}
	deps->fences[deps->num_deps++] = dma_fence_get(fence);
	return 0;

sync:
	if (ctx->no_wait_gpu && !dma_fence_is_signaled(fence)) {
		ret = -EBUSY;
		goto unref;
	}

	ret = dma_fence_wait(fence, ctx->interruptible);
	if (ret)
		goto unref;

	ret = fence->error;
	if (ret)
		goto unref;

	return 0;

unref:
	i915_deps_fini(deps);
	return ret;
}

 
int i915_deps_sync(const struct i915_deps *deps, const struct ttm_operation_ctx *ctx)
{
	struct dma_fence **fences = deps->fences;
	unsigned int i;
	int ret = 0;

	for (i = 0; i < deps->num_deps; ++i, ++fences) {
		if (ctx->no_wait_gpu && !dma_fence_is_signaled(*fences)) {
			ret = -EBUSY;
			break;
		}

		ret = dma_fence_wait(*fences, ctx->interruptible);
		if (!ret)
			ret = (*fences)->error;
		if (ret)
			break;
	}

	return ret;
}

 
int i915_deps_add_dependency(struct i915_deps *deps,
			     struct dma_fence *fence,
			     const struct ttm_operation_ctx *ctx)
{
	unsigned int i;
	int ret;

	if (!fence)
		return 0;

	if (dma_fence_is_signaled(fence)) {
		ret = fence->error;
		if (ret)
			i915_deps_fini(deps);
		return ret;
	}

	for (i = 0; i < deps->num_deps; ++i) {
		struct dma_fence *entry = deps->fences[i];

		if (!entry->context || entry->context != fence->context)
			continue;

		if (dma_fence_is_later(fence, entry)) {
			dma_fence_put(entry);
			deps->fences[i] = dma_fence_get(fence);
		}

		return 0;
	}

	return i915_deps_grow(deps, fence, ctx);
}

 
int i915_deps_add_resv(struct i915_deps *deps, struct dma_resv *resv,
		       const struct ttm_operation_ctx *ctx)
{
	struct dma_resv_iter iter;
	struct dma_fence *fence;

	dma_resv_assert_held(resv);
	dma_resv_for_each_fence(&iter, resv, dma_resv_usage_rw(true), fence) {
		int ret = i915_deps_add_dependency(deps, fence, ctx);

		if (ret)
			return ret;
	}

	return 0;
}
