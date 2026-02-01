 

 

#include <drm/drm_atomic_helper.h>
#include <drm/drm_blend.h>
#include <drm/drm_fourcc.h>

#include "i915_config.h"
#include "i915_reg.h"
#include "intel_atomic_plane.h"
#include "intel_cdclk.h"
#include "intel_display_rps.h"
#include "intel_display_trace.h"
#include "intel_display_types.h"
#include "intel_fb.h"
#include "intel_fb_pin.h"
#include "skl_scaler.h"
#include "skl_watermark.h"

static void intel_plane_state_reset(struct intel_plane_state *plane_state,
				    struct intel_plane *plane)
{
	memset(plane_state, 0, sizeof(*plane_state));

	__drm_atomic_helper_plane_state_reset(&plane_state->uapi, &plane->base);

	plane_state->scaler_id = -1;
}

struct intel_plane *intel_plane_alloc(void)
{
	struct intel_plane_state *plane_state;
	struct intel_plane *plane;

	plane = kzalloc(sizeof(*plane), GFP_KERNEL);
	if (!plane)
		return ERR_PTR(-ENOMEM);

	plane_state = kzalloc(sizeof(*plane_state), GFP_KERNEL);
	if (!plane_state) {
		kfree(plane);
		return ERR_PTR(-ENOMEM);
	}

	intel_plane_state_reset(plane_state, plane);

	plane->base.state = &plane_state->uapi;

	return plane;
}

void intel_plane_free(struct intel_plane *plane)
{
	intel_plane_destroy_state(&plane->base, plane->base.state);
	kfree(plane);
}

 
struct drm_plane_state *
intel_plane_duplicate_state(struct drm_plane *plane)
{
	struct intel_plane_state *intel_state;

	intel_state = to_intel_plane_state(plane->state);
	intel_state = kmemdup(intel_state, sizeof(*intel_state), GFP_KERNEL);

	if (!intel_state)
		return NULL;

	__drm_atomic_helper_plane_duplicate_state(plane, &intel_state->uapi);

	intel_state->ggtt_vma = NULL;
	intel_state->dpt_vma = NULL;
	intel_state->flags = 0;

	 
	if (intel_state->hw.fb)
		drm_framebuffer_get(intel_state->hw.fb);

	return &intel_state->uapi;
}

 
void
intel_plane_destroy_state(struct drm_plane *plane,
			  struct drm_plane_state *state)
{
	struct intel_plane_state *plane_state = to_intel_plane_state(state);

	drm_WARN_ON(plane->dev, plane_state->ggtt_vma);
	drm_WARN_ON(plane->dev, plane_state->dpt_vma);

	__drm_atomic_helper_plane_destroy_state(&plane_state->uapi);
	if (plane_state->hw.fb)
		drm_framebuffer_put(plane_state->hw.fb);
	kfree(plane_state);
}

unsigned int intel_adjusted_rate(const struct drm_rect *src,
				 const struct drm_rect *dst,
				 unsigned int rate)
{
	unsigned int src_w, src_h, dst_w, dst_h;

	src_w = drm_rect_width(src) >> 16;
	src_h = drm_rect_height(src) >> 16;
	dst_w = drm_rect_width(dst);
	dst_h = drm_rect_height(dst);

	 
	dst_w = min(src_w, dst_w);
	dst_h = min(src_h, dst_h);

	return DIV_ROUND_UP_ULL(mul_u32_u32(rate, src_w * src_h),
				dst_w * dst_h);
}

unsigned int intel_plane_pixel_rate(const struct intel_crtc_state *crtc_state,
				    const struct intel_plane_state *plane_state)
{
	 

	return intel_adjusted_rate(&plane_state->uapi.src,
				   &plane_state->uapi.dst,
				   crtc_state->pixel_rate);
}

unsigned int intel_plane_data_rate(const struct intel_crtc_state *crtc_state,
				   const struct intel_plane_state *plane_state,
				   int color_plane)
{
	const struct drm_framebuffer *fb = plane_state->hw.fb;

	if (!plane_state->uapi.visible)
		return 0;

	return intel_plane_pixel_rate(crtc_state, plane_state) *
		fb->format->cpp[color_plane];
}

static bool
use_min_ddb(const struct intel_crtc_state *crtc_state,
	    struct intel_plane *plane)
{
	struct drm_i915_private *i915 = to_i915(plane->base.dev);

	return DISPLAY_VER(i915) >= 13 &&
	       crtc_state->uapi.async_flip &&
	       plane->async_flip;
}

