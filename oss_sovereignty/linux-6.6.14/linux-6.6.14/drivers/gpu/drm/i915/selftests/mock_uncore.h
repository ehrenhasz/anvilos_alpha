#ifndef __MOCK_UNCORE_H
#define __MOCK_UNCORE_H
struct drm_i915_private;
struct intel_uncore;
void mock_uncore_init(struct intel_uncore *uncore,
		      struct drm_i915_private *i915);
#endif  
