#ifndef IGT_LIVE_TEST_H
#define IGT_LIVE_TEST_H
#include "gt/intel_gt_defines.h"  
#include "gt/intel_engine.h"  
struct drm_i915_private;
struct igt_live_test {
	struct drm_i915_private *i915;
	const char *func;
	const char *name;
	unsigned int reset_global;
	unsigned int reset_engine[I915_MAX_GT][I915_NUM_ENGINES];
};
int igt_live_test_begin(struct igt_live_test *t,
			struct drm_i915_private *i915,
			const char *func,
			const char *name);
int igt_live_test_end(struct igt_live_test *t);
#endif  
