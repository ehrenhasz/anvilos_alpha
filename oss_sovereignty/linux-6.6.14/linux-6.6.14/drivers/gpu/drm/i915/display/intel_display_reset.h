#ifndef __INTEL_RESET_H__
#define __INTEL_RESET_H__
struct drm_i915_private;
void intel_display_reset_prepare(struct drm_i915_private *i915);
void intel_display_reset_finish(struct drm_i915_private *i915);
#endif  
