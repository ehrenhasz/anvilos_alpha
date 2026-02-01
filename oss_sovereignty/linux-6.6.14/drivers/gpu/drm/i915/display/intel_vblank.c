
 

#include "i915_drv.h"
#include "i915_reg.h"
#include "intel_de.h"
#include "intel_display_types.h"
#include "intel_vblank.h"
#include "intel_vrr.h"

 

 
u32 i915_get_vblank_counter(struct drm_crtc *crtc)
{
	struct drm_i915_private *dev_priv = to_i915(crtc->dev);
	struct drm_vblank_crtc *vblank = &dev_priv->drm.vblank[drm_crtc_index(crtc)];
	const struct drm_display_mode *mode = &vblank->hwmode;
	enum pipe pipe = to_intel_crtc(crtc)->pipe;
	u32 pixel, vbl_start, hsync_start, htotal;
	u64 frame;

	 
	if (!vblank->max_vblank_count)
		return 0;

	htotal = mode->crtc_htotal;
	hsync_start = mode->crtc_hsync_start;
	vbl_start = mode->crtc_vblank_start;
	if (mode->flags & DRM_MODE_FLAG_INTERLACE)
		vbl_start = DIV_ROUND_UP(vbl_start, 2);

	 
	vbl_start *= htotal;

	 
	vbl_start -= htotal - hsync_start;

	 
	frame = intel_de_read64_2x32(dev_priv, PIPEFRAMEPIXEL(pipe), PIPEFRAME(pipe));

	pixel = frame & PIPE_PIXEL_MASK;
	frame = (frame >> PIPE_FRAME_LOW_SHIFT) & 0xffffff;

	 
	return (frame + (pixel >= vbl_start)) & 0xffffff;
}

u32 g4x_get_vblank_counter(struct drm_crtc *crtc)
{
	struct drm_i915_private *dev_priv = to_i915(crtc->dev);
	struct drm_vblank_crtc *vblank = &dev_priv->drm.vblank[drm_crtc_index(crtc)];
	enum pipe pipe = to_intel_crtc(crtc)->pipe;

	if (!vblank->max_vblank_count)
		return 0;

	return intel_de_read(dev_priv, PIPE_FRMCOUNT_G4X(pipe));
}

static u32 intel_crtc_scanlines_since_frame_timestamp(struct intel_crtc *crtc)
{
	struct drm_i915_private *dev_priv = to_i915(crtc->base.dev);
	struct drm_vblank_crtc *vblank =
		&crtc->base.dev->vblank[drm_crtc_index(&crtc->base)];
	const struct drm_display_mode *mode = &vblank->hwmode;
	u32 htotal = mode->crtc_htotal;
	u32 clock = mode->crtc_clock;
	u32 scan_prev_time, scan_curr_time, scan_post_time;

	 
	do {
		 
		scan_prev_time = intel_de_read_fw(dev_priv,
						  PIPE_FRMTMSTMP(crtc->pipe));

		 
		scan_curr_time = intel_de_read_fw(dev_priv, IVB_TIMESTAMP_CTR);

		scan_post_time = intel_de_read_fw(dev_priv,
						  PIPE_FRMTMSTMP(crtc->pipe));
	} while (scan_post_time != scan_prev_time);

	return div_u64(mul_u32_u32(scan_curr_time - scan_prev_time,
				   clock), 1000 * htotal);
}

 
static u32 __intel_get_crtc_scanline_from_timestamp(struct intel_crtc *crtc)
{
	struct drm_vblank_crtc *vblank =
		&crtc->base.dev->vblank[drm_crtc_index(&crtc->base)];
	const struct drm_display_mode *mode = &vblank->hwmode;
	u32 vblank_start = mode->crtc_vblank_start;
	u32 vtotal = mode->crtc_vtotal;
	u32 scanline;

	scanline = intel_crtc_scanlines_since_frame_timestamp(crtc);
	scanline = min(scanline, vtotal - 1);
	scanline = (scanline + vblank_start) % vtotal;

	return scanline;
}

 
static int __intel_get_crtc_scanline(struct intel_crtc *crtc)
{
	struct drm_device *dev = crtc->base.dev;
	struct drm_i915_private *dev_priv = to_i915(dev);
	const struct drm_display_mode *mode;
	struct drm_vblank_crtc *vblank;
	enum pipe pipe = crtc->pipe;
	int position, vtotal;

	if (!crtc->active)
		return 0;

	vblank = &crtc->base.dev->vblank[drm_crtc_index(&crtc->base)];
	mode = &vblank->hwmode;

	if (crtc->mode_flags & I915_MODE_FLAG_GET_SCANLINE_FROM_TIMESTAMP)
		return __intel_get_crtc_scanline_from_timestamp(crtc);

	vtotal = mode->crtc_vtotal;
	if (mode->flags & DRM_MODE_FLAG_INTERLACE)
		vtotal /= 2;

	position = intel_de_read_fw(dev_priv, PIPEDSL(pipe)) & PIPEDSL_LINE_MASK;

	 
	if (HAS_DDI(dev_priv) && !position) {
		int i, temp;

		for (i = 0; i < 100; i++) {
			udelay(1);
			temp = intel_de_read_fw(dev_priv, PIPEDSL(pipe)) & PIPEDSL_LINE_MASK;
			if (temp != position) {
				position = temp;
				break;
			}
		}
	}

	 
	return (position + crtc->scanline_offset) % vtotal;
}

