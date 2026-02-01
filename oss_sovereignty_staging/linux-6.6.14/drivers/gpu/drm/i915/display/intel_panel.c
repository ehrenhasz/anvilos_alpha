 

#include <linux/kernel.h>
#include <linux/pwm.h>

#include <drm/drm_edid.h>

#include "i915_reg.h"
#include "intel_backlight.h"
#include "intel_connector.h"
#include "intel_de.h"
#include "intel_display_types.h"
#include "intel_drrs.h"
#include "intel_lvds_regs.h"
#include "intel_panel.h"
#include "intel_quirks.h"
#include "intel_vrr.h"

bool intel_panel_use_ssc(struct drm_i915_private *i915)
{
	if (i915->params.panel_use_ssc >= 0)
		return i915->params.panel_use_ssc != 0;
	return i915->display.vbt.lvds_use_ssc &&
		!intel_has_quirk(i915, QUIRK_LVDS_SSC_DISABLE);
}

const struct drm_display_mode *
intel_panel_preferred_fixed_mode(struct intel_connector *connector)
{
	return list_first_entry_or_null(&connector->panel.fixed_modes,
					struct drm_display_mode, head);
}

static bool is_in_vrr_range(struct intel_connector *connector, int vrefresh)
{
	const struct drm_display_info *info = &connector->base.display_info;

	return intel_vrr_is_capable(connector) &&
		vrefresh >= info->monitor_range.min_vfreq &&
		vrefresh <= info->monitor_range.max_vfreq;
}

static bool is_best_fixed_mode(struct intel_connector *connector,
			       int vrefresh, int fixed_mode_vrefresh,
			       const struct drm_display_mode *best_mode)
{
	 
	if (!best_mode)
		return true;

	 
	if (is_in_vrr_range(connector, vrefresh) &&
	    is_in_vrr_range(connector, fixed_mode_vrefresh) &&
	    fixed_mode_vrefresh < vrefresh)
		return false;

	 
	return abs(fixed_mode_vrefresh - vrefresh) <
		abs(drm_mode_vrefresh(best_mode) - vrefresh);
}

const struct drm_display_mode *
intel_panel_fixed_mode(struct intel_connector *connector,
		       const struct drm_display_mode *mode)
{
	const struct drm_display_mode *fixed_mode, *best_mode = NULL;
	int vrefresh = drm_mode_vrefresh(mode);

	list_for_each_entry(fixed_mode, &connector->panel.fixed_modes, head) {
		int fixed_mode_vrefresh = drm_mode_vrefresh(fixed_mode);

		if (is_best_fixed_mode(connector, vrefresh,
				       fixed_mode_vrefresh, best_mode))
			best_mode = fixed_mode;
	}

	return best_mode;
}

static bool is_alt_drrs_mode(const struct drm_display_mode *mode,
			     const struct drm_display_mode *preferred_mode)
{
	return drm_mode_match(mode, preferred_mode,
			      DRM_MODE_MATCH_TIMINGS |
			      DRM_MODE_MATCH_FLAGS |
			      DRM_MODE_MATCH_3D_FLAGS) &&
		mode->clock != preferred_mode->clock;
}

static bool is_alt_fixed_mode(const struct drm_display_mode *mode,
			      const struct drm_display_mode *preferred_mode)
{
	u32 sync_flags = DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_NHSYNC |
		DRM_MODE_FLAG_PVSYNC | DRM_MODE_FLAG_NVSYNC;

	return (mode->flags & ~sync_flags) == (preferred_mode->flags & ~sync_flags) &&
		mode->hdisplay == preferred_mode->hdisplay &&
		mode->vdisplay == preferred_mode->vdisplay;
}

const struct drm_display_mode *
intel_panel_downclock_mode(struct intel_connector *connector,
			   const struct drm_display_mode *adjusted_mode)
{
	const struct drm_display_mode *fixed_mode, *best_mode = NULL;
	int min_vrefresh = connector->panel.vbt.seamless_drrs_min_refresh_rate;
	int max_vrefresh = drm_mode_vrefresh(adjusted_mode);

	 
	list_for_each_entry(fixed_mode, &connector->panel.fixed_modes, head) {
		int vrefresh = drm_mode_vrefresh(fixed_mode);

		if (is_alt_drrs_mode(fixed_mode, adjusted_mode) &&
		    vrefresh >= min_vrefresh && vrefresh < max_vrefresh) {
			max_vrefresh = vrefresh;
			best_mode = fixed_mode;
		}
	}

	return best_mode;
}