static unsigned int
intel_plane_relative_data_rate(const struct intel_crtc_state *crtc_state,
			       const struct intel_plane_state *plane_state,
			       int color_plane)
{
	struct intel_plane *plane = to_intel_plane(plane_state->uapi.plane);
	const struct drm_framebuffer *fb = plane_state->hw.fb;
	int width, height;
	unsigned int rel_data_rate;

	if (plane->id == PLANE_CURSOR)
		return 0;

	if (!plane_state->uapi.visible)
		return 0;

	 
	if (use_min_ddb(crtc_state, plane))
		return 0;

	 
	width = drm_rect_width(&plane_state->uapi.src) >> 16;
	height = drm_rect_height(&plane_state->uapi.src) >> 16;

	 
	if (color_plane == 1) {
		width /= 2;
		height /= 2;
	}

	rel_data_rate = width * height * fb->format->cpp[color_plane];

	return intel_adjusted_rate(&plane_state->uapi.src,
				   &plane_state->uapi.dst,
				   rel_data_rate);
}

int intel_plane_calc_min_cdclk(struct intel_atomic_state *state,
			       struct intel_plane *plane,
			       bool *need_cdclk_calc)
{
	struct drm_i915_private *dev_priv = to_i915(plane->base.dev);
	const struct intel_plane_state *plane_state =
		intel_atomic_get_new_plane_state(state, plane);
	struct intel_crtc *crtc = to_intel_crtc(plane_state->hw.crtc);
	const struct intel_cdclk_state *cdclk_state;
	const struct intel_crtc_state *old_crtc_state;
	struct intel_crtc_state *new_crtc_state;

	if (!plane_state->uapi.visible || !plane->min_cdclk)
		return 0;

	old_crtc_state = intel_atomic_get_old_crtc_state(state, crtc);
	new_crtc_state = intel_atomic_get_new_crtc_state(state, crtc);

	new_crtc_state->min_cdclk[plane->id] =
		plane->min_cdclk(new_crtc_state, plane_state);

	 
	if (new_crtc_state->min_cdclk[plane->id] <=
	    old_crtc_state->min_cdclk[plane->id])
		return 0;

	cdclk_state = intel_atomic_get_cdclk_state(state);
	if (IS_ERR(cdclk_state))
		return PTR_ERR(cdclk_state);

	 
	if (new_crtc_state->min_cdclk[plane->id] <=
	    cdclk_state->min_cdclk[crtc->pipe])
		return 0;

	drm_dbg_kms(&dev_priv->drm,
		    "[PLANE:%d:%s] min cdclk (%d kHz) > [CRTC:%d:%s] min cdclk (%d kHz)\n",
		    plane->base.base.id, plane->base.name,
		    new_crtc_state->min_cdclk[plane->id],
		    crtc->base.base.id, crtc->base.name,
		    cdclk_state->min_cdclk[crtc->pipe]);
	*need_cdclk_calc = true;

	return 0;
}

static void intel_plane_clear_hw_state(struct intel_plane_state *plane_state)
{
	if (plane_state->hw.fb)
		drm_framebuffer_put(plane_state->hw.fb);

	memset(&plane_state->hw, 0, sizeof(plane_state->hw));
}

void intel_plane_copy_uapi_to_hw_state(struct intel_plane_state *plane_state,
				       const struct intel_plane_state *from_plane_state,
				       struct intel_crtc *crtc)
{
	intel_plane_clear_hw_state(plane_state);

	 
	plane_state->hw.crtc = from_plane_state->uapi.crtc ? &crtc->base : NULL;

	plane_state->hw.fb = from_plane_state->uapi.fb;
	if (plane_state->hw.fb)
		drm_framebuffer_get(plane_state->hw.fb);

	plane_state->hw.alpha = from_plane_state->uapi.alpha;
	plane_state->hw.pixel_blend_mode =
		from_plane_state->uapi.pixel_blend_mode;
	plane_state->hw.rotation = from_plane_state->uapi.rotation;
	plane_state->hw.color_encoding = from_plane_state->uapi.color_encoding;
	plane_state->hw.color_range = from_plane_state->uapi.color_range;
	plane_state->hw.scaling_filter = from_plane_state->uapi.scaling_filter;

	plane_state->uapi.src = drm_plane_state_src(&from_plane_state->uapi);
	plane_state->uapi.dst = drm_plane_state_dest(&from_plane_state->uapi);
}