static bool i915_get_crtc_scanoutpos(struct drm_crtc *_crtc,
				     bool in_vblank_irq,
				     int *vpos, int *hpos,
				     ktime_t *stime, ktime_t *etime,
				     const struct drm_display_mode *mode)
{
	struct drm_device *dev = _crtc->dev;
	struct drm_i915_private *dev_priv = to_i915(dev);
	struct intel_crtc *crtc = to_intel_crtc(_crtc);
	enum pipe pipe = crtc->pipe;
	int position;
	int vbl_start, vbl_end, hsync_start, htotal, vtotal;
	unsigned long irqflags;
	bool use_scanline_counter = DISPLAY_VER(dev_priv) >= 5 ||
		IS_G4X(dev_priv) || DISPLAY_VER(dev_priv) == 2 ||
		crtc->mode_flags & I915_MODE_FLAG_USE_SCANLINE_COUNTER;

	if (drm_WARN_ON(&dev_priv->drm, !mode->crtc_clock)) {
		drm_dbg(&dev_priv->drm,
			"trying to get scanoutpos for disabled pipe %c\n",
			pipe_name(pipe));
		return false;
	}

	htotal = mode->crtc_htotal;
	hsync_start = mode->crtc_hsync_start;
	vtotal = mode->crtc_vtotal;
	vbl_start = mode->crtc_vblank_start;
	vbl_end = mode->crtc_vblank_end;

	if (mode->flags & DRM_MODE_FLAG_INTERLACE) {
		vbl_start = DIV_ROUND_UP(vbl_start, 2);
		vbl_end /= 2;
		vtotal /= 2;
	}

	 
	spin_lock_irqsave(&dev_priv->uncore.lock, irqflags);

	 

	 
	if (stime)
		*stime = ktime_get();

	if (crtc->mode_flags & I915_MODE_FLAG_VRR) {
		int scanlines = intel_crtc_scanlines_since_frame_timestamp(crtc);

		position = __intel_get_crtc_scanline(crtc);

		 
		if (position >= vbl_start && scanlines < position)
			position = min(crtc->vmax_vblank_start + scanlines, vtotal - 1);
	} else if (use_scanline_counter) {
		 
		position = __intel_get_crtc_scanline(crtc);
	} else {
		 
		position = (intel_de_read_fw(dev_priv, PIPEFRAMEPIXEL(pipe)) & PIPE_PIXEL_MASK) >> PIPE_PIXEL_SHIFT;

		 
		vbl_start *= htotal;
		vbl_end *= htotal;
		vtotal *= htotal;

		 
		position = min(position, vtotal - 1);

		 
		position = (position + htotal - hsync_start) % vtotal;
	}

	 
	if (etime)
		*etime = ktime_get();

	 

	spin_unlock_irqrestore(&dev_priv->uncore.lock, irqflags);

	 
	if (position >= vbl_start)
		position -= vbl_end;
	else
		position += vtotal - vbl_end;

	if (use_scanline_counter) {
		*vpos = position;
		*hpos = 0;
	} else {
		*vpos = position / htotal;
		*hpos = position - (*vpos * htotal);
	}

	return true;
}