const struct drm_display_mode *
intel_panel_highest_mode(struct intel_connector *connector,
			 const struct drm_display_mode *adjusted_mode)
{
	const struct drm_display_mode *fixed_mode, *best_mode = adjusted_mode;

	 
	list_for_each_entry(fixed_mode, &connector->panel.fixed_modes, head) {
		if (fixed_mode->clock > best_mode->clock)
			best_mode = fixed_mode;
	}

	return best_mode;
}

int intel_panel_get_modes(struct intel_connector *connector)
{
	const struct drm_display_mode *fixed_mode;
	int num_modes = 0;

	list_for_each_entry(fixed_mode, &connector->panel.fixed_modes, head) {
		struct drm_display_mode *mode;

		mode = drm_mode_duplicate(connector->base.dev, fixed_mode);
		if (mode) {
			drm_mode_probed_add(&connector->base, mode);
			num_modes++;
		}
	}

	return num_modes;
}

static bool has_drrs_modes(struct intel_connector *connector)
{
	const struct drm_display_mode *mode1;

	list_for_each_entry(mode1, &connector->panel.fixed_modes, head) {
		const struct drm_display_mode *mode2 = mode1;

		list_for_each_entry_continue(mode2, &connector->panel.fixed_modes, head) {
			if (is_alt_drrs_mode(mode1, mode2))
				return true;
		}
	}

	return false;
}

enum drrs_type intel_panel_drrs_type(struct intel_connector *connector)
{
	return connector->panel.vbt.drrs_type;
}

int intel_panel_compute_config(struct intel_connector *connector,
			       struct drm_display_mode *adjusted_mode)
{
	const struct drm_display_mode *fixed_mode =
		intel_panel_fixed_mode(connector, adjusted_mode);
	int vrefresh, fixed_mode_vrefresh;
	bool is_vrr;

	if (!fixed_mode)
		return 0;

	vrefresh = drm_mode_vrefresh(adjusted_mode);
	fixed_mode_vrefresh = drm_mode_vrefresh(fixed_mode);

	 
	is_vrr = is_in_vrr_range(connector, vrefresh) &&
		is_in_vrr_range(connector, fixed_mode_vrefresh);

	if (!is_vrr) {
		 
		if (abs(vrefresh - fixed_mode_vrefresh) > 1) {
			drm_dbg_kms(connector->base.dev,
				    "[CONNECTOR:%d:%s] Requested mode vrefresh (%d Hz) does not match fixed mode vrefresh (%d Hz)\n",
				    connector->base.base.id, connector->base.name,
				    vrefresh, fixed_mode_vrefresh);

			return -EINVAL;
		}
	}

	drm_mode_copy(adjusted_mode, fixed_mode);

	if (is_vrr && fixed_mode_vrefresh != vrefresh)
		adjusted_mode->vtotal =
			DIV_ROUND_CLOSEST(adjusted_mode->clock * 1000,
					  adjusted_mode->htotal * vrefresh);

	drm_mode_set_crtcinfo(adjusted_mode, 0);

	return 0;
}

static void intel_panel_add_edid_alt_fixed_modes(struct intel_connector *connector)
{
	struct drm_i915_private *dev_priv = to_i915(connector->base.dev);
	const struct drm_display_mode *preferred_mode =
		intel_panel_preferred_fixed_mode(connector);
	struct drm_display_mode *mode, *next;

	list_for_each_entry_safe(mode, next, &connector->base.probed_modes, head) {
		if (!is_alt_fixed_mode(mode, preferred_mode))
			continue;

		drm_dbg_kms(&dev_priv->drm,
			    "[CONNECTOR:%d:%s] using alternate EDID fixed mode: " DRM_MODE_FMT "\n",
			    connector->base.base.id, connector->base.name,
			    DRM_MODE_ARG(mode));

		list_move_tail(&mode->head, &connector->panel.fixed_modes);
	}
}

static void intel_panel_add_edid_preferred_mode(struct intel_connector *connector)
{
	struct drm_i915_private *dev_priv = to_i915(connector->base.dev);
	struct drm_display_mode *scan, *fixed_mode = NULL;

	if (list_empty(&connector->base.probed_modes))
		return;

	 
	list_for_each_entry(scan, &connector->base.probed_modes, head) {
		if (scan->type & DRM_MODE_TYPE_PREFERRED) {
			fixed_mode = scan;
			break;
		}
	}

	if (!fixed_mode)
		fixed_mode = list_first_entry(&connector->base.probed_modes,
					      typeof(*fixed_mode), head);

	drm_dbg_kms(&dev_priv->drm,
		    "[CONNECTOR:%d:%s] using %s EDID fixed mode: " DRM_MODE_FMT "\n",
		    connector->base.base.id, connector->base.name,
		    fixed_mode->type & DRM_MODE_TYPE_PREFERRED ? "preferred" : "first",
		    DRM_MODE_ARG(fixed_mode));

	fixed_mode->type |= DRM_MODE_TYPE_PREFERRED;

	list_move_tail(&fixed_mode->head, &connector->panel.fixed_modes);
}

