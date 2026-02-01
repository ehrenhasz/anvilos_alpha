 
 

#ifndef __I915_VMA_RESOURCE_H__
#define __I915_VMA_RESOURCE_H__

#include <linux/dma-fence.h>
#include <linux/refcount.h>

#include "i915_gem.h"
#include "i915_scatterlist.h"
#include "i915_sw_fence.h"
#include "intel_runtime_pm.h"

struct intel_memory_region;

struct i915_page_sizes {
	 
	unsigned int phys;

	 
	unsigned int sg;
};

 
struct i915_vma_bindinfo {
	struct sg_table *pages;
	struct i915_page_sizes page_sizes;
	struct i915_refct_sgt *pages_rsgt;
	bool readonly:1;
	bool lmem:1;
};

 
struct i915_vma_resource {
	struct dma_fence unbind_fence;
	 
	spinlock_t lock;
	refcount_t hold_count;
	struct work_struct work;
	struct i915_sw_fence chain;
	struct rb_node rb;
	u64 __subtree_last;
	struct i915_address_space *vm;
	intel_wakeref_t wakeref;

	 
	struct i915_vma_bindinfo bi;

#if IS_ENABLED(CONFIG_DRM_I915_CAPTURE_ERROR)
	struct intel_memory_region *mr;
#endif
	const struct i915_vma_ops *ops;
	void *private;
	u64 start;
	u64 node_size;
	u64 vma_size;
	u32 guard;
	u32 page_sizes_gtt;

	u32 bound_flags;
	bool allocated:1;
	bool immediate_unbind:1;
	bool needs_wakeref:1;
	bool skip_pte_rewrite:1;

	u32 *tlb;
};

bool i915_vma_resource_hold(struct i915_vma_resource *vma_res,
			    bool *lockdep_cookie);

void i915_vma_resource_unhold(struct i915_vma_resource *vma_res,
			      bool lockdep_cookie);

struct i915_vma_resource *i915_vma_resource_alloc(void);

void i915_vma_resource_free(struct i915_vma_resource *vma_res);

struct dma_fence *i915_vma_resource_unbind(struct i915_vma_resource *vma_res,
					   u32 *tlb);

void __i915_vma_resource_init(struct i915_vma_resource *vma_res);

 
static inline struct i915_vma_resource
*i915_vma_resource_get(struct i915_vma_resource *vma_res)
{
	dma_fence_get(&vma_res->unbind_fence);
	return vma_res;
}

 
static inline void i915_vma_resource_put(struct i915_vma_resource *vma_res)
{
	dma_fence_put(&vma_res->unbind_fence);
}

 
static inline void i915_vma_resource_init(struct i915_vma_resource *vma_res,
					  struct i915_address_space *vm,
					  struct sg_table *pages,
					  const struct i915_page_sizes *page_sizes,
					  struct i915_refct_sgt *pages_rsgt,
					  bool readonly,
					  bool lmem,
					  struct intel_memory_region *mr,
					  const struct i915_vma_ops *ops,
					  void *private,
					  u64 start,
					  u64 node_size,
					  u64 size,
					  u32 guard)
{
	__i915_vma_resource_init(vma_res);
	vma_res->vm = vm;
	vma_res->bi.pages = pages;
	vma_res->bi.page_sizes = *page_sizes;
	if (pages_rsgt)
		vma_res->bi.pages_rsgt = i915_refct_sgt_get(pages_rsgt);
	vma_res->bi.readonly = readonly;
	vma_res->bi.lmem = lmem;
#if IS_ENABLED(CONFIG_DRM_I915_CAPTURE_ERROR)
	vma_res->mr = mr;
#endif
	vma_res->ops = ops;
	vma_res->private = private;
	vma_res->start = start;
	vma_res->node_size = node_size;
	vma_res->vma_size = size;
	vma_res->guard = guard;
}

static inline void i915_vma_resource_fini(struct i915_vma_resource *vma_res)
{
	GEM_BUG_ON(refcount_read(&vma_res->hold_count) != 1);
	if (vma_res->bi.pages_rsgt)
		i915_refct_sgt_put(vma_res->bi.pages_rsgt);
	i915_sw_fence_fini(&vma_res->chain);
}

int i915_vma_resource_bind_dep_sync(struct i915_address_space *vm,
				    u64 first,
				    u64 last,
				    bool intr);

int i915_vma_resource_bind_dep_await(struct i915_address_space *vm,
				     struct i915_sw_fence *sw_fence,
				     u64 first,
				     u64 last,
				     bool intr,
				     gfp_t gfp);

void i915_vma_resource_bind_dep_sync_all(struct i915_address_space *vm);

void i915_vma_resource_module_exit(void);

int i915_vma_resource_module_init(void);

#endif