void intel_plane_copy_hw_state(struct intel_plane_state *plane_state,
			       const struct intel_plane_state *from_plane_state)
{
	intel_plane_clear_hw_state(plane_state);

	memcpy(&plane_state->hw, &from_plane_state->hw,
	       sizeof(plane_state->hw));

	if (plane_state->hw.fb)
		drm_framebuffer_get(plane_state->hw.fb);
}

void intel_plane_set_invisible(struct intel_crtc_state *crtc_state,
			       struct intel_plane_state *plane_state)
{
	struct intel_plane *plane = to_intel_plane(plane_state->uapi.plane);

	crtc_state->active_planes &= ~BIT(plane->id);
	crtc_state->scaled_planes &= ~BIT(plane->id);
	crtc_state->nv12_planes &= ~BIT(plane->id);
	crtc_state->c8_planes &= ~BIT(plane->id);
	crtc_state->async_flip_planes &= ~BIT(plane->id);
	crtc_state->data_rate[plane->id] = 0;
	crtc_state->data_rate_y[plane->id] = 0;
	crtc_state->rel_data_rate[plane->id] = 0;
	crtc_state->rel_data_rate_y[plane->id] = 0;
	crtc_state->min_cdclk[plane->id] = 0;

	plane_state->uapi.visible = false;
}

 
static bool intel_wm_need_update(const struct intel_plane_state *cur,
				 struct intel_plane_state *new)
{
	 
	if (new->uapi.visible != cur->uapi.visible)
		return true;

	if (!cur->hw.fb || !new->hw.fb)
		return false;

	if (cur->hw.fb->modifier != new->hw.fb->modifier ||
	    cur->hw.rotation != new->hw.rotation ||
	    drm_rect_width(&new->uapi.src) != drm_rect_width(&cur->uapi.src) ||
	    drm_rect_height(&new->uapi.src) != drm_rect_height(&cur->uapi.src) ||
	    drm_rect_width(&new->uapi.dst) != drm_rect_width(&cur->uapi.dst) ||
	    drm_rect_height(&new->uapi.dst) != drm_rect_height(&cur->uapi.dst))
		return true;

	return false;
}

static bool intel_plane_is_scaled(const struct intel_plane_state *plane_state)
{
	int src_w = drm_rect_width(&plane_state->uapi.src) >> 16;
	int src_h = drm_rect_height(&plane_state->uapi.src) >> 16;
	int dst_w = drm_rect_width(&plane_state->uapi.dst);
	int dst_h = drm_rect_height(&plane_state->uapi.dst);

	return src_w != dst_w || src_h != dst_h;
}

static bool intel_plane_do_async_flip(struct intel_plane *plane,
				      const struct intel_crtc_state *old_crtc_state,
				      const struct intel_crtc_state *new_crtc_state)
{
	struct drm_i915_private *i915 = to_i915(plane->base.dev);

	if (!plane->async_flip)
		return false;

	if (!new_crtc_state->uapi.async_flip)
		return false;

	 
	return DISPLAY_VER(i915) < 13 || old_crtc_state->uapi.async_flip;
}

static bool i9xx_must_disable_cxsr(const struct intel_crtc_state *new_crtc_state,
				   const struct intel_plane_state *old_plane_state,
				   const struct intel_plane_state *new_plane_state)
{
	struct intel_plane *plane = to_intel_plane(new_plane_state->uapi.plane);
	bool old_visible = old_plane_state->uapi.visible;
	bool new_visible = new_plane_state->uapi.visible;
	u32 old_ctl = old_plane_state->ctl;
	u32 new_ctl = new_plane_state->ctl;
	bool modeset, turn_on, turn_off;

	if (plane->id == PLANE_CURSOR)
		return false;

	modeset = intel_crtc_needs_modeset(new_crtc_state);
	turn_off = old_visible && (!new_visible || modeset);
	turn_on = new_visible && (!old_visible || modeset);

	 
	if (turn_on || turn_off)
		return true;

	if (!old_visible || !new_visible)
		return false;

	 
	if (plane->id == PLANE_PRIMARY) {
		old_ctl &= ~DISP_TILED;
		new_ctl &= ~DISP_TILED;
	}

	return old_ctl != new_ctl;
}

