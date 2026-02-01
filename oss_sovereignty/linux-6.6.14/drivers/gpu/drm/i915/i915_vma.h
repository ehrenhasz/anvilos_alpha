 

#ifndef __I915_VMA_H__
#define __I915_VMA_H__

#include <linux/io-mapping.h>
#include <linux/rbtree.h>

#include <drm/drm_mm.h>

#include "gt/intel_ggtt_fencing.h"
#include "gem/i915_gem_object.h"

#include "i915_gem_gtt.h"

#include "i915_active.h"
#include "i915_request.h"
#include "i915_vma_resource.h"
#include "i915_vma_types.h"

struct i915_vma *
i915_vma_instance(struct drm_i915_gem_object *obj,
		  struct i915_address_space *vm,
		  const struct i915_gtt_view *view);

void i915_vma_unpin_and_release(struct i915_vma **p_vma, unsigned int flags);
#define I915_VMA_RELEASE_MAP BIT(0)

static inline bool i915_vma_is_active(const struct i915_vma *vma)
{
	return !i915_active_is_idle(&vma->active);
}

 
#define __EXEC_OBJECT_NO_RESERVE BIT(31)
#define __EXEC_OBJECT_NO_REQUEST_AWAIT BIT(30)

int __must_check _i915_vma_move_to_active(struct i915_vma *vma,
					  struct i915_request *rq,
					  struct dma_fence *fence,
					  unsigned int flags);
static inline int __must_check
i915_vma_move_to_active(struct i915_vma *vma, struct i915_request *rq,
			unsigned int flags)
{
	return _i915_vma_move_to_active(vma, rq, &rq->fence, flags);
}

#define __i915_vma_flags(v) ((unsigned long *)&(v)->flags.counter)

static inline bool i915_vma_is_ggtt(const struct i915_vma *vma)
{
	return test_bit(I915_VMA_GGTT_BIT, __i915_vma_flags(vma));
}

static inline bool i915_vma_is_dpt(const struct i915_vma *vma)
{
	return i915_is_dpt(vma->vm);
}

static inline bool i915_vma_has_ggtt_write(const struct i915_vma *vma)
{
	return test_bit(I915_VMA_GGTT_WRITE_BIT, __i915_vma_flags(vma));
}

static inline void i915_vma_set_ggtt_write(struct i915_vma *vma)
{
	GEM_BUG_ON(!i915_vma_is_ggtt(vma));
	set_bit(I915_VMA_GGTT_WRITE_BIT, __i915_vma_flags(vma));
}

static inline bool i915_vma_unset_ggtt_write(struct i915_vma *vma)
{
	return test_and_clear_bit(I915_VMA_GGTT_WRITE_BIT,
				  __i915_vma_flags(vma));
}

void i915_vma_flush_writes(struct i915_vma *vma);

static inline bool i915_vma_is_map_and_fenceable(const struct i915_vma *vma)
{
	return test_bit(I915_VMA_CAN_FENCE_BIT, __i915_vma_flags(vma));
}

static inline bool i915_vma_set_userfault(struct i915_vma *vma)
{
	GEM_BUG_ON(!i915_vma_is_map_and_fenceable(vma));
	return test_and_set_bit(I915_VMA_USERFAULT_BIT, __i915_vma_flags(vma));
}

static inline void i915_vma_unset_userfault(struct i915_vma *vma)
{
	return clear_bit(I915_VMA_USERFAULT_BIT, __i915_vma_flags(vma));
}

static inline bool i915_vma_has_userfault(const struct i915_vma *vma)
{
	return test_bit(I915_VMA_USERFAULT_BIT, __i915_vma_flags(vma));
}

static inline bool i915_vma_is_closed(const struct i915_vma *vma)
{
	return !list_empty(&vma->closed_link);
}

 
static inline u64 __i915_vma_size(const struct i915_vma *vma)
{
	return vma->node.size - 2 * vma->guard;
}

 
static inline u64 i915_vma_size(const struct i915_vma *vma)
{
	GEM_BUG_ON(!drm_mm_node_allocated(&vma->node));
	return __i915_vma_size(vma);
}

 
static inline u64 __i915_vma_offset(const struct i915_vma *vma)
{
	 
	return vma->node.start + vma->guard;
}

 
static inline u64 i915_vma_offset(const struct i915_vma *vma)
{
	GEM_BUG_ON(!drm_mm_node_allocated(&vma->node));
	return __i915_vma_offset(vma);
}