static void intel_panel_destroy_probed_modes(struct intel_connector *connector)
{
	struct drm_i915_private *i915 = to_i915(connector->base.dev);
	struct drm_display_mode *mode, *next;

	list_for_each_entry_safe(mode, next, &connector->base.probed_modes, head) {
		drm_dbg_kms(&i915->drm,
			    "[CONNECTOR:%d:%s] not using EDID mode: " DRM_MODE_FMT "\n",
			    connector->base.base.id, connector->base.name,
			    DRM_MODE_ARG(mode));
		list_del(&mode->head);
		drm_mode_destroy(&i915->drm, mode);
	}
}

void intel_panel_add_edid_fixed_modes(struct intel_connector *connector,
				      bool use_alt_fixed_modes)
{
	intel_panel_add_edid_preferred_mode(connector);
	if (intel_panel_preferred_fixed_mode(connector) && use_alt_fixed_modes)
		intel_panel_add_edid_alt_fixed_modes(connector);
	intel_panel_destroy_probed_modes(connector);
}

static void intel_panel_add_fixed_mode(struct intel_connector *connector,
				       struct drm_display_mode *fixed_mode,
				       const char *type)
{
	struct drm_i915_private *i915 = to_i915(connector->base.dev);
	struct drm_display_info *info = &connector->base.display_info;

	if (!fixed_mode)
		return;

	fixed_mode->type |= DRM_MODE_TYPE_PREFERRED | DRM_MODE_TYPE_DRIVER;

	info->width_mm = fixed_mode->width_mm;
	info->height_mm = fixed_mode->height_mm;

	drm_dbg_kms(&i915->drm, "[CONNECTOR:%d:%s] using %s fixed mode: " DRM_MODE_FMT "\n",
		    connector->base.base.id, connector->base.name, type,
		    DRM_MODE_ARG(fixed_mode));

	list_add_tail(&fixed_mode->head, &connector->panel.fixed_modes);
}

void intel_panel_add_vbt_lfp_fixed_mode(struct intel_connector *connector)
{
	struct drm_i915_private *i915 = to_i915(connector->base.dev);
	const struct drm_display_mode *mode;

	mode = connector->panel.vbt.lfp_lvds_vbt_mode;
	if (!mode)
		return;

	intel_panel_add_fixed_mode(connector,
				   drm_mode_duplicate(&i915->drm, mode),
				   "VBT LFP");
}

void intel_panel_add_vbt_sdvo_fixed_mode(struct intel_connector *connector)
{
	struct drm_i915_private *i915 = to_i915(connector->base.dev);
	const struct drm_display_mode *mode;

	mode = connector->panel.vbt.sdvo_lvds_vbt_mode;
	if (!mode)
		return;

	intel_panel_add_fixed_mode(connector,
				   drm_mode_duplicate(&i915->drm, mode),
				   "VBT SDVO");
}

