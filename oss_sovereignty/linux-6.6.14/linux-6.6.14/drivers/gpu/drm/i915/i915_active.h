#ifndef _I915_ACTIVE_H_
#define _I915_ACTIVE_H_
#include <linux/lockdep.h>
#include "i915_active_types.h"
#include "i915_request.h"
struct i915_request;
struct intel_engine_cs;
struct intel_timeline;
void i915_active_noop(struct dma_fence *fence, struct dma_fence_cb *cb);
static inline void
__i915_active_fence_init(struct i915_active_fence *active,
			 void *fence,
			 dma_fence_func_t fn)
{
	RCU_INIT_POINTER(active->fence, fence);
	active->cb.func = fn ?: i915_active_noop;
}
#define INIT_ACTIVE_FENCE(A) \
	__i915_active_fence_init((A), NULL, NULL)
struct dma_fence *
__i915_active_fence_set(struct i915_active_fence *active,
			struct dma_fence *fence);
int __must_check
i915_active_fence_set(struct i915_active_fence *active,
		      struct i915_request *rq);
static inline struct dma_fence *
i915_active_fence_get(struct i915_active_fence *active)
{
	struct dma_fence *fence;
	rcu_read_lock();
	fence = dma_fence_get_rcu_safe(&active->fence);
	rcu_read_unlock();
	return fence;
}
static inline bool
i915_active_fence_isset(const struct i915_active_fence *active)
{
	return rcu_access_pointer(active->fence);
}
void __i915_active_init(struct i915_active *ref,
			int (*active)(struct i915_active *ref),
			void (*retire)(struct i915_active *ref),
			unsigned long flags,
			struct lock_class_key *mkey,
			struct lock_class_key *wkey);
#define i915_active_init(ref, active, retire, flags) do {			\
	static struct lock_class_key __mkey;					\
	static struct lock_class_key __wkey;					\
										\
	__i915_active_init(ref, active, retire, flags, &__mkey, &__wkey);	\
} while (0)
int i915_active_add_request(struct i915_active *ref, struct i915_request *rq);
struct dma_fence *
i915_active_set_exclusive(struct i915_active *ref, struct dma_fence *f);
int __i915_active_wait(struct i915_active *ref, int state);
static inline int i915_active_wait(struct i915_active *ref)
{
	return __i915_active_wait(ref, TASK_INTERRUPTIBLE);
}
int i915_sw_fence_await_active(struct i915_sw_fence *fence,
			       struct i915_active *ref,
			       unsigned int flags);
int i915_request_await_active(struct i915_request *rq,
			      struct i915_active *ref,
			      unsigned int flags);
#define I915_ACTIVE_AWAIT_EXCL BIT(0)
#define I915_ACTIVE_AWAIT_ACTIVE BIT(1)
#define I915_ACTIVE_AWAIT_BARRIER BIT(2)
int i915_active_acquire(struct i915_active *ref);
int i915_active_acquire_for_context(struct i915_active *ref, u64 idx);
bool i915_active_acquire_if_busy(struct i915_active *ref);
void i915_active_release(struct i915_active *ref);
static inline void __i915_active_acquire(struct i915_active *ref)
{
	GEM_BUG_ON(!atomic_read(&ref->count));
	atomic_inc(&ref->count);
}
static inline bool
i915_active_is_idle(const struct i915_active *ref)
{
	return !atomic_read(&ref->count);
}
void i915_active_fini(struct i915_active *ref);
int i915_active_acquire_preallocate_barrier(struct i915_active *ref,
					    struct intel_engine_cs *engine);
void i915_active_acquire_barrier(struct i915_active *ref);
void i915_request_add_active_barriers(struct i915_request *rq);
void i915_active_print(struct i915_active *ref, struct drm_printer *m);
void i915_active_unlock_wait(struct i915_active *ref);
struct i915_active *i915_active_create(void);
struct i915_active *i915_active_get(struct i915_active *ref);
void i915_active_put(struct i915_active *ref);
static inline int __i915_request_await_exclusive(struct i915_request *rq,
						 struct i915_active *active)
{
	struct dma_fence *fence;
	int err = 0;
	fence = i915_active_fence_get(&active->excl);
	if (fence) {
		err = i915_request_await_dma_fence(rq, fence);
		dma_fence_put(fence);
	}
	return err;
}
void i915_active_module_exit(void);
int i915_active_module_init(void);
#endif  
