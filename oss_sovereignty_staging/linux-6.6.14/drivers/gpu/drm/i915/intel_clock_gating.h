 
 

#ifndef __INTEL_CLOCK_GATING_H__
#define __INTEL_CLOCK_GATING_H__

struct drm_i915_private;

void intel_clock_gating_init(struct drm_i915_private *i915);
void intel_clock_gating_hooks_init(struct drm_i915_private *i915);

#endif  