static int intel_plane_atomic_calc_changes(const struct intel_crtc_state *old_crtc_state,
					   struct intel_crtc_state *new_crtc_state,
					   const struct intel_plane_state *old_plane_state,
					   struct intel_plane_state *new_plane_state)
{
	struct intel_crtc *crtc = to_intel_crtc(new_crtc_state->uapi.crtc);
	struct intel_plane *plane = to_intel_plane(new_plane_state->uapi.plane);
	struct drm_i915_private *dev_priv = to_i915(crtc->base.dev);
	bool mode_changed = intel_crtc_needs_modeset(new_crtc_state);
	bool was_crtc_enabled = old_crtc_state->hw.active;
	bool is_crtc_enabled = new_crtc_state->hw.active;
	bool turn_off, turn_on, visible, was_visible;
	int ret;

	if (DISPLAY_VER(dev_priv) >= 9 && plane->id != PLANE_CURSOR) {
		ret = skl_update_scaler_plane(new_crtc_state, new_plane_state);
		if (ret)
			return ret;
	}

	was_visible = old_plane_state->uapi.visible;
	visible = new_plane_state->uapi.visible;

	if (!was_crtc_enabled && drm_WARN_ON(&dev_priv->drm, was_visible))
		was_visible = false;

	 
	if (!is_crtc_enabled) {
		intel_plane_set_invisible(new_crtc_state, new_plane_state);
		visible = false;
	}

	if (!was_visible && !visible)
		return 0;

	turn_off = was_visible && (!visible || mode_changed);
	turn_on = visible && (!was_visible || mode_changed);

	drm_dbg_atomic(&dev_priv->drm,
		       "[CRTC:%d:%s] with [PLANE:%d:%s] visible %i -> %i, off %i, on %i, ms %i\n",
		       crtc->base.base.id, crtc->base.name,
		       plane->base.base.id, plane->base.name,
		       was_visible, visible,
		       turn_off, turn_on, mode_changed);

	if (turn_on) {
		if (DISPLAY_VER(dev_priv) < 5 && !IS_G4X(dev_priv))
			new_crtc_state->update_wm_pre = true;
	} else if (turn_off) {
		if (DISPLAY_VER(dev_priv) < 5 && !IS_G4X(dev_priv))
			new_crtc_state->update_wm_post = true;
	} else if (intel_wm_need_update(old_plane_state, new_plane_state)) {
		if (DISPLAY_VER(dev_priv) < 5 && !IS_G4X(dev_priv)) {
			 
			new_crtc_state->update_wm_pre = true;
			new_crtc_state->update_wm_post = true;
		}
	}

	if (visible || was_visible)
		new_crtc_state->fb_bits |= plane->frontbuffer_bit;

	if (HAS_GMCH(dev_priv) &&
	    i9xx_must_disable_cxsr(new_crtc_state, old_plane_state, new_plane_state))
		new_crtc_state->disable_cxsr = true;

	 
	if (plane->id != PLANE_CURSOR &&
	    (IS_IRONLAKE(dev_priv) || IS_SANDYBRIDGE(dev_priv) ||
	     IS_IVYBRIDGE(dev_priv)) &&
	    (turn_on || (!intel_plane_is_scaled(old_plane_state) &&
			 intel_plane_is_scaled(new_plane_state))))
		new_crtc_state->disable_lp_wm = true;

	if (intel_plane_do_async_flip(plane, old_crtc_state, new_crtc_state)) {
		new_crtc_state->do_async_flip = true;
		new_crtc_state->async_flip_planes |= BIT(plane->id);
	}

	return 0;
}

int intel_plane_atomic_check_with_state(const struct intel_crtc_state *old_crtc_state,
					struct intel_crtc_state *new_crtc_state,
					const struct intel_plane_state *old_plane_state,
					struct intel_plane_state *new_plane_state)
{
	struct intel_plane *plane = to_intel_plane(new_plane_state->uapi.plane);
	const struct drm_framebuffer *fb = new_plane_state->hw.fb;
	int ret;

	intel_plane_set_invisible(new_crtc_state, new_plane_state);
	new_crtc_state->enabled_planes &= ~BIT(plane->id);

	if (!new_plane_state->hw.crtc && !old_plane_state->hw.crtc)
		return 0;

	ret = plane->check_plane(new_crtc_state, new_plane_state);
	if (ret)
		return ret;

	if (fb)
		new_crtc_state->enabled_planes |= BIT(plane->id);

	 
	if (new_plane_state->uapi.visible)
		new_crtc_state->active_planes |= BIT(plane->id);

	if (new_plane_state->uapi.visible &&
	    intel_plane_is_scaled(new_plane_state))
		new_crtc_state->scaled_planes |= BIT(plane->id);