static inline u32 i915_ggtt_offset(const struct i915_vma *vma)
{
	GEM_BUG_ON(!i915_vma_is_ggtt(vma));
	GEM_BUG_ON(!drm_mm_node_allocated(&vma->node));
	GEM_BUG_ON(upper_32_bits(i915_vma_offset(vma)));
	GEM_BUG_ON(upper_32_bits(i915_vma_offset(vma) +
				 i915_vma_size(vma) - 1));
	return lower_32_bits(i915_vma_offset(vma));
}

static inline u32 i915_ggtt_pin_bias(struct i915_vma *vma)
{
	return i915_vm_to_ggtt(vma->vm)->pin_bias;
}

static inline struct i915_vma *i915_vma_get(struct i915_vma *vma)
{
	i915_gem_object_get(vma->obj);
	return vma;
}

static inline struct i915_vma *i915_vma_tryget(struct i915_vma *vma)
{
	if (likely(kref_get_unless_zero(&vma->obj->base.refcount)))
		return vma;

	return NULL;
}

static inline void i915_vma_put(struct i915_vma *vma)
{
	i915_gem_object_put(vma->obj);
}

static inline long
i915_vma_compare(struct i915_vma *vma,
		 struct i915_address_space *vm,
		 const struct i915_gtt_view *view)
{
	ptrdiff_t cmp;

	GEM_BUG_ON(view && !i915_is_ggtt_or_dpt(vm));

	cmp = ptrdiff(vma->vm, vm);
	if (cmp)
		return cmp;

	BUILD_BUG_ON(I915_GTT_VIEW_NORMAL != 0);
	cmp = vma->gtt_view.type;
	if (!view)
		return cmp;

	cmp -= view->type;
	if (cmp)
		return cmp;

	assert_i915_gem_gtt_types();

	 
	BUILD_BUG_ON(I915_GTT_VIEW_NORMAL >= I915_GTT_VIEW_PARTIAL);
	BUILD_BUG_ON(I915_GTT_VIEW_PARTIAL >= I915_GTT_VIEW_ROTATED);
	BUILD_BUG_ON(I915_GTT_VIEW_ROTATED >= I915_GTT_VIEW_REMAPPED);
	BUILD_BUG_ON(offsetof(typeof(*view), rotated) !=
		     offsetof(typeof(*view), partial));
	BUILD_BUG_ON(offsetof(typeof(*view), rotated) !=
		     offsetof(typeof(*view), remapped));
	return memcmp(&vma->gtt_view.partial, &view->partial, view->type);
}

struct i915_vma_work *i915_vma_work(void);
int i915_vma_bind(struct i915_vma *vma,
		  unsigned int pat_index,
		  u32 flags,
		  struct i915_vma_work *work,
		  struct i915_vma_resource *vma_res);

bool i915_gem_valid_gtt_space(struct i915_vma *vma, unsigned long color);
bool i915_vma_misplaced(const struct i915_vma *vma,
			u64 size, u64 alignment, u64 flags);
void __i915_vma_set_map_and_fenceable(struct i915_vma *vma);
void i915_vma_revoke_mmap(struct i915_vma *vma);
void vma_invalidate_tlb(struct i915_address_space *vm, u32 *tlb);
struct dma_fence *__i915_vma_evict(struct i915_vma *vma, bool async);
int __i915_vma_unbind(struct i915_vma *vma);
int __must_check i915_vma_unbind(struct i915_vma *vma);
int __must_check i915_vma_unbind_async(struct i915_vma *vma, bool trylock_vm);
int __must_check i915_vma_unbind_unlocked(struct i915_vma *vma);
void i915_vma_unlink_ctx(struct i915_vma *vma);
void i915_vma_close(struct i915_vma *vma);
void i915_vma_reopen(struct i915_vma *vma);

void i915_vma_destroy_locked(struct i915_vma *vma);
void i915_vma_destroy(struct i915_vma *vma);

#define assert_vma_held(vma) dma_resv_assert_held((vma)->obj->base.resv)

static inline void i915_vma_lock(struct i915_vma *vma)
{
	dma_resv_lock(vma->obj->base.resv, NULL);
}

static inline void i915_vma_unlock(struct i915_vma *vma)
{
	dma_resv_unlock(vma->obj->base.resv);
}

int __must_check
i915_vma_pin_ww(struct i915_vma *vma, struct i915_gem_ww_ctx *ww,
		u64 size, u64 alignment, u64 flags);