void intel_panel_add_encoder_fixed_mode(struct intel_connector *connector,
					struct intel_encoder *encoder)
{
	intel_panel_add_fixed_mode(connector,
				   intel_encoder_current_mode(encoder),
				   "current (BIOS)");
}

 
static int pch_panel_fitting(struct intel_crtc_state *crtc_state,
			     const struct drm_connector_state *conn_state)
{
	const struct drm_display_mode *adjusted_mode =
		&crtc_state->hw.adjusted_mode;
	int pipe_src_w = drm_rect_width(&crtc_state->pipe_src);
	int pipe_src_h = drm_rect_height(&crtc_state->pipe_src);
	int x, y, width, height;

	 
	if (adjusted_mode->crtc_hdisplay == pipe_src_w &&
	    adjusted_mode->crtc_vdisplay == pipe_src_h &&
	    crtc_state->output_format != INTEL_OUTPUT_FORMAT_YCBCR420)
		return 0;

	switch (conn_state->scaling_mode) {
	case DRM_MODE_SCALE_CENTER:
		width = pipe_src_w;
		height = pipe_src_h;
		x = (adjusted_mode->crtc_hdisplay - width + 1)/2;
		y = (adjusted_mode->crtc_vdisplay - height + 1)/2;
		break;

	case DRM_MODE_SCALE_ASPECT:
		 
		{
			u32 scaled_width = adjusted_mode->crtc_hdisplay * pipe_src_h;
			u32 scaled_height = pipe_src_w * adjusted_mode->crtc_vdisplay;
			if (scaled_width > scaled_height) {  
				width = scaled_height / pipe_src_h;
				if (width & 1)
					width++;
				x = (adjusted_mode->crtc_hdisplay - width + 1) / 2;
				y = 0;
				height = adjusted_mode->crtc_vdisplay;
			} else if (scaled_width < scaled_height) {  
				height = scaled_width / pipe_src_w;
				if (height & 1)
				    height++;
				y = (adjusted_mode->crtc_vdisplay - height + 1) / 2;
				x = 0;
				width = adjusted_mode->crtc_hdisplay;
			} else {
				x = y = 0;
				width = adjusted_mode->crtc_hdisplay;
				height = adjusted_mode->crtc_vdisplay;
			}
		}
		break;

	case DRM_MODE_SCALE_NONE:
		WARN_ON(adjusted_mode->crtc_hdisplay != pipe_src_w);
		WARN_ON(adjusted_mode->crtc_vdisplay != pipe_src_h);
		fallthrough;
	case DRM_MODE_SCALE_FULLSCREEN:
		x = y = 0;
		width = adjusted_mode->crtc_hdisplay;
		height = adjusted_mode->crtc_vdisplay;
		break;

	default:
		MISSING_CASE(conn_state->scaling_mode);
		return -EINVAL;
	}

	drm_rect_init(&crtc_state->pch_pfit.dst,
		      x, y, width, height);
	crtc_state->pch_pfit.enabled = true;

	return 0;
}

static void
centre_horizontally(struct drm_display_mode *adjusted_mode,
		    int width)
{
	u32 border, sync_pos, blank_width, sync_width;

	 
	sync_width = adjusted_mode->crtc_hsync_end - adjusted_mode->crtc_hsync_start;
	blank_width = adjusted_mode->crtc_hblank_end - adjusted_mode->crtc_hblank_start;
	sync_pos = (blank_width - sync_width + 1) / 2;

	border = (adjusted_mode->crtc_hdisplay - width + 1) / 2;
	border += border & 1;  

	adjusted_mode->crtc_hdisplay = width;
	adjusted_mode->crtc_hblank_start = width + border;
	adjusted_mode->crtc_hblank_end = adjusted_mode->crtc_hblank_start + blank_width;

	adjusted_mode->crtc_hsync_start = adjusted_mode->crtc_hblank_start + sync_pos;
	adjusted_mode->crtc_hsync_end = adjusted_mode->crtc_hsync_start + sync_width;
}

static void
centre_vertically(struct drm_display_mode *adjusted_mode,
		  int height)
{
	u32 border, sync_pos, blank_width, sync_width;

	 
	sync_width = adjusted_mode->crtc_vsync_end - adjusted_mode->crtc_vsync_start;
	blank_width = adjusted_mode->crtc_vblank_end - adjusted_mode->crtc_vblank_start;
	sync_pos = (blank_width - sync_width + 1) / 2;

	border = (adjusted_mode->crtc_vdisplay - height + 1) / 2;

	adjusted_mode->crtc_vdisplay = height;
	adjusted_mode->crtc_vblank_start = height + border;
	adjusted_mode->crtc_vblank_end = adjusted_mode->crtc_vblank_start + blank_width;

	adjusted_mode->crtc_vsync_start = adjusted_mode->crtc_vblank_start + sync_pos;
	adjusted_mode->crtc_vsync_end = adjusted_mode->crtc_vsync_start + sync_width;
}

static u32 panel_fitter_scaling(u32 source, u32 target)
{
	 
#define ACCURACY 12
#define FACTOR (1 << ACCURACY)
	u32 ratio = source * FACTOR / target;
	return (FACTOR * ratio + FACTOR/2) / FACTOR;
}

