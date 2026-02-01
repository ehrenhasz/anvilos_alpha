 

#include <linux/pm_runtime.h>

#include <drm/drm_print.h>

#include "i915_drv.h"
#include "i915_trace.h"

 

#if IS_ENABLED(CONFIG_DRM_I915_DEBUG_RUNTIME_PM)

#include <linux/sort.h>

#define STACKDEPTH 8

static noinline depot_stack_handle_t __save_depot_stack(void)
{
	unsigned long entries[STACKDEPTH];
	unsigned int n;

	n = stack_trace_save(entries, ARRAY_SIZE(entries), 1);
	return stack_depot_save(entries, n, GFP_NOWAIT | __GFP_NOWARN);
}

static void init_intel_runtime_pm_wakeref(struct intel_runtime_pm *rpm)
{
	spin_lock_init(&rpm->debug.lock);
	stack_depot_init();
}

static noinline depot_stack_handle_t
track_intel_runtime_pm_wakeref(struct intel_runtime_pm *rpm)
{
	depot_stack_handle_t stack, *stacks;
	unsigned long flags;

	if (rpm->no_wakeref_tracking)
		return -1;

	stack = __save_depot_stack();
	if (!stack)
		return -1;

	spin_lock_irqsave(&rpm->debug.lock, flags);

	if (!rpm->debug.count)
		rpm->debug.last_acquire = stack;

	stacks = krealloc(rpm->debug.owners,
			  (rpm->debug.count + 1) * sizeof(*stacks),
			  GFP_NOWAIT | __GFP_NOWARN);
	if (stacks) {
		stacks[rpm->debug.count++] = stack;
		rpm->debug.owners = stacks;
	} else {
		stack = -1;
	}

	spin_unlock_irqrestore(&rpm->debug.lock, flags);

	return stack;
}

static void untrack_intel_runtime_pm_wakeref(struct intel_runtime_pm *rpm,
					     depot_stack_handle_t stack)
{
	struct drm_i915_private *i915 = container_of(rpm,
						     struct drm_i915_private,
						     runtime_pm);
	unsigned long flags, n;
	bool found = false;

	if (unlikely(stack == -1))
		return;

	spin_lock_irqsave(&rpm->debug.lock, flags);
	for (n = rpm->debug.count; n--; ) {
		if (rpm->debug.owners[n] == stack) {
			memmove(rpm->debug.owners + n,
				rpm->debug.owners + n + 1,
				(--rpm->debug.count - n) * sizeof(stack));
			found = true;
			break;
		}
	}
	spin_unlock_irqrestore(&rpm->debug.lock, flags);

	if (drm_WARN(&i915->drm, !found,
		     "Unmatched wakeref (tracking %lu), count %u\n",
		     rpm->debug.count, atomic_read(&rpm->wakeref_count))) {
		char *buf;

		buf = kmalloc(PAGE_SIZE, GFP_NOWAIT | __GFP_NOWARN);
		if (!buf)
			return;

		stack_depot_snprint(stack, buf, PAGE_SIZE, 2);
		DRM_DEBUG_DRIVER("wakeref %x from\n%s", stack, buf);

		stack = READ_ONCE(rpm->debug.last_release);
		if (stack) {
			stack_depot_snprint(stack, buf, PAGE_SIZE, 2);
			DRM_DEBUG_DRIVER("wakeref last released at\n%s", buf);
		}

		kfree(buf);
	}
}

static int cmphandle(const void *_a, const void *_b)
{
	const depot_stack_handle_t * const a = _a, * const b = _b;

	if (*a < *b)
		return -1;
	else if (*a > *b)
		return 1;
	else
		return 0;
}

static void
__print_intel_runtime_pm_wakeref(struct drm_printer *p,
				 const struct intel_runtime_pm_debug *dbg)
{
	unsigned long i;
	char *buf;

	buf = kmalloc(PAGE_SIZE, GFP_NOWAIT | __GFP_NOWARN);
	if (!buf)
		return;

	if (dbg->last_acquire) {
		stack_depot_snprint(dbg->last_acquire, buf, PAGE_SIZE, 2);
		drm_printf(p, "Wakeref last acquired:\n%s", buf);
	}

	if (dbg->last_release) {
		stack_depot_snprint(dbg->last_release, buf, PAGE_SIZE, 2);
		drm_printf(p, "Wakeref last released:\n%s", buf);
	}

	drm_printf(p, "Wakeref count: %lu\n", dbg->count);

	sort(dbg->owners, dbg->count, sizeof(*dbg->owners), cmphandle, NULL);

	for (i = 0; i < dbg->count; i++) {
		depot_stack_handle_t stack = dbg->owners[i];
		unsigned long rep;

		rep = 1;
		while (i + 1 < dbg->count && dbg->owners[i + 1] == stack)
			rep++, i++;
		stack_depot_snprint(stack, buf, PAGE_SIZE, 2);
		drm_printf(p, "Wakeref x%lu taken at:\n%s", rep, buf);
	}

	kfree(buf);
}

static noinline void
__untrack_all_wakerefs(struct intel_runtime_pm_debug *debug,
		       struct intel_runtime_pm_debug *saved)
{
	*saved = *debug;