static inline int __must_check
i915_vma_pin(struct i915_vma *vma, u64 size, u64 alignment, u64 flags)
{
	struct i915_gem_ww_ctx ww;
	int err;

	i915_gem_ww_ctx_init(&ww, true);
retry:
	err = i915_gem_object_lock(vma->obj, &ww);
	if (!err)
		err = i915_vma_pin_ww(vma, &ww, size, alignment, flags);
	if (err == -EDEADLK) {
		err = i915_gem_ww_ctx_backoff(&ww);
		if (!err)
			goto retry;
	}
	i915_gem_ww_ctx_fini(&ww);

	return err;
}

int i915_ggtt_pin(struct i915_vma *vma, struct i915_gem_ww_ctx *ww,
		  u32 align, unsigned int flags);

static inline int i915_vma_pin_count(const struct i915_vma *vma)
{
	return atomic_read(&vma->flags) & I915_VMA_PIN_MASK;
}

static inline bool i915_vma_is_pinned(const struct i915_vma *vma)
{
	return i915_vma_pin_count(vma);
}

static inline void __i915_vma_pin(struct i915_vma *vma)
{
	atomic_inc(&vma->flags);
	GEM_BUG_ON(!i915_vma_is_pinned(vma));
}

static inline void __i915_vma_unpin(struct i915_vma *vma)
{
	GEM_BUG_ON(!i915_vma_is_pinned(vma));
	atomic_dec(&vma->flags);
}

static inline void i915_vma_unpin(struct i915_vma *vma)
{
	GEM_BUG_ON(!drm_mm_node_allocated(&vma->node));
	__i915_vma_unpin(vma);
}

static inline bool i915_vma_is_bound(const struct i915_vma *vma,
				     unsigned int where)
{
	return atomic_read(&vma->flags) & where;
}

static inline bool i915_node_color_differs(const struct drm_mm_node *node,
					   unsigned long color)
{
	return drm_mm_node_allocated(node) && node->color != color;
}

 
void __iomem *i915_vma_pin_iomap(struct i915_vma *vma);

 
void i915_vma_unpin_iomap(struct i915_vma *vma);

 
int __must_check i915_vma_pin_fence(struct i915_vma *vma);
void i915_vma_revoke_fence(struct i915_vma *vma);

int __i915_vma_pin_fence(struct i915_vma *vma);

static inline void __i915_vma_unpin_fence(struct i915_vma *vma)
{
	GEM_BUG_ON(atomic_read(&vma->fence->pin_count) <= 0);
	atomic_dec(&vma->fence->pin_count);
}

 
static inline void
i915_vma_unpin_fence(struct i915_vma *vma)
{
	if (vma->fence)
		__i915_vma_unpin_fence(vma);
}

static inline int i915_vma_fence_id(const struct i915_vma *vma)
{
	return vma->fence ? vma->fence->id : -1;
}

void i915_vma_parked(struct intel_gt *gt);

static inline bool i915_vma_is_scanout(const struct i915_vma *vma)
{
	return test_bit(I915_VMA_SCANOUT_BIT, __i915_vma_flags(vma));
}

static inline void i915_vma_mark_scanout(struct i915_vma *vma)
{
	set_bit(I915_VMA_SCANOUT_BIT, __i915_vma_flags(vma));
}

static inline void i915_vma_clear_scanout(struct i915_vma *vma)
{
	clear_bit(I915_VMA_SCANOUT_BIT, __i915_vma_flags(vma));
}

void i915_ggtt_clear_scanout(struct drm_i915_gem_object *obj);

#define for_each_until(cond) if (cond) break; else

 
#define for_each_ggtt_vma(V, OBJ) \
	list_for_each_entry(V, &(OBJ)->vma.list, obj_link)		\
		for_each_until(!i915_vma_is_ggtt(V))

struct i915_vma *i915_vma_make_unshrinkable(struct i915_vma *vma);
void i915_vma_make_shrinkable(struct i915_vma *vma);
void i915_vma_make_purgeable(struct i915_vma *vma);

int i915_vma_wait_for_bind(struct i915_vma *vma);

static inline int i915_vma_sync(struct i915_vma *vma)
{
	 
	return i915_active_wait(&vma->active);
}

 
static inline struct i915_vma_resource *
i915_vma_get_current_resource(struct i915_vma *vma)
{
	return i915_vma_resource_get(vma->resource);
}

#if IS_ENABLED(CONFIG_DRM_I915_SELFTEST)
void i915_vma_resource_init_from_vma(struct i915_vma_resource *vma_res,
				     struct i915_vma *vma);
#endif

void i915_vma_module_exit(void);
int i915_vma_module_init(void);

I915_SELFTEST_DECLARE(int i915_vma_get_pages(struct i915_vma *vma));
I915_SELFTEST_DECLARE(void i915_vma_put_pages(struct i915_vma *vma));

#endif
