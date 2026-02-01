 

#include "lib_sw_fence.h"

 

static int
nop_fence_notify(struct i915_sw_fence *fence, enum i915_sw_fence_notify state)
{
	return NOTIFY_DONE;
}

void __onstack_fence_init(struct i915_sw_fence *fence,
			  const char *name,
			  struct lock_class_key *key)
{
	debug_fence_init_onstack(fence);

	__init_waitqueue_head(&fence->wait, name, key);
	atomic_set(&fence->pending, 1);
	fence->error = 0;
	fence->fn = nop_fence_notify;
}

void onstack_fence_fini(struct i915_sw_fence *fence)
{
	if (!fence->fn)
		return;

	i915_sw_fence_commit(fence);
	i915_sw_fence_fini(fence);
}

static void timed_fence_wake(struct timer_list *t)
{
	struct timed_fence *tf = from_timer(tf, t, timer);

	i915_sw_fence_commit(&tf->fence);
}

void timed_fence_init(struct timed_fence *tf, unsigned long expires)
{
	onstack_fence_init(&tf->fence);

	timer_setup_on_stack(&tf->timer, timed_fence_wake, 0);

	if (time_after(expires, jiffies))
		mod_timer(&tf->timer, expires);
	else
		i915_sw_fence_commit(&tf->fence);
}

void timed_fence_fini(struct timed_fence *tf)
{
	if (del_timer_sync(&tf->timer))
		i915_sw_fence_commit(&tf->fence);

	destroy_timer_on_stack(&tf->timer);
	i915_sw_fence_fini(&tf->fence);
}

struct heap_fence {
	struct i915_sw_fence fence;
	union {
		struct kref ref;
		struct rcu_head rcu;
	};
};

static int
heap_fence_notify(struct i915_sw_fence *fence, enum i915_sw_fence_notify state)
{
	struct heap_fence *h = container_of(fence, typeof(*h), fence);

	switch (state) {
	case FENCE_COMPLETE:
		break;

	case FENCE_FREE:
		heap_fence_put(&h->fence);
	}

	return NOTIFY_DONE;
}

struct i915_sw_fence *heap_fence_create(gfp_t gfp)
{
	struct heap_fence *h;

	h = kmalloc(sizeof(*h), gfp);
	if (!h)
		return NULL;

	i915_sw_fence_init(&h->fence, heap_fence_notify);
	refcount_set(&h->ref.refcount, 2);

	return &h->fence;
}

static void heap_fence_release(struct kref *ref)
{
	struct heap_fence *h = container_of(ref, typeof(*h), ref);

	i915_sw_fence_fini(&h->fence);

	kfree_rcu(h, rcu);
}

void heap_fence_put(struct i915_sw_fence *fence)
{
	struct heap_fence *h = container_of(fence, typeof(*h), fence);

	kref_put(&h->ref, heap_fence_release);
}
