#ifndef __INTEL_CDCLK_H__
#define __INTEL_CDCLK_H__
#include <linux/types.h>
#include "intel_display_limits.h"
#include "intel_global_state.h"
struct drm_i915_private;
struct intel_atomic_state;
struct intel_crtc_state;
struct intel_cdclk_config {
	unsigned int cdclk, vco, ref, bypass;
	u8 voltage_level;
};
struct intel_cdclk_state {
	struct intel_global_state base;
	struct intel_cdclk_config logical;
	struct intel_cdclk_config actual;
	int bw_min_cdclk;
	int min_cdclk[I915_MAX_PIPES];
	u8 min_voltage_level[I915_MAX_PIPES];
	enum pipe pipe;
	int force_min_cdclk;
	u8 active_pipes;
};
int intel_crtc_compute_min_cdclk(const struct intel_crtc_state *crtc_state);
void intel_cdclk_init_hw(struct drm_i915_private *i915);
void intel_cdclk_uninit_hw(struct drm_i915_private *i915);
void intel_init_cdclk_hooks(struct drm_i915_private *dev_priv);
void intel_update_max_cdclk(struct drm_i915_private *dev_priv);
void intel_update_cdclk(struct drm_i915_private *dev_priv);
u32 intel_read_rawclk(struct drm_i915_private *dev_priv);
bool intel_cdclk_needs_modeset(const struct intel_cdclk_config *a,
			       const struct intel_cdclk_config *b);
void intel_set_cdclk_pre_plane_update(struct intel_atomic_state *state);
void intel_set_cdclk_post_plane_update(struct intel_atomic_state *state);
void intel_cdclk_dump_config(struct drm_i915_private *i915,
			     const struct intel_cdclk_config *cdclk_config,
			     const char *context);
int intel_modeset_calc_cdclk(struct intel_atomic_state *state);
void intel_cdclk_get_cdclk(struct drm_i915_private *dev_priv,
			   struct intel_cdclk_config *cdclk_config);
int intel_cdclk_atomic_check(struct intel_atomic_state *state,
			     bool *need_cdclk_calc);
struct intel_cdclk_state *
intel_atomic_get_cdclk_state(struct intel_atomic_state *state);
#define to_intel_cdclk_state(x) container_of((x), struct intel_cdclk_state, base)
#define intel_atomic_get_old_cdclk_state(state) \
	to_intel_cdclk_state(intel_atomic_get_old_global_obj_state(state, &to_i915(state->base.dev)->display.cdclk.obj))
#define intel_atomic_get_new_cdclk_state(state) \
	to_intel_cdclk_state(intel_atomic_get_new_global_obj_state(state, &to_i915(state->base.dev)->display.cdclk.obj))
int intel_cdclk_init(struct drm_i915_private *dev_priv);
void intel_cdclk_debugfs_register(struct drm_i915_private *i915);
#endif  