	if (new_plane_state->uapi.visible &&
	    intel_format_info_is_yuv_semiplanar(fb->format, fb->modifier))
		new_crtc_state->nv12_planes |= BIT(plane->id);

	if (new_plane_state->uapi.visible &&
	    fb->format->format == DRM_FORMAT_C8)
		new_crtc_state->c8_planes |= BIT(plane->id);

	if (new_plane_state->uapi.visible || old_plane_state->uapi.visible)
		new_crtc_state->update_planes |= BIT(plane->id);

	if (new_plane_state->uapi.visible &&
	    intel_format_info_is_yuv_semiplanar(fb->format, fb->modifier)) {
		new_crtc_state->data_rate_y[plane->id] =
			intel_plane_data_rate(new_crtc_state, new_plane_state, 0);
		new_crtc_state->data_rate[plane->id] =
			intel_plane_data_rate(new_crtc_state, new_plane_state, 1);

		new_crtc_state->rel_data_rate_y[plane->id] =
			intel_plane_relative_data_rate(new_crtc_state,
						       new_plane_state, 0);
		new_crtc_state->rel_data_rate[plane->id] =
			intel_plane_relative_data_rate(new_crtc_state,
						       new_plane_state, 1);
	} else if (new_plane_state->uapi.visible) {
		new_crtc_state->data_rate[plane->id] =
			intel_plane_data_rate(new_crtc_state, new_plane_state, 0);

		new_crtc_state->rel_data_rate[plane->id] =
			intel_plane_relative_data_rate(new_crtc_state,
						       new_plane_state, 0);
	}

	return intel_plane_atomic_calc_changes(old_crtc_state, new_crtc_state,
					       old_plane_state, new_plane_state);
}

static struct intel_plane *
intel_crtc_get_plane(struct intel_crtc *crtc, enum plane_id plane_id)
{
	struct drm_i915_private *i915 = to_i915(crtc->base.dev);
	struct intel_plane *plane;

	for_each_intel_plane_on_crtc(&i915->drm, crtc, plane) {
		if (plane->id == plane_id)
			return plane;
	}

	return NULL;
}

int intel_plane_atomic_check(struct intel_atomic_state *state,
			     struct intel_plane *plane)
{
	struct drm_i915_private *i915 = to_i915(state->base.dev);
	struct intel_plane_state *new_plane_state =
		intel_atomic_get_new_plane_state(state, plane);
	const struct intel_plane_state *old_plane_state =
		intel_atomic_get_old_plane_state(state, plane);
	const struct intel_plane_state *new_master_plane_state;
	struct intel_crtc *crtc = intel_crtc_for_pipe(i915, plane->pipe);
	const struct intel_crtc_state *old_crtc_state =
		intel_atomic_get_old_crtc_state(state, crtc);
	struct intel_crtc_state *new_crtc_state =
		intel_atomic_get_new_crtc_state(state, crtc);

	if (new_crtc_state && intel_crtc_is_bigjoiner_slave(new_crtc_state)) {
		struct intel_crtc *master_crtc =
			intel_master_crtc(new_crtc_state);
		struct intel_plane *master_plane =
			intel_crtc_get_plane(master_crtc, plane->id);

		new_master_plane_state =
			intel_atomic_get_new_plane_state(state, master_plane);
	} else {
		new_master_plane_state = new_plane_state;
	}

	intel_plane_copy_uapi_to_hw_state(new_plane_state,
					  new_master_plane_state,
					  crtc);

	new_plane_state->uapi.visible = false;
	if (!new_crtc_state)
		return 0;

	return intel_plane_atomic_check_with_state(old_crtc_state,
						   new_crtc_state,
						   old_plane_state,
						   new_plane_state);
}

