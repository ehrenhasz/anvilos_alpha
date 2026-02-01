 
 

#ifndef __INTEL_RUNTIME_PM_H__
#define __INTEL_RUNTIME_PM_H__

#include <linux/types.h>

#include "intel_wakeref.h"

#include "i915_utils.h"

struct device;
struct drm_i915_private;
struct drm_printer;

 
struct intel_runtime_pm {
	atomic_t wakeref_count;
	struct device *kdev;  
	bool available;
	bool suspended;
	bool irqs_enabled;
	bool no_wakeref_tracking;

	 
	spinlock_t lmem_userfault_lock;

	 
	struct list_head lmem_userfault_list;

	 
	struct intel_wakeref_auto userfault_wakeref;

#if IS_ENABLED(CONFIG_DRM_I915_DEBUG_RUNTIME_PM)
	 
	struct intel_runtime_pm_debug {
		spinlock_t lock;

		depot_stack_handle_t last_acquire;
		depot_stack_handle_t last_release;

		depot_stack_handle_t *owners;
		unsigned long count;
	} debug;
#endif
};

#define BITS_PER_WAKEREF	\
	BITS_PER_TYPE(typeof_member(struct intel_runtime_pm, wakeref_count))
#define INTEL_RPM_WAKELOCK_SHIFT	(BITS_PER_WAKEREF / 2)
#define INTEL_RPM_WAKELOCK_BIAS		(1 << INTEL_RPM_WAKELOCK_SHIFT)
#define INTEL_RPM_RAW_WAKEREF_MASK	(INTEL_RPM_WAKELOCK_BIAS - 1)

static inline int
intel_rpm_raw_wakeref_count(int wakeref_count)
{
	return wakeref_count & INTEL_RPM_RAW_WAKEREF_MASK;
}

static inline int
intel_rpm_wakelock_count(int wakeref_count)
{
	return wakeref_count >> INTEL_RPM_WAKELOCK_SHIFT;
}

static inline void
assert_rpm_device_not_suspended(struct intel_runtime_pm *rpm)
{
	WARN_ONCE(rpm->suspended,
		  "Device suspended during HW access\n");
}

static inline void
__assert_rpm_raw_wakeref_held(struct intel_runtime_pm *rpm, int wakeref_count)
{
	assert_rpm_device_not_suspended(rpm);
	WARN_ONCE(!intel_rpm_raw_wakeref_count(wakeref_count),
		  "RPM raw-wakeref not held\n");
}

static inline void
__assert_rpm_wakelock_held(struct intel_runtime_pm *rpm, int wakeref_count)
{
	__assert_rpm_raw_wakeref_held(rpm, wakeref_count);
	WARN_ONCE(!intel_rpm_wakelock_count(wakeref_count),
		  "RPM wakelock ref not held during HW access\n");
}

static inline void
assert_rpm_raw_wakeref_held(struct intel_runtime_pm *rpm)
{
	__assert_rpm_raw_wakeref_held(rpm, atomic_read(&rpm->wakeref_count));
}

static inline void
assert_rpm_wakelock_held(struct intel_runtime_pm *rpm)
{
	__assert_rpm_wakelock_held(rpm, atomic_read(&rpm->wakeref_count));
}

 
static inline void
disable_rpm_wakeref_asserts(struct intel_runtime_pm *rpm)
{
	atomic_add(INTEL_RPM_WAKELOCK_BIAS + 1,
		   &rpm->wakeref_count);
}

 
static inline void
enable_rpm_wakeref_asserts(struct intel_runtime_pm *rpm)
{
	atomic_sub(INTEL_RPM_WAKELOCK_BIAS + 1,
		   &rpm->wakeref_count);
}

void intel_runtime_pm_init_early(struct intel_runtime_pm *rpm);
void intel_runtime_pm_enable(struct intel_runtime_pm *rpm);
void intel_runtime_pm_disable(struct intel_runtime_pm *rpm);
void intel_runtime_pm_driver_release(struct intel_runtime_pm *rpm);

intel_wakeref_t intel_runtime_pm_get(struct intel_runtime_pm *rpm);
intel_wakeref_t intel_runtime_pm_get_if_in_use(struct intel_runtime_pm *rpm);
intel_wakeref_t intel_runtime_pm_get_if_active(struct intel_runtime_pm *rpm);
intel_wakeref_t intel_runtime_pm_get_noresume(struct intel_runtime_pm *rpm);
intel_wakeref_t intel_runtime_pm_get_raw(struct intel_runtime_pm *rpm);

#define with_intel_runtime_pm(rpm, wf) \
	for ((wf) = intel_runtime_pm_get(rpm); (wf); \
	     intel_runtime_pm_put((rpm), (wf)), (wf) = 0)

#define with_intel_runtime_pm_if_in_use(rpm, wf) \
	for ((wf) = intel_runtime_pm_get_if_in_use(rpm); (wf); \
	     intel_runtime_pm_put((rpm), (wf)), (wf) = 0)

#define with_intel_runtime_pm_if_active(rpm, wf) \
	for ((wf) = intel_runtime_pm_get_if_active(rpm); (wf); \
	     intel_runtime_pm_put((rpm), (wf)), (wf) = 0)

void intel_runtime_pm_put_unchecked(struct intel_runtime_pm *rpm);
#if IS_ENABLED(CONFIG_DRM_I915_DEBUG_RUNTIME_PM)
void intel_runtime_pm_put(struct intel_runtime_pm *rpm, intel_wakeref_t wref);
#else
static inline void
intel_runtime_pm_put(struct intel_runtime_pm *rpm, intel_wakeref_t wref)
{
	intel_runtime_pm_put_unchecked(rpm);
}
#endif
void intel_runtime_pm_put_raw(struct intel_runtime_pm *rpm, intel_wakeref_t wref);

#if IS_ENABLED(CONFIG_DRM_I915_DEBUG_RUNTIME_PM)
void print_intel_runtime_pm_wakeref(struct intel_runtime_pm *rpm,
				    struct drm_printer *p);
#else
static inline void print_intel_runtime_pm_wakeref(struct intel_runtime_pm *rpm,
						  struct drm_printer *p)
{
}
#endif

#endif  