bool intel_crtc_get_vblank_timestamp(struct drm_crtc *crtc, int *max_error,
				     ktime_t *vblank_time, bool in_vblank_irq)
{
	return drm_crtc_vblank_helper_get_vblank_timestamp_internal(
		crtc, max_error, vblank_time, in_vblank_irq,
		i915_get_crtc_scanoutpos);
}

int intel_get_crtc_scanline(struct intel_crtc *crtc)
{
	struct drm_i915_private *dev_priv = to_i915(crtc->base.dev);
	unsigned long irqflags;
	int position;

	spin_lock_irqsave(&dev_priv->uncore.lock, irqflags);
	position = __intel_get_crtc_scanline(crtc);
	spin_unlock_irqrestore(&dev_priv->uncore.lock, irqflags);

	return position;
}

static bool pipe_scanline_is_moving(struct drm_i915_private *dev_priv,
				    enum pipe pipe)
{
	i915_reg_t reg = PIPEDSL(pipe);
	u32 line1, line2;

	line1 = intel_de_read(dev_priv, reg) & PIPEDSL_LINE_MASK;
	msleep(5);
	line2 = intel_de_read(dev_priv, reg) & PIPEDSL_LINE_MASK;

	return line1 != line2;
}

static void wait_for_pipe_scanline_moving(struct intel_crtc *crtc, bool state)
{
	struct drm_i915_private *dev_priv = to_i915(crtc->base.dev);
	enum pipe pipe = crtc->pipe;

	 
	if (wait_for(pipe_scanline_is_moving(dev_priv, pipe) == state, 100))
		drm_err(&dev_priv->drm,
			"pipe %c scanline %s wait timed out\n",
			pipe_name(pipe), str_on_off(state));
}

void intel_wait_for_pipe_scanline_stopped(struct intel_crtc *crtc)
{
	wait_for_pipe_scanline_moving(crtc, false);
}

void intel_wait_for_pipe_scanline_moving(struct intel_crtc *crtc)
{
	wait_for_pipe_scanline_moving(crtc, true);
}

static int intel_crtc_scanline_offset(const struct intel_crtc_state *crtc_state)
{
	struct drm_i915_private *i915 = to_i915(crtc_state->uapi.crtc->dev);
	const struct drm_display_mode *adjusted_mode = &crtc_state->hw.adjusted_mode;

	 
	if (DISPLAY_VER(i915) == 2) {
		int vtotal;

		vtotal = adjusted_mode->crtc_vtotal;
		if (adjusted_mode->flags & DRM_MODE_FLAG_INTERLACE)
			vtotal /= 2;

		return vtotal - 1;
	} else if (HAS_DDI(i915) && intel_crtc_has_type(crtc_state, INTEL_OUTPUT_HDMI)) {
		return 2;
	} else {
		return 1;
	}
}

void intel_crtc_update_active_timings(const struct intel_crtc_state *crtc_state,
				      bool vrr_enable)
{
	struct intel_crtc *crtc = to_intel_crtc(crtc_state->uapi.crtc);
	struct drm_i915_private *i915 = to_i915(crtc->base.dev);
	u8 mode_flags = crtc_state->mode_flags;
	struct drm_display_mode adjusted_mode;
	int vmax_vblank_start = 0;
	unsigned long irqflags;

	drm_mode_init(&adjusted_mode, &crtc_state->hw.adjusted_mode);

	if (vrr_enable) {
		drm_WARN_ON(&i915->drm, (mode_flags & I915_MODE_FLAG_VRR) == 0);

		adjusted_mode.crtc_vtotal = crtc_state->vrr.vmax;
		adjusted_mode.crtc_vblank_end = crtc_state->vrr.vmax;
		adjusted_mode.crtc_vblank_start = intel_vrr_vmin_vblank_start(crtc_state);
		vmax_vblank_start = intel_vrr_vmax_vblank_start(crtc_state);
	} else {
		mode_flags &= ~I915_MODE_FLAG_VRR;
	}

	 
	spin_lock_irqsave(&i915->drm.vblank_time_lock, irqflags);
	spin_lock(&i915->uncore.lock);

	drm_calc_timestamping_constants(&crtc->base, &adjusted_mode);

	crtc->vmax_vblank_start = vmax_vblank_start;

	crtc->mode_flags = mode_flags;

	crtc->scanline_offset = intel_crtc_scanline_offset(crtc_state);

	spin_unlock(&i915->uncore.lock);
	spin_unlock_irqrestore(&i915->drm.vblank_time_lock, irqflags);
}
