#ifndef __INTEL_DRAM_H__
#define __INTEL_DRAM_H__
struct drm_i915_private;
void intel_dram_edram_detect(struct drm_i915_private *i915);
void intel_dram_detect(struct drm_i915_private *i915);
#endif  