	debug->owners = NULL;
	debug->count = 0;
	debug->last_release = __save_depot_stack();
}

static void
dump_and_free_wakeref_tracking(struct intel_runtime_pm_debug *debug)
{
	if (debug->count) {
		struct drm_printer p = drm_debug_printer("i915");

		__print_intel_runtime_pm_wakeref(&p, debug);
	}

	kfree(debug->owners);
}

static noinline void
__intel_wakeref_dec_and_check_tracking(struct intel_runtime_pm *rpm)
{
	struct intel_runtime_pm_debug dbg = {};
	unsigned long flags;

	if (!atomic_dec_and_lock_irqsave(&rpm->wakeref_count,
					 &rpm->debug.lock,
					 flags))
		return;

	__untrack_all_wakerefs(&rpm->debug, &dbg);
	spin_unlock_irqrestore(&rpm->debug.lock, flags);

	dump_and_free_wakeref_tracking(&dbg);
}

static noinline void
untrack_all_intel_runtime_pm_wakerefs(struct intel_runtime_pm *rpm)
{
	struct intel_runtime_pm_debug dbg = {};
	unsigned long flags;

	spin_lock_irqsave(&rpm->debug.lock, flags);
	__untrack_all_wakerefs(&rpm->debug, &dbg);
	spin_unlock_irqrestore(&rpm->debug.lock, flags);

	dump_and_free_wakeref_tracking(&dbg);
}

void print_intel_runtime_pm_wakeref(struct intel_runtime_pm *rpm,
				    struct drm_printer *p)
{
	struct intel_runtime_pm_debug dbg = {};

	do {
		unsigned long alloc = dbg.count;
		depot_stack_handle_t *s;

		spin_lock_irq(&rpm->debug.lock);
		dbg.count = rpm->debug.count;
		if (dbg.count <= alloc) {
			memcpy(dbg.owners,
			       rpm->debug.owners,
			       dbg.count * sizeof(*s));
		}
		dbg.last_acquire = rpm->debug.last_acquire;
		dbg.last_release = rpm->debug.last_release;
		spin_unlock_irq(&rpm->debug.lock);
		if (dbg.count <= alloc)
			break;

		s = krealloc(dbg.owners,
			     dbg.count * sizeof(*s),
			     GFP_NOWAIT | __GFP_NOWARN);
		if (!s)
			goto out;

		dbg.owners = s;
	} while (1);

	__print_intel_runtime_pm_wakeref(p, &dbg);

out:
	kfree(dbg.owners);
}

#else

static void init_intel_runtime_pm_wakeref(struct intel_runtime_pm *rpm)
{
}

static depot_stack_handle_t
track_intel_runtime_pm_wakeref(struct intel_runtime_pm *rpm)
{
	return -1;
}

static void untrack_intel_runtime_pm_wakeref(struct intel_runtime_pm *rpm,
					     intel_wakeref_t wref)
{
}

static void
__intel_wakeref_dec_and_check_tracking(struct intel_runtime_pm *rpm)
{
	atomic_dec(&rpm->wakeref_count);
}

static void
untrack_all_intel_runtime_pm_wakerefs(struct intel_runtime_pm *rpm)
{
}

#endif

static void
intel_runtime_pm_acquire(struct intel_runtime_pm *rpm, bool wakelock)
{
	if (wakelock) {
		atomic_add(1 + INTEL_RPM_WAKELOCK_BIAS, &rpm->wakeref_count);
		assert_rpm_wakelock_held(rpm);
	} else {
		atomic_inc(&rpm->wakeref_count);
		assert_rpm_raw_wakeref_held(rpm);
	}
}

static void
intel_runtime_pm_release(struct intel_runtime_pm *rpm, int wakelock)
{
	if (wakelock) {
		assert_rpm_wakelock_held(rpm);
		atomic_sub(INTEL_RPM_WAKELOCK_BIAS, &rpm->wakeref_count);
	} else {
		assert_rpm_raw_wakeref_held(rpm);
	}

	__intel_wakeref_dec_and_check_tracking(rpm);
}

static intel_wakeref_t __intel_runtime_pm_get(struct intel_runtime_pm *rpm,
					      bool wakelock)
{
	struct drm_i915_private *i915 = container_of(rpm,
						     struct drm_i915_private,
						     runtime_pm);
	int ret;

	ret = pm_runtime_get_sync(rpm->kdev);
	drm_WARN_ONCE(&i915->drm, ret < 0,
		      "pm_runtime_get_sync() failed: %d\n", ret);

	intel_runtime_pm_acquire(rpm, wakelock);

	return track_intel_runtime_pm_wakeref(rpm);
}

 
intel_wakeref_t intel_runtime_pm_get_raw(struct intel_runtime_pm *rpm)
{
	return __intel_runtime_pm_get(rpm, false);
}

 
intel_wakeref_t intel_runtime_pm_get(struct intel_runtime_pm *rpm)
{
	return __intel_runtime_pm_get(rpm, true);
}

 
static intel_wakeref_t __intel_runtime_pm_get_if_active(struct intel_runtime_pm *rpm,
							bool ignore_usecount)
{
	if (IS_ENABLED(CONFIG_PM)) {
		 
		if (pm_runtime_get_if_active(rpm->kdev, ignore_usecount) <= 0)
			return 0;
	}

	intel_runtime_pm_acquire(rpm, true);

	return track_intel_runtime_pm_wakeref(rpm);
}

