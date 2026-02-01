 

#ifndef __I915_PMU_H__
#define __I915_PMU_H__

#include <linux/hrtimer.h>
#include <linux/perf_event.h>
#include <linux/spinlock_types.h>
#include <uapi/drm/i915_drm.h>

struct drm_i915_private;
struct intel_gt;

 
enum i915_pmu_tracked_events {
	__I915_PMU_ACTUAL_FREQUENCY_ENABLED = 0,
	__I915_PMU_REQUESTED_FREQUENCY_ENABLED,
	__I915_PMU_RC6_RESIDENCY_ENABLED,
	__I915_PMU_TRACKED_EVENT_COUNT,  
};

 
enum {
	__I915_SAMPLE_FREQ_ACT = 0,
	__I915_SAMPLE_FREQ_REQ,
	__I915_SAMPLE_RC6,
	__I915_SAMPLE_RC6_LAST_REPORTED,
	__I915_NUM_PMU_SAMPLERS
};

#define I915_PMU_MAX_GT 2

 
#define I915_PMU_MASK_BITS \
	(I915_ENGINE_SAMPLE_COUNT + \
	 I915_PMU_MAX_GT * __I915_PMU_TRACKED_EVENT_COUNT)

#define I915_ENGINE_SAMPLE_COUNT (I915_SAMPLE_SEMA + 1)

struct i915_pmu_sample {
	u64 cur;
};

struct i915_pmu {
	 
	struct {
		struct hlist_node node;
		unsigned int cpu;
	} cpuhp;
	 
	struct pmu base;
	 
	bool closed;
	 
	const char *name;
	 
	spinlock_t lock;
	 
	unsigned int unparked;
	 
	struct hrtimer timer;
	 
	u32 enable;

	 
	ktime_t timer_last;

	 
	unsigned int enable_count[I915_PMU_MASK_BITS];
	 
	bool timer_enabled;
	 
	struct i915_pmu_sample sample[I915_PMU_MAX_GT][__I915_NUM_PMU_SAMPLERS];
	 
	ktime_t sleep_last[I915_PMU_MAX_GT];
	 
	unsigned long irq_count;
	 
	struct attribute_group events_attr_group;
	 
	void *i915_attr;
	 
	void *pmu_attr;
};

#ifdef CONFIG_PERF_EVENTS
int i915_pmu_init(void);
void i915_pmu_exit(void);
void i915_pmu_register(struct drm_i915_private *i915);
void i915_pmu_unregister(struct drm_i915_private *i915);
void i915_pmu_gt_parked(struct intel_gt *gt);
void i915_pmu_gt_unparked(struct intel_gt *gt);
#else
static inline int i915_pmu_init(void) { return 0; }
static inline void i915_pmu_exit(void) {}
static inline void i915_pmu_register(struct drm_i915_private *i915) {}
static inline void i915_pmu_unregister(struct drm_i915_private *i915) {}
static inline void i915_pmu_gt_parked(struct intel_gt *gt) {}
static inline void i915_pmu_gt_unparked(struct intel_gt *gt) {}
#endif

#endif