static void i965_scale_aspect(struct intel_crtc_state *crtc_state,
			      u32 *pfit_control)
{
	const struct drm_display_mode *adjusted_mode =
		&crtc_state->hw.adjusted_mode;
	int pipe_src_w = drm_rect_width(&crtc_state->pipe_src);
	int pipe_src_h = drm_rect_height(&crtc_state->pipe_src);
	u32 scaled_width = adjusted_mode->crtc_hdisplay * pipe_src_h;
	u32 scaled_height = pipe_src_w * adjusted_mode->crtc_vdisplay;

	 
	if (scaled_width > scaled_height)
		*pfit_control |= PFIT_ENABLE |
			PFIT_SCALING_PILLAR;
	else if (scaled_width < scaled_height)
		*pfit_control |= PFIT_ENABLE |
			PFIT_SCALING_LETTER;
	else if (adjusted_mode->crtc_hdisplay != pipe_src_w)
		*pfit_control |= PFIT_ENABLE | PFIT_SCALING_AUTO;
}

static void i9xx_scale_aspect(struct intel_crtc_state *crtc_state,
			      u32 *pfit_control, u32 *pfit_pgm_ratios,
			      u32 *border)
{
	struct drm_display_mode *adjusted_mode = &crtc_state->hw.adjusted_mode;
	int pipe_src_w = drm_rect_width(&crtc_state->pipe_src);
	int pipe_src_h = drm_rect_height(&crtc_state->pipe_src);
	u32 scaled_width = adjusted_mode->crtc_hdisplay * pipe_src_h;
	u32 scaled_height = pipe_src_w * adjusted_mode->crtc_vdisplay;
	u32 bits;

	 
	if (scaled_width > scaled_height) {  
		centre_horizontally(adjusted_mode,
				    scaled_height / pipe_src_h);

		*border = LVDS_BORDER_ENABLE;
		if (pipe_src_h != adjusted_mode->crtc_vdisplay) {
			bits = panel_fitter_scaling(pipe_src_h,
						    adjusted_mode->crtc_vdisplay);

			*pfit_pgm_ratios |= (PFIT_HORIZ_SCALE(bits) |
					     PFIT_VERT_SCALE(bits));
			*pfit_control |= (PFIT_ENABLE |
					  PFIT_VERT_INTERP_BILINEAR |
					  PFIT_HORIZ_INTERP_BILINEAR);
		}
	} else if (scaled_width < scaled_height) {  
		centre_vertically(adjusted_mode,
				  scaled_width / pipe_src_w);

		*border = LVDS_BORDER_ENABLE;
		if (pipe_src_w != adjusted_mode->crtc_hdisplay) {
			bits = panel_fitter_scaling(pipe_src_w,
						    adjusted_mode->crtc_hdisplay);

			*pfit_pgm_ratios |= (PFIT_HORIZ_SCALE(bits) |
					     PFIT_VERT_SCALE(bits));
			*pfit_control |= (PFIT_ENABLE |
					  PFIT_VERT_INTERP_BILINEAR |
					  PFIT_HORIZ_INTERP_BILINEAR);
		}
	} else {
		 
		*pfit_control |= (PFIT_ENABLE |
				  PFIT_VERT_AUTO_SCALE |
				  PFIT_HORIZ_AUTO_SCALE |
				  PFIT_VERT_INTERP_BILINEAR |
				  PFIT_HORIZ_INTERP_BILINEAR);
	}
}