static struct intel_plane *
skl_next_plane_to_commit(struct intel_atomic_state *state,
			 struct intel_crtc *crtc,
			 struct skl_ddb_entry ddb[I915_MAX_PLANES],
			 struct skl_ddb_entry ddb_y[I915_MAX_PLANES],
			 unsigned int *update_mask)
{
	struct intel_crtc_state *crtc_state =
		intel_atomic_get_new_crtc_state(state, crtc);
	struct intel_plane_state __maybe_unused *plane_state;
	struct intel_plane *plane;
	int i;

	if (*update_mask == 0)
		return NULL;

	for_each_new_intel_plane_in_state(state, plane, plane_state, i) {
		enum plane_id plane_id = plane->id;

		if (crtc->pipe != plane->pipe ||
		    !(*update_mask & BIT(plane_id)))
			continue;

		if (skl_ddb_allocation_overlaps(&crtc_state->wm.skl.plane_ddb[plane_id],
						ddb, I915_MAX_PLANES, plane_id) ||
		    skl_ddb_allocation_overlaps(&crtc_state->wm.skl.plane_ddb_y[plane_id],
						ddb_y, I915_MAX_PLANES, plane_id))
			continue;

		*update_mask &= ~BIT(plane_id);
		ddb[plane_id] = crtc_state->wm.skl.plane_ddb[plane_id];
		ddb_y[plane_id] = crtc_state->wm.skl.plane_ddb_y[plane_id];

		return plane;
	}

	 
	drm_WARN_ON(state->base.dev, 1);

	return NULL;
}

void intel_plane_update_noarm(struct intel_plane *plane,
			      const struct intel_crtc_state *crtc_state,
			      const struct intel_plane_state *plane_state)
{
	struct intel_crtc *crtc = to_intel_crtc(crtc_state->uapi.crtc);

	trace_intel_plane_update_noarm(plane, crtc);

	if (plane->update_noarm)
		plane->update_noarm(plane, crtc_state, plane_state);
}

void intel_plane_update_arm(struct intel_plane *plane,
			    const struct intel_crtc_state *crtc_state,
			    const struct intel_plane_state *plane_state)
{
	struct intel_crtc *crtc = to_intel_crtc(crtc_state->uapi.crtc);

	trace_intel_plane_update_arm(plane, crtc);

	if (crtc_state->do_async_flip && plane->async_flip)
		plane->async_flip(plane, crtc_state, plane_state, true);
	else
		plane->update_arm(plane, crtc_state, plane_state);
}

void intel_plane_disable_arm(struct intel_plane *plane,
			     const struct intel_crtc_state *crtc_state)
{
	struct intel_crtc *crtc = to_intel_crtc(crtc_state->uapi.crtc);

	trace_intel_plane_disable_arm(plane, crtc);
	plane->disable_arm(plane, crtc_state);
}

void intel_crtc_planes_update_noarm(struct intel_atomic_state *state,
				    struct intel_crtc *crtc)
{
	struct intel_crtc_state *new_crtc_state =
		intel_atomic_get_new_crtc_state(state, crtc);
	u32 update_mask = new_crtc_state->update_planes;
	struct intel_plane_state *new_plane_state;
	struct intel_plane *plane;
	int i;

	if (new_crtc_state->do_async_flip)
		return;

	 
	for_each_new_intel_plane_in_state(state, plane, new_plane_state, i) {
		if (crtc->pipe != plane->pipe ||
		    !(update_mask & BIT(plane->id)))
			continue;

		 
		if (new_plane_state->uapi.visible ||
		    new_plane_state->planar_slave)
			intel_plane_update_noarm(plane, new_crtc_state, new_plane_state);
	}
}

static void skl_crtc_planes_update_arm(struct intel_atomic_state *state,
				       struct intel_crtc *crtc)
{
	struct intel_crtc_state *old_crtc_state =
		intel_atomic_get_old_crtc_state(state, crtc);
	struct intel_crtc_state *new_crtc_state =
		intel_atomic_get_new_crtc_state(state, crtc);
	struct skl_ddb_entry ddb[I915_MAX_PLANES];
	struct skl_ddb_entry ddb_y[I915_MAX_PLANES];
	u32 update_mask = new_crtc_state->update_planes;
	struct intel_plane *plane;

	memcpy(ddb, old_crtc_state->wm.skl.plane_ddb,
	       sizeof(old_crtc_state->wm.skl.plane_ddb));
	memcpy(ddb_y, old_crtc_state->wm.skl.plane_ddb_y,
	       sizeof(old_crtc_state->wm.skl.plane_ddb_y));

	while ((plane = skl_next_plane_to_commit(state, crtc, ddb, ddb_y, &update_mask))) {
		struct intel_plane_state *new_plane_state =
			intel_atomic_get_new_plane_state(state, plane);

		 
		if (new_plane_state->uapi.visible ||
		    new_plane_state->planar_slave)
			intel_plane_update_arm(plane, new_crtc_state, new_plane_state);
		else
			intel_plane_disable_arm(plane, new_crtc_state);
	}
}