intel_wakeref_t intel_runtime_pm_get_if_in_use(struct intel_runtime_pm *rpm)
{
	return __intel_runtime_pm_get_if_active(rpm, false);
}

intel_wakeref_t intel_runtime_pm_get_if_active(struct intel_runtime_pm *rpm)
{
	return __intel_runtime_pm_get_if_active(rpm, true);
}

 
intel_wakeref_t intel_runtime_pm_get_noresume(struct intel_runtime_pm *rpm)
{
	assert_rpm_wakelock_held(rpm);
	pm_runtime_get_noresume(rpm->kdev);

	intel_runtime_pm_acquire(rpm, true);

	return track_intel_runtime_pm_wakeref(rpm);
}

static void __intel_runtime_pm_put(struct intel_runtime_pm *rpm,
				   intel_wakeref_t wref,
				   bool wakelock)
{
	struct device *kdev = rpm->kdev;

	untrack_intel_runtime_pm_wakeref(rpm, wref);

	intel_runtime_pm_release(rpm, wakelock);

	pm_runtime_mark_last_busy(kdev);
	pm_runtime_put_autosuspend(kdev);
}

 
void
intel_runtime_pm_put_raw(struct intel_runtime_pm *rpm, intel_wakeref_t wref)
{
	__intel_runtime_pm_put(rpm, wref, false);
}

 
void intel_runtime_pm_put_unchecked(struct intel_runtime_pm *rpm)
{
	__intel_runtime_pm_put(rpm, -1, true);
}

#if IS_ENABLED(CONFIG_DRM_I915_DEBUG_RUNTIME_PM)
 
void intel_runtime_pm_put(struct intel_runtime_pm *rpm, intel_wakeref_t wref)
{
	__intel_runtime_pm_put(rpm, wref, true);
}
#endif

 
void intel_runtime_pm_enable(struct intel_runtime_pm *rpm)
{
	struct drm_i915_private *i915 = container_of(rpm,
						     struct drm_i915_private,
						     runtime_pm);
	struct device *kdev = rpm->kdev;

	 
	dev_pm_set_driver_flags(kdev, DPM_FLAG_NO_DIRECT_COMPLETE);

	pm_runtime_set_autosuspend_delay(kdev, 10000);  
	pm_runtime_mark_last_busy(kdev);

	 
	if (!rpm->available) {
		int ret;

		pm_runtime_dont_use_autosuspend(kdev);
		ret = pm_runtime_get_sync(kdev);
		drm_WARN(&i915->drm, ret < 0,
			 "pm_runtime_get_sync() failed: %d\n", ret);
	} else {
		pm_runtime_use_autosuspend(kdev);
	}

	 
	if (!IS_DGFX(i915))
		pm_runtime_allow(kdev);

	 
	pm_runtime_put_autosuspend(kdev);
}

void intel_runtime_pm_disable(struct intel_runtime_pm *rpm)
{
	struct drm_i915_private *i915 = container_of(rpm,
						     struct drm_i915_private,
						     runtime_pm);
	struct device *kdev = rpm->kdev;

	 
	drm_WARN(&i915->drm, pm_runtime_get_sync(kdev) < 0,
		 "Failed to pass rpm ownership back to core\n");

	pm_runtime_dont_use_autosuspend(kdev);

	if (!rpm->available)
		pm_runtime_put(kdev);
}

void intel_runtime_pm_driver_release(struct intel_runtime_pm *rpm)
{
	struct drm_i915_private *i915 = container_of(rpm,
						     struct drm_i915_private,
						     runtime_pm);
	int count = atomic_read(&rpm->wakeref_count);

	intel_wakeref_auto_fini(&rpm->userfault_wakeref);

	drm_WARN(&i915->drm, count,
		 "i915 raw-wakerefs=%d wakelocks=%d on cleanup\n",
		 intel_rpm_raw_wakeref_count(count),
		 intel_rpm_wakelock_count(count));

	untrack_all_intel_runtime_pm_wakerefs(rpm);
}

void intel_runtime_pm_init_early(struct intel_runtime_pm *rpm)
{
	struct drm_i915_private *i915 =
			container_of(rpm, struct drm_i915_private, runtime_pm);
	struct pci_dev *pdev = to_pci_dev(i915->drm.dev);
	struct device *kdev = &pdev->dev;

	rpm->kdev = kdev;
	rpm->available = HAS_RUNTIME_PM(i915);
	rpm->suspended = false;
	atomic_set(&rpm->wakeref_count, 0);

	init_intel_runtime_pm_wakeref(rpm);
	INIT_LIST_HEAD(&rpm->lmem_userfault_list);
	spin_lock_init(&rpm->lmem_userfault_lock);
	intel_wakeref_auto_init(&rpm->userfault_wakeref, i915);
}
