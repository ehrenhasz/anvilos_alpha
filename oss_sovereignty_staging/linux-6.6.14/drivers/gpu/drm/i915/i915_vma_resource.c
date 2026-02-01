
 

#include <linux/interval_tree_generic.h>
#include <linux/sched/mm.h>

#include "i915_sw_fence.h"
#include "i915_vma_resource.h"
#include "i915_drv.h"
#include "intel_memory_region.h"

#include "gt/intel_gtt.h"

static struct kmem_cache *slab_vma_resources;

 
#define VMA_RES_START(_node) ((_node)->start - (_node)->guard)
#define VMA_RES_LAST(_node) ((_node)->start + (_node)->node_size + (_node)->guard - 1)
INTERVAL_TREE_DEFINE(struct i915_vma_resource, rb,
		     u64, __subtree_last,
		     VMA_RES_START, VMA_RES_LAST, static, vma_res_itree);

 

 
struct i915_vma_resource *i915_vma_resource_alloc(void)
{
	struct i915_vma_resource *vma_res =
		kmem_cache_zalloc(slab_vma_resources, GFP_KERNEL);

	return vma_res ? vma_res : ERR_PTR(-ENOMEM);
}

 
void i915_vma_resource_free(struct i915_vma_resource *vma_res)
{
	if (vma_res)
		kmem_cache_free(slab_vma_resources, vma_res);
}

static const char *get_driver_name(struct dma_fence *fence)
{
	return "vma unbind fence";
}

static const char *get_timeline_name(struct dma_fence *fence)
{
	return "unbound";
}

static void unbind_fence_free_rcu(struct rcu_head *head)
{
	struct i915_vma_resource *vma_res =
		container_of(head, typeof(*vma_res), unbind_fence.rcu);

	i915_vma_resource_free(vma_res);
}

static void unbind_fence_release(struct dma_fence *fence)
{
	struct i915_vma_resource *vma_res =
		container_of(fence, typeof(*vma_res), unbind_fence);

	i915_sw_fence_fini(&vma_res->chain);

	call_rcu(&fence->rcu, unbind_fence_free_rcu);
}

static struct dma_fence_ops unbind_fence_ops = {
	.get_driver_name = get_driver_name,
	.get_timeline_name = get_timeline_name,
	.release = unbind_fence_release,
};

static void __i915_vma_resource_unhold(struct i915_vma_resource *vma_res)
{
	struct i915_address_space *vm;

	if (!refcount_dec_and_test(&vma_res->hold_count))
		return;

	dma_fence_signal(&vma_res->unbind_fence);

	vm = vma_res->vm;
	if (vma_res->wakeref)
		intel_runtime_pm_put(&vm->i915->runtime_pm, vma_res->wakeref);

	vma_res->vm = NULL;
	if (!RB_EMPTY_NODE(&vma_res->rb)) {
		mutex_lock(&vm->mutex);
		vma_res_itree_remove(vma_res, &vm->pending_unbind);
		mutex_unlock(&vm->mutex);
	}

	if (vma_res->bi.pages_rsgt)
		i915_refct_sgt_put(vma_res->bi.pages_rsgt);
}

 
void i915_vma_resource_unhold(struct i915_vma_resource *vma_res,
			      bool lockdep_cookie)
{
	dma_fence_end_signalling(lockdep_cookie);

	if (IS_ENABLED(CONFIG_PROVE_LOCKING)) {
		unsigned long irq_flags;

		 
		spin_lock_irqsave(&vma_res->lock, irq_flags);
		spin_unlock_irqrestore(&vma_res->lock, irq_flags);
	}

	__i915_vma_resource_unhold(vma_res);
}

 
bool i915_vma_resource_hold(struct i915_vma_resource *vma_res,
			    bool *lockdep_cookie)
{
	bool held = refcount_inc_not_zero(&vma_res->hold_count);

	if (held)
		*lockdep_cookie = dma_fence_begin_signalling();

	return held;
}

static void i915_vma_resource_unbind_work(struct work_struct *work)
{
	struct i915_vma_resource *vma_res =
		container_of(work, typeof(*vma_res), work);
	struct i915_address_space *vm = vma_res->vm;
	bool lockdep_cookie;

	lockdep_cookie = dma_fence_begin_signalling();
	if (likely(!vma_res->skip_pte_rewrite))
		vma_res->ops->unbind_vma(vm, vma_res);

	dma_fence_end_signalling(lockdep_cookie);
	__i915_vma_resource_unhold(vma_res);
	i915_vma_resource_put(vma_res);
}

static int
i915_vma_resource_fence_notify(struct i915_sw_fence *fence,
			       enum i915_sw_fence_notify state)
{
	struct i915_vma_resource *vma_res =
		container_of(fence, typeof(*vma_res), chain);
	struct dma_fence *unbind_fence =
		&vma_res->unbind_fence;

