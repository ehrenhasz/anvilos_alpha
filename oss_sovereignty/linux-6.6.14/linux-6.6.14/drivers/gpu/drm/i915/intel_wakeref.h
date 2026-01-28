#ifndef INTEL_WAKEREF_H
#define INTEL_WAKEREF_H
#include <linux/atomic.h>
#include <linux/bitfield.h>
#include <linux/bits.h>
#include <linux/lockdep.h>
#include <linux/mutex.h>
#include <linux/refcount.h>
#include <linux/stackdepot.h>
#include <linux/timer.h>
#include <linux/workqueue.h>
#if IS_ENABLED(CONFIG_DRM_I915_DEBUG)
#define INTEL_WAKEREF_BUG_ON(expr) BUG_ON(expr)
#else
#define INTEL_WAKEREF_BUG_ON(expr) BUILD_BUG_ON_INVALID(expr)
#endif
struct intel_runtime_pm;
struct intel_wakeref;
typedef depot_stack_handle_t intel_wakeref_t;
struct intel_wakeref_ops {
	int (*get)(struct intel_wakeref *wf);
	int (*put)(struct intel_wakeref *wf);
};
struct intel_wakeref {
	atomic_t count;
	struct mutex mutex;
	intel_wakeref_t wakeref;
	struct drm_i915_private *i915;
	const struct intel_wakeref_ops *ops;
	struct delayed_work work;
};
struct intel_wakeref_lockclass {
	struct lock_class_key mutex;
	struct lock_class_key work;
};
void __intel_wakeref_init(struct intel_wakeref *wf,
			  struct drm_i915_private *i915,
			  const struct intel_wakeref_ops *ops,
			  struct intel_wakeref_lockclass *key);
#define intel_wakeref_init(wf, i915, ops) do {				\
	static struct intel_wakeref_lockclass __key;			\
									\
	__intel_wakeref_init((wf), (i915), (ops), &__key);		\
} while (0)
int __intel_wakeref_get_first(struct intel_wakeref *wf);
void __intel_wakeref_put_last(struct intel_wakeref *wf, unsigned long flags);
static inline int
intel_wakeref_get(struct intel_wakeref *wf)
{
	might_sleep();
	if (unlikely(!atomic_inc_not_zero(&wf->count)))
		return __intel_wakeref_get_first(wf);
	return 0;
}
static inline void
__intel_wakeref_get(struct intel_wakeref *wf)
{
	INTEL_WAKEREF_BUG_ON(atomic_read(&wf->count) <= 0);
	atomic_inc(&wf->count);
}
static inline bool
intel_wakeref_get_if_active(struct intel_wakeref *wf)
{
	return atomic_inc_not_zero(&wf->count);
}
enum {
	INTEL_WAKEREF_PUT_ASYNC_BIT = 0,
	__INTEL_WAKEREF_PUT_LAST_BIT__
};
static inline void
intel_wakeref_might_get(struct intel_wakeref *wf)
{
	might_lock(&wf->mutex);
}
static inline void
__intel_wakeref_put(struct intel_wakeref *wf, unsigned long flags)
#define INTEL_WAKEREF_PUT_ASYNC BIT(INTEL_WAKEREF_PUT_ASYNC_BIT)
#define INTEL_WAKEREF_PUT_DELAY \
	GENMASK(BITS_PER_LONG - 1, __INTEL_WAKEREF_PUT_LAST_BIT__)
{
	INTEL_WAKEREF_BUG_ON(atomic_read(&wf->count) <= 0);
	if (unlikely(!atomic_add_unless(&wf->count, -1, 1)))
		__intel_wakeref_put_last(wf, flags);
}
static inline void
intel_wakeref_put(struct intel_wakeref *wf)
{
	might_sleep();
	__intel_wakeref_put(wf, 0);
}
static inline void
intel_wakeref_put_async(struct intel_wakeref *wf)
{
	__intel_wakeref_put(wf, INTEL_WAKEREF_PUT_ASYNC);
}
static inline void
intel_wakeref_put_delay(struct intel_wakeref *wf, unsigned long delay)
{
	__intel_wakeref_put(wf,
			    INTEL_WAKEREF_PUT_ASYNC |
			    FIELD_PREP(INTEL_WAKEREF_PUT_DELAY, delay));
}
static inline void
intel_wakeref_might_put(struct intel_wakeref *wf)
{
	might_lock(&wf->mutex);
}
static inline void
intel_wakeref_lock(struct intel_wakeref *wf)
	__acquires(wf->mutex)
{
	mutex_lock(&wf->mutex);
}
static inline void
intel_wakeref_unlock(struct intel_wakeref *wf)
	__releases(wf->mutex)
{
	mutex_unlock(&wf->mutex);
}
static inline void
intel_wakeref_unlock_wait(struct intel_wakeref *wf)
{
	mutex_lock(&wf->mutex);
	mutex_unlock(&wf->mutex);
	flush_delayed_work(&wf->work);
}
static inline bool
intel_wakeref_is_active(const struct intel_wakeref *wf)
{
	return READ_ONCE(wf->wakeref);
}
static inline void
__intel_wakeref_defer_park(struct intel_wakeref *wf)
{
	lockdep_assert_held(&wf->mutex);
	INTEL_WAKEREF_BUG_ON(atomic_read(&wf->count));
	atomic_set_release(&wf->count, 1);
}
int intel_wakeref_wait_for_idle(struct intel_wakeref *wf);
struct intel_wakeref_auto {
	struct drm_i915_private *i915;
	struct timer_list timer;
	intel_wakeref_t wakeref;
	spinlock_t lock;
	refcount_t count;
};
void intel_wakeref_auto(struct intel_wakeref_auto *wf, unsigned long timeout);
void intel_wakeref_auto_init(struct intel_wakeref_auto *wf,
			     struct drm_i915_private *i915);
void intel_wakeref_auto_fini(struct intel_wakeref_auto *wf);
#endif  
