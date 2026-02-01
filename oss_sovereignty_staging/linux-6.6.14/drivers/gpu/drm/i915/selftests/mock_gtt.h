 

#ifndef __MOCK_GTT_H
#define __MOCK_GTT_H

struct drm_i915_private;
struct i915_ggtt;
struct intel_gt;

void mock_init_ggtt(struct intel_gt *gt);
void mock_fini_ggtt(struct i915_ggtt *ggtt);

struct i915_ppgtt *mock_ppgtt(struct drm_i915_private *i915, const char *name);

#endif  