static void i9xx_crtc_planes_update_arm(struct intel_atomic_state *state,
					struct intel_crtc *crtc)
{
	struct intel_crtc_state *new_crtc_state =
		intel_atomic_get_new_crtc_state(state, crtc);
	u32 update_mask = new_crtc_state->update_planes;
	struct intel_plane_state *new_plane_state;
	struct intel_plane *plane;
	int i;

	for_each_new_intel_plane_in_state(state, plane, new_plane_state, i) {
		if (crtc->pipe != plane->pipe ||
		    !(update_mask & BIT(plane->id)))
			continue;

		 
		if (new_plane_state->uapi.visible)
			intel_plane_update_arm(plane, new_crtc_state, new_plane_state);
		else
			intel_plane_disable_arm(plane, new_crtc_state);
	}
}

void intel_crtc_planes_update_arm(struct intel_atomic_state *state,
				  struct intel_crtc *crtc)
{
	struct drm_i915_private *i915 = to_i915(state->base.dev);

	if (DISPLAY_VER(i915) >= 9)
		skl_crtc_planes_update_arm(state, crtc);
	else
		i9xx_crtc_planes_update_arm(state, crtc);
}

int intel_atomic_plane_check_clipping(struct intel_plane_state *plane_state,
				      struct intel_crtc_state *crtc_state,
				      int min_scale, int max_scale,
				      bool can_position)
{
	struct drm_i915_private *i915 = to_i915(plane_state->uapi.plane->dev);
	struct drm_framebuffer *fb = plane_state->hw.fb;
	struct drm_rect *src = &plane_state->uapi.src;
	struct drm_rect *dst = &plane_state->uapi.dst;
	const struct drm_rect *clip = &crtc_state->pipe_src;
	unsigned int rotation = plane_state->hw.rotation;
	int hscale, vscale;

	if (!fb) {
		plane_state->uapi.visible = false;
		return 0;
	}

	drm_rect_rotate(src, fb->width << 16, fb->height << 16, rotation);

	 
	hscale = drm_rect_calc_hscale(src, dst, min_scale, max_scale);
	vscale = drm_rect_calc_vscale(src, dst, min_scale, max_scale);
	if (hscale < 0 || vscale < 0) {
		drm_dbg_kms(&i915->drm, "Invalid scaling of plane\n");
		drm_rect_debug_print("src: ", src, true);
		drm_rect_debug_print("dst: ", dst, false);
		return -ERANGE;
	}

	 
	plane_state->uapi.visible = drm_rect_clip_scaled(src, dst, clip);

	drm_rect_rotate_inv(src, fb->width << 16, fb->height << 16, rotation);

	if (!can_position && plane_state->uapi.visible &&
	    !drm_rect_equals(dst, clip)) {
		drm_dbg_kms(&i915->drm, "Plane must cover entire CRTC\n");
		drm_rect_debug_print("dst: ", dst, false);
		drm_rect_debug_print("clip: ", clip, false);
		return -EINVAL;
	}

	 
	drm_rect_translate(dst, -clip->x1, -clip->y1);

	return 0;
}

int intel_plane_check_src_coordinates(struct intel_plane_state *plane_state)
{
	struct drm_i915_private *i915 = to_i915(plane_state->uapi.plane->dev);
	const struct drm_framebuffer *fb = plane_state->hw.fb;
	struct drm_rect *src = &plane_state->uapi.src;
	u32 src_x, src_y, src_w, src_h, hsub, vsub;
	bool rotated = drm_rotation_90_or_270(plane_state->hw.rotation);

	 
	if (fb->modifier == I915_FORMAT_MOD_Y_TILED_CCS ||
	    fb->modifier == I915_FORMAT_MOD_Yf_TILED_CCS)
		return 0;

	 
	src_x = src->x1 >> 16;
	src_w = drm_rect_width(src) >> 16;
	src_y = src->y1 >> 16;
	src_h = drm_rect_height(src) >> 16;

	drm_rect_init(src, src_x << 16, src_y << 16,
		      src_w << 16, src_h << 16);

	if (fb->format->format == DRM_FORMAT_RGB565 && rotated) {
		hsub = 2;
		vsub = 2;
	} else {
		hsub = fb->format->hsub;
		vsub = fb->format->vsub;
	}

	if (rotated)
		hsub = vsub = max(hsub, vsub);

	if (src_x % hsub || src_w % hsub) {
		drm_dbg_kms(&i915->drm, "src x/w (%u, %u) must be a multiple of %u (rotated: %s)\n",
			    src_x, src_w, hsub, str_yes_no(rotated));
		return -EINVAL;
	}

	if (src_y % vsub || src_h % vsub) {
		drm_dbg_kms(&i915->drm, "src y/h (%u, %u) must be a multiple of %u (rotated: %s)\n",
			    src_y, src_h, vsub, str_yes_no(rotated));
		return -EINVAL;
	}

	return 0;
}

 
static int
intel_prepare_plane_fb(struct drm_plane *_plane,
		       struct drm_plane_state *_new_plane_state)
{
	struct i915_sched_attr attr = { .priority = I915_PRIORITY_DISPLAY };
	struct intel_plane *plane = to_intel_plane(_plane);
	struct intel_plane_state *new_plane_state =
		to_intel_plane_state(_new_plane_state);
	struct intel_atomic_state *state =
		to_intel_atomic_state(new_plane_state->uapi.state);
	struct drm_i915_private *dev_priv = to_i915(plane->base.dev);
	const struct intel_plane_state *old_plane_state =
		intel_atomic_get_old_plane_state(state, plane);
	struct drm_i915_gem_object *obj = intel_fb_obj(new_plane_state->hw.fb);
	struct drm_i915_gem_object *old_obj = intel_fb_obj(old_plane_state->hw.fb);
	int ret;

