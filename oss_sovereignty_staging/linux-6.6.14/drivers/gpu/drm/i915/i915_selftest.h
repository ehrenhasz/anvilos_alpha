 

#ifndef __I915_SELFTEST_H__
#define __I915_SELFTEST_H__

#include <linux/types.h>

struct pci_dev;
struct drm_i915_private;

struct i915_selftest {
	unsigned long timeout_jiffies;
	unsigned int timeout_ms;
	unsigned int random_seed;
	char *filter;
	int mock;
	int live;
	int perf;
};

#if IS_ENABLED(CONFIG_DRM_I915_SELFTEST)
#include <linux/fault-inject.h>

extern struct i915_selftest i915_selftest;

int i915_mock_selftests(void);
int i915_live_selftests(struct pci_dev *pdev);
int i915_perf_selftests(struct pci_dev *pdev);

 
#define selftest(name, func) int func(void);
#include "selftests/i915_mock_selftests.h"
#undef selftest
#define selftest(name, func) int func(struct drm_i915_private *i915);
#include "selftests/i915_live_selftests.h"
#include "selftests/i915_perf_selftests.h"
#undef selftest

struct i915_subtest {
	int (*func)(void *data);
	const char *name;
};

int __i915_nop_setup(void *data);
int __i915_nop_teardown(int err, void *data);

int __i915_live_setup(void *data);
int __i915_live_teardown(int err, void *data);

int __intel_gt_live_setup(void *data);
int __intel_gt_live_teardown(int err, void *data);

int __i915_subtests(const char *caller,
		    int (*setup)(void *data),
		    int (*teardown)(int err, void *data),
		    const struct i915_subtest *st,
		    unsigned int count,
		    void *data);
#define i915_subtests(T, data) \
	__i915_subtests(__func__, \
			__i915_nop_setup, __i915_nop_teardown, \
			T, ARRAY_SIZE(T), data)
#define i915_live_subtests(T, data) ({ \
	typecheck(struct drm_i915_private *, data); \
	(data)->gt[0]->uc.guc.submission_state.sched_disable_delay_ms = 0; \
	__i915_subtests(__func__, \
			__i915_live_setup, __i915_live_teardown, \
			T, ARRAY_SIZE(T), data); \
})
#define intel_gt_live_subtests(T, data) ({ \
	typecheck(struct intel_gt *, data); \
	(data)->uc.guc.submission_state.sched_disable_delay_ms = 0; \
	__i915_subtests(__func__, \
			__intel_gt_live_setup, __intel_gt_live_teardown, \
			T, ARRAY_SIZE(T), data); \
})

#define SUBTEST(x) { x, #x }

#define I915_SELFTEST_DECLARE(x) x
#define I915_SELFTEST_ONLY(x) unlikely(x)
#define I915_SELFTEST_EXPORT

#else  

static inline int i915_mock_selftests(void) { return 0; }
static inline int i915_live_selftests(struct pci_dev *pdev) { return 0; }
static inline int i915_perf_selftests(struct pci_dev *pdev) { return 0; }

#define I915_SELFTEST_DECLARE(x)
#define I915_SELFTEST_ONLY(x) 0
#define I915_SELFTEST_EXPORT static

#endif

 

#define IGT_TIMEOUT(name__) \
	unsigned long name__ = jiffies + i915_selftest.timeout_jiffies

__printf(2, 3)
bool __igt_timeout(unsigned long timeout, const char *fmt, ...);

#define igt_timeout(t, fmt, ...) \
	__igt_timeout((t), KERN_DEBUG pr_fmt(fmt), ##__VA_ARGS__)

void igt_hexdump(const void *buf, size_t len);

#endif  