static int gmch_panel_fitting(struct intel_crtc_state *crtc_state,
			      const struct drm_connector_state *conn_state)
{
	struct intel_crtc *crtc = to_intel_crtc(crtc_state->uapi.crtc);
	struct drm_i915_private *dev_priv = to_i915(crtc->base.dev);
	u32 pfit_control = 0, pfit_pgm_ratios = 0, border = 0;
	struct drm_display_mode *adjusted_mode = &crtc_state->hw.adjusted_mode;
	int pipe_src_w = drm_rect_width(&crtc_state->pipe_src);
	int pipe_src_h = drm_rect_height(&crtc_state->pipe_src);

	 
	if (adjusted_mode->crtc_hdisplay == pipe_src_w &&
	    adjusted_mode->crtc_vdisplay == pipe_src_h)
		goto out;

	switch (conn_state->scaling_mode) {
	case DRM_MODE_SCALE_CENTER:
		 
		centre_horizontally(adjusted_mode, pipe_src_w);
		centre_vertically(adjusted_mode, pipe_src_h);
		border = LVDS_BORDER_ENABLE;
		break;
	case DRM_MODE_SCALE_ASPECT:
		 
		if (DISPLAY_VER(dev_priv) >= 4)
			i965_scale_aspect(crtc_state, &pfit_control);
		else
			i9xx_scale_aspect(crtc_state, &pfit_control,
					  &pfit_pgm_ratios, &border);
		break;
	case DRM_MODE_SCALE_FULLSCREEN:
		 
		if (pipe_src_h != adjusted_mode->crtc_vdisplay ||
		    pipe_src_w != adjusted_mode->crtc_hdisplay) {
			pfit_control |= PFIT_ENABLE;
			if (DISPLAY_VER(dev_priv) >= 4)
				pfit_control |= PFIT_SCALING_AUTO;
			else
				pfit_control |= (PFIT_VERT_AUTO_SCALE |
						 PFIT_VERT_INTERP_BILINEAR |
						 PFIT_HORIZ_AUTO_SCALE |
						 PFIT_HORIZ_INTERP_BILINEAR);
		}
		break;
	default:
		MISSING_CASE(conn_state->scaling_mode);
		return -EINVAL;
	}

	 
	 
	if (DISPLAY_VER(dev_priv) >= 4)
		pfit_control |= PFIT_PIPE(crtc->pipe) | PFIT_FILTER_FUZZY;

out:
	if ((pfit_control & PFIT_ENABLE) == 0) {
		pfit_control = 0;
		pfit_pgm_ratios = 0;
	}

	 
	if (DISPLAY_VER(dev_priv) < 4 && crtc_state->pipe_bpp == 18)
		pfit_control |= PFIT_PANEL_8TO6_DITHER_ENABLE;

	crtc_state->gmch_pfit.control = pfit_control;
	crtc_state->gmch_pfit.pgm_ratios = pfit_pgm_ratios;
	crtc_state->gmch_pfit.lvds_border_bits = border;

	return 0;
}

int intel_panel_fitting(struct intel_crtc_state *crtc_state,
			const struct drm_connector_state *conn_state)
{
	struct intel_crtc *crtc = to_intel_crtc(crtc_state->uapi.crtc);
	struct drm_i915_private *i915 = to_i915(crtc->base.dev);

	if (HAS_GMCH(i915))
		return gmch_panel_fitting(crtc_state, conn_state);
	else
		return pch_panel_fitting(crtc_state, conn_state);
}

enum drm_connector_status
intel_panel_detect(struct drm_connector *connector, bool force)
{
	struct drm_i915_private *i915 = to_i915(connector->dev);

	if (!INTEL_DISPLAY_ENABLED(i915))
		return connector_status_disconnected;

	return connector_status_connected;
}

enum drm_mode_status
intel_panel_mode_valid(struct intel_connector *connector,
		       const struct drm_display_mode *mode)
{
	const struct drm_display_mode *fixed_mode =
		intel_panel_fixed_mode(connector, mode);

	if (!fixed_mode)
		return MODE_OK;

	if (mode->hdisplay != fixed_mode->hdisplay)
		return MODE_PANEL;

	if (mode->vdisplay != fixed_mode->vdisplay)
		return MODE_PANEL;

	if (drm_mode_vrefresh(mode) != drm_mode_vrefresh(fixed_mode))
		return MODE_PANEL;

	return MODE_OK;
}

void intel_panel_init_alloc(struct intel_connector *connector)
{
	struct intel_panel *panel = &connector->panel;

	connector->panel.vbt.panel_type = -1;
	connector->panel.vbt.backlight.controller = -1;
	INIT_LIST_HEAD(&panel->fixed_modes);
}

int intel_panel_init(struct intel_connector *connector,
		     const struct drm_edid *fixed_edid)
{
	struct intel_panel *panel = &connector->panel;

	panel->fixed_edid = fixed_edid;

	intel_backlight_init_funcs(panel);

	if (!has_drrs_modes(connector))
		connector->panel.vbt.drrs_type = DRRS_TYPE_NONE;

	drm_dbg_kms(connector->base.dev,
		    "[CONNECTOR:%d:%s] DRRS type: %s\n",
		    connector->base.base.id, connector->base.name,
		    intel_drrs_type_str(intel_panel_drrs_type(connector)));

	return 0;
}

void intel_panel_fini(struct intel_connector *connector)
{
	struct intel_panel *panel = &connector->panel;
	struct drm_display_mode *fixed_mode, *next;

	if (!IS_ERR_OR_NULL(panel->fixed_edid))
		drm_edid_free(panel->fixed_edid);

	intel_backlight_destroy(panel);

	intel_bios_fini_panel(panel);

	list_for_each_entry_safe(fixed_mode, next, &panel->fixed_modes, head) {
		list_del(&fixed_mode->head);
		drm_mode_destroy(connector->base.dev, fixed_mode);
	}
}