	if (old_obj) {
		const struct intel_crtc_state *new_crtc_state =
			intel_atomic_get_new_crtc_state(state,
							to_intel_crtc(old_plane_state->hw.crtc));

		 
		if (new_crtc_state && intel_crtc_needs_modeset(new_crtc_state)) {
			ret = i915_sw_fence_await_reservation(&state->commit_ready,
							      old_obj->base.resv,
							      false, 0,
							      GFP_KERNEL);
			if (ret < 0)
				return ret;
		}
	}

	if (new_plane_state->uapi.fence) {  
		i915_gem_fence_wait_priority(new_plane_state->uapi.fence,
					     &attr);
		ret = i915_sw_fence_await_dma_fence(&state->commit_ready,
						    new_plane_state->uapi.fence,
						    i915_fence_timeout(dev_priv),
						    GFP_KERNEL);
		if (ret < 0)
			return ret;
	}

	if (!obj)
		return 0;


	ret = intel_plane_pin_fb(new_plane_state);
	if (ret)
		return ret;

	i915_gem_object_wait_priority(obj, 0, &attr);

	if (!new_plane_state->uapi.fence) {  
		struct dma_resv_iter cursor;
		struct dma_fence *fence;

		ret = i915_sw_fence_await_reservation(&state->commit_ready,
						      obj->base.resv, false,
						      i915_fence_timeout(dev_priv),
						      GFP_KERNEL);
		if (ret < 0)
			goto unpin_fb;

		dma_resv_iter_begin(&cursor, obj->base.resv,
				    DMA_RESV_USAGE_WRITE);
		dma_resv_for_each_fence_unlocked(&cursor, fence) {
			intel_display_rps_boost_after_vblank(new_plane_state->hw.crtc,
							     fence);
		}
		dma_resv_iter_end(&cursor);
	} else {
		intel_display_rps_boost_after_vblank(new_plane_state->hw.crtc,
						     new_plane_state->uapi.fence);
	}

	 
	intel_display_rps_mark_interactive(dev_priv, state, true);

	return 0;

unpin_fb:
	intel_plane_unpin_fb(new_plane_state);

	return ret;
}

 
static void
intel_cleanup_plane_fb(struct drm_plane *plane,
		       struct drm_plane_state *_old_plane_state)
{
	struct intel_plane_state *old_plane_state =
		to_intel_plane_state(_old_plane_state);
	struct intel_atomic_state *state =
		to_intel_atomic_state(old_plane_state->uapi.state);
	struct drm_i915_private *dev_priv = to_i915(plane->dev);
	struct drm_i915_gem_object *obj = intel_fb_obj(old_plane_state->hw.fb);

	if (!obj)
		return;

	intel_display_rps_mark_interactive(dev_priv, state, false);

	 
	intel_plane_unpin_fb(old_plane_state);
}

static const struct drm_plane_helper_funcs intel_plane_helper_funcs = {
	.prepare_fb = intel_prepare_plane_fb,
	.cleanup_fb = intel_cleanup_plane_fb,
};

void intel_plane_helper_add(struct intel_plane *plane)
{
	drm_plane_helper_add(&plane->base, &intel_plane_helper_funcs);
}