	switch (state) {
	case FENCE_COMPLETE:
		dma_fence_get(unbind_fence);
		if (vma_res->immediate_unbind) {
			i915_vma_resource_unbind_work(&vma_res->work);
		} else {
			INIT_WORK(&vma_res->work, i915_vma_resource_unbind_work);
			queue_work(system_unbound_wq, &vma_res->work);
		}
		break;
	case FENCE_FREE:
		i915_vma_resource_put(vma_res);
		break;
	}

	return NOTIFY_DONE;
}

 
struct dma_fence *i915_vma_resource_unbind(struct i915_vma_resource *vma_res,
					   u32 *tlb)
{
	struct i915_address_space *vm = vma_res->vm;

	vma_res->tlb = tlb;

	 
	i915_vma_resource_get(vma_res);

	 
	if (vma_res->needs_wakeref)
		vma_res->wakeref = intel_runtime_pm_get_if_in_use(&vm->i915->runtime_pm);

	if (atomic_read(&vma_res->chain.pending) <= 1) {
		RB_CLEAR_NODE(&vma_res->rb);
		vma_res->immediate_unbind = 1;
	} else {
		vma_res_itree_insert(vma_res, &vma_res->vm->pending_unbind);
	}

	i915_sw_fence_commit(&vma_res->chain);

	return &vma_res->unbind_fence;
}

 
void __i915_vma_resource_init(struct i915_vma_resource *vma_res)
{
	spin_lock_init(&vma_res->lock);
	dma_fence_init(&vma_res->unbind_fence, &unbind_fence_ops,
		       &vma_res->lock, 0, 0);
	refcount_set(&vma_res->hold_count, 1);
	i915_sw_fence_init(&vma_res->chain, i915_vma_resource_fence_notify);
}

static void
i915_vma_resource_color_adjust_range(struct i915_address_space *vm,
				     u64 *start,
				     u64 *end)
{
	if (i915_vm_has_cache_coloring(vm)) {
		if (*start)
			*start -= I915_GTT_PAGE_SIZE;
		*end += I915_GTT_PAGE_SIZE;
	}
}

 
int i915_vma_resource_bind_dep_sync(struct i915_address_space *vm,
				    u64 offset,
				    u64 size,
				    bool intr)
{
	struct i915_vma_resource *node;
	u64 last = offset + size - 1;

	lockdep_assert_held(&vm->mutex);
	might_sleep();

	i915_vma_resource_color_adjust_range(vm, &offset, &last);
	node = vma_res_itree_iter_first(&vm->pending_unbind, offset, last);
	while (node) {
		int ret = dma_fence_wait(&node->unbind_fence, intr);

		if (ret)
			return ret;

		node = vma_res_itree_iter_next(node, offset, last);
	}

	return 0;
}

 
void i915_vma_resource_bind_dep_sync_all(struct i915_address_space *vm)
{
	struct i915_vma_resource *node;
	struct dma_fence *fence;

	do {
		fence = NULL;
		mutex_lock(&vm->mutex);
		node = vma_res_itree_iter_first(&vm->pending_unbind, 0,
						U64_MAX);
		if (node)
			fence = dma_fence_get_rcu(&node->unbind_fence);
		mutex_unlock(&vm->mutex);

		if (fence) {
			 
			dma_fence_wait(fence, false);
			dma_fence_put(fence);
		}
	} while (node);
}

 
int i915_vma_resource_bind_dep_await(struct i915_address_space *vm,
				     struct i915_sw_fence *sw_fence,
				     u64 offset,
				     u64 size,
				     bool intr,
				     gfp_t gfp)
{
	struct i915_vma_resource *node;
	u64 last = offset + size - 1;

	lockdep_assert_held(&vm->mutex);
	might_alloc(gfp);
	might_sleep();

	i915_vma_resource_color_adjust_range(vm, &offset, &last);
	node = vma_res_itree_iter_first(&vm->pending_unbind, offset, last);
	while (node) {
		int ret;

		ret = i915_sw_fence_await_dma_fence(sw_fence,
						    &node->unbind_fence,
						    0, gfp);
		if (ret < 0) {
			ret = dma_fence_wait(&node->unbind_fence, intr);
			if (ret)
				return ret;
		}

		node = vma_res_itree_iter_next(node, offset, last);
	}

	return 0;
}

void i915_vma_resource_module_exit(void)
{
	kmem_cache_destroy(slab_vma_resources);
}

int __init i915_vma_resource_module_init(void)
{
	slab_vma_resources = KMEM_CACHE(i915_vma_resource, SLAB_HWCACHE_ALIGN);
	if (!slab_vma_resources)
		return -ENOMEM;

	return 0;
}
