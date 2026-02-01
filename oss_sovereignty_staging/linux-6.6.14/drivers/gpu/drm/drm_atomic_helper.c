 

#include <linux/dma-fence.h>
#include <linux/ktime.h>

#include <drm/drm_atomic.h>
#include <drm/drm_atomic_helper.h>
#include <drm/drm_atomic_uapi.h>
#include <drm/drm_blend.h>
#include <drm/drm_bridge.h>
#include <drm/drm_damage_helper.h>
#include <drm/drm_device.h>
#include <drm/drm_drv.h>
#include <drm/drm_framebuffer.h>
#include <drm/drm_gem_atomic_helper.h>
#include <drm/drm_print.h>
#include <drm/drm_self_refresh_helper.h>
#include <drm/drm_vblank.h>
#include <drm/drm_writeback.h>

#include "drm_crtc_helper_internal.h"
#include "drm_crtc_internal.h"

 
static void
drm_atomic_helper_plane_changed(struct drm_atomic_state *state,
				struct drm_plane_state *old_plane_state,
				struct drm_plane_state *plane_state,
				struct drm_plane *plane)
{
	struct drm_crtc_state *crtc_state;

	if (old_plane_state->crtc) {
		crtc_state = drm_atomic_get_new_crtc_state(state,
							   old_plane_state->crtc);

		if (WARN_ON(!crtc_state))
			return;

		crtc_state->planes_changed = true;
	}

	if (plane_state->crtc) {
		crtc_state = drm_atomic_get_new_crtc_state(state, plane_state->crtc);

		if (WARN_ON(!crtc_state))
			return;

		crtc_state->planes_changed = true;
	}
}

static int handle_conflicting_encoders(struct drm_atomic_state *state,
				       bool disable_conflicting_encoders)
{
	struct drm_connector_state *new_conn_state;
	struct drm_connector *connector;
	struct drm_connector_list_iter conn_iter;
	struct drm_encoder *encoder;
	unsigned int encoder_mask = 0;
	int i, ret = 0;

	 
	for_each_new_connector_in_state(state, connector, new_conn_state, i) {
		const struct drm_connector_helper_funcs *funcs = connector->helper_private;
		struct drm_encoder *new_encoder;

		if (!new_conn_state->crtc)
			continue;

		if (funcs->atomic_best_encoder)
			new_encoder = funcs->atomic_best_encoder(connector,
								 state);
		else if (funcs->best_encoder)
			new_encoder = funcs->best_encoder(connector);
		else
			new_encoder = drm_connector_get_single_encoder(connector);

		if (new_encoder) {
			if (encoder_mask & drm_encoder_mask(new_encoder)) {
				drm_dbg_atomic(connector->dev,
					       "[ENCODER:%d:%s] on [CONNECTOR:%d:%s] already assigned\n",
					       new_encoder->base.id, new_encoder->name,
					       connector->base.id, connector->name);

				return -EINVAL;
			}

			encoder_mask |= drm_encoder_mask(new_encoder);
		}
	}

	if (!encoder_mask)
		return 0;

	 
	drm_connector_list_iter_begin(state->dev, &conn_iter);
	drm_for_each_connector_iter(connector, &conn_iter) {
		struct drm_crtc_state *crtc_state;

		if (drm_atomic_get_new_connector_state(state, connector))
			continue;

		encoder = connector->state->best_encoder;
		if (!encoder || !(encoder_mask & drm_encoder_mask(encoder)))
			continue;

		if (!disable_conflicting_encoders) {
			drm_dbg_atomic(connector->dev,
				       "[ENCODER:%d:%s] in use on [CRTC:%d:%s] by [CONNECTOR:%d:%s]\n",
				       encoder->base.id, encoder->name,
				       connector->state->crtc->base.id,
				       connector->state->crtc->name,
				       connector->base.id, connector->name);
			ret = -EINVAL;
			goto out;
		}

		new_conn_state = drm_atomic_get_connector_state(state, connector);
		if (IS_ERR(new_conn_state)) {
			ret = PTR_ERR(new_conn_state);
			goto out;
		}

		drm_dbg_atomic(connector->dev,
			       "[ENCODER:%d:%s] in use on [CRTC:%d:%s], disabling [CONNECTOR:%d:%s]\n",
			       encoder->base.id, encoder->name,
			       new_conn_state->crtc->base.id, new_conn_state->crtc->name,
			       connector->base.id, connector->name);

		crtc_state = drm_atomic_get_new_crtc_state(state, new_conn_state->crtc);

		ret = drm_atomic_set_crtc_for_connector(new_conn_state, NULL);
		if (ret)
			goto out;

		if (!crtc_state->connector_mask) {
			ret = drm_atomic_set_mode_prop_for_crtc(crtc_state,
								NULL);
			if (ret < 0)
				goto out;

			crtc_state->active = false;
		}
	}
out:
	drm_connector_list_iter_end(&conn_iter);

	return ret;
}

static void
set_best_encoder(struct drm_atomic_state *state,
		 struct drm_connector_state *conn_state,
		 struct drm_encoder *encoder)
{
	struct drm_crtc_state *crtc_state;
	struct drm_crtc *crtc;

	if (conn_state->best_encoder) {
		 
		crtc = conn_state->connector->state->crtc;

		 
		WARN_ON(!crtc && encoder != conn_state->best_encoder);
		if (crtc) {
			crtc_state = drm_atomic_get_new_crtc_state(state, crtc);

			crtc_state->encoder_mask &=
				~drm_encoder_mask(conn_state->best_encoder);
		}
	}

	if (encoder) {
		crtc = conn_state->crtc;
		WARN_ON(!crtc);
		if (crtc) {
			crtc_state = drm_atomic_get_new_crtc_state(state, crtc);

			crtc_state->encoder_mask |=
				drm_encoder_mask(encoder);
		}
	}

	conn_state->best_encoder = encoder;
}

static void
steal_encoder(struct drm_atomic_state *state,
	      struct drm_encoder *encoder)
{
	struct drm_crtc_state *crtc_state;
	struct drm_connector *connector;
	struct drm_connector_state *old_connector_state, *new_connector_state;
	int i;

	for_each_oldnew_connector_in_state(state, connector, old_connector_state, new_connector_state, i) {
		struct drm_crtc *encoder_crtc;

		if (new_connector_state->best_encoder != encoder)
			continue;

		encoder_crtc = old_connector_state->crtc;

		drm_dbg_atomic(encoder->dev,
			       "[ENCODER:%d:%s] in use on [CRTC:%d:%s], stealing it\n",
			       encoder->base.id, encoder->name,
			       encoder_crtc->base.id, encoder_crtc->name);

		set_best_encoder(state, new_connector_state, NULL);

		crtc_state = drm_atomic_get_new_crtc_state(state, encoder_crtc);
		crtc_state->connectors_changed = true;

		return;
	}
}

static int
update_connector_routing(struct drm_atomic_state *state,
			 struct drm_connector *connector,
			 struct drm_connector_state *old_connector_state,
			 struct drm_connector_state *new_connector_state,
			 bool added_by_user)
{
	const struct drm_connector_helper_funcs *funcs;
	struct drm_encoder *new_encoder;
	struct drm_crtc_state *crtc_state;

	drm_dbg_atomic(connector->dev, "Updating routing for [CONNECTOR:%d:%s]\n",
		       connector->base.id, connector->name);

	if (old_connector_state->crtc != new_connector_state->crtc) {
		if (old_connector_state->crtc) {
			crtc_state = drm_atomic_get_new_crtc_state(state, old_connector_state->crtc);
			crtc_state->connectors_changed = true;
		}

		if (new_connector_state->crtc) {
			crtc_state = drm_atomic_get_new_crtc_state(state, new_connector_state->crtc);
			crtc_state->connectors_changed = true;
		}
	}

	if (!new_connector_state->crtc) {
		drm_dbg_atomic(connector->dev, "Disabling [CONNECTOR:%d:%s]\n",
				connector->base.id, connector->name);

		set_best_encoder(state, new_connector_state, NULL);

		return 0;
	}

	crtc_state = drm_atomic_get_new_crtc_state(state,
						   new_connector_state->crtc);
	 
	if (!state->duplicated && drm_connector_is_unregistered(connector) &&
	    added_by_user && crtc_state->active) {
		drm_dbg_atomic(connector->dev,
			       "[CONNECTOR:%d:%s] is not registered\n",
			       connector->base.id, connector->name);
		return -EINVAL;
	}

	funcs = connector->helper_private;

	if (funcs->atomic_best_encoder)
		new_encoder = funcs->atomic_best_encoder(connector, state);
	else if (funcs->best_encoder)
		new_encoder = funcs->best_encoder(connector);
	else
		new_encoder = drm_connector_get_single_encoder(connector);

	if (!new_encoder) {
		drm_dbg_atomic(connector->dev,
			       "No suitable encoder found for [CONNECTOR:%d:%s]\n",
			       connector->base.id, connector->name);
		return -EINVAL;
	}

	if (!drm_encoder_crtc_ok(new_encoder, new_connector_state->crtc)) {
		drm_dbg_atomic(connector->dev,
			       "[ENCODER:%d:%s] incompatible with [CRTC:%d:%s]\n",
			       new_encoder->base.id,
			       new_encoder->name,
			       new_connector_state->crtc->base.id,
			       new_connector_state->crtc->name);
		return -EINVAL;
	}

	if (new_encoder == new_connector_state->best_encoder) {
		set_best_encoder(state, new_connector_state, new_encoder);

		drm_dbg_atomic(connector->dev,
			       "[CONNECTOR:%d:%s] keeps [ENCODER:%d:%s], now on [CRTC:%d:%s]\n",
			       connector->base.id,
			       connector->name,
			       new_encoder->base.id,
			       new_encoder->name,
			       new_connector_state->crtc->base.id,
			       new_connector_state->crtc->name);

		return 0;
	}

	steal_encoder(state, new_encoder);

	set_best_encoder(state, new_connector_state, new_encoder);

	crtc_state->connectors_changed = true;

	drm_dbg_atomic(connector->dev,
		       "[CONNECTOR:%d:%s] using [ENCODER:%d:%s] on [CRTC:%d:%s]\n",
		       connector->base.id,
		       connector->name,
		       new_encoder->base.id,
		       new_encoder->name,
		       new_connector_state->crtc->base.id,
		       new_connector_state->crtc->name);

	return 0;
}

static int
mode_fixup(struct drm_atomic_state *state)
{
	struct drm_crtc *crtc;
	struct drm_crtc_state *new_crtc_state;
	struct drm_connector *connector;
	struct drm_connector_state *new_conn_state;
	int i;
	int ret;

	for_each_new_crtc_in_state(state, crtc, new_crtc_state, i) {
		if (!new_crtc_state->mode_changed &&
		    !new_crtc_state->connectors_changed)
			continue;

		drm_mode_copy(&new_crtc_state->adjusted_mode, &new_crtc_state->mode);
	}

	for_each_new_connector_in_state(state, connector, new_conn_state, i) {
		const struct drm_encoder_helper_funcs *funcs;
		struct drm_encoder *encoder;
		struct drm_bridge *bridge;

		WARN_ON(!!new_conn_state->best_encoder != !!new_conn_state->crtc);

		if (!new_conn_state->crtc || !new_conn_state->best_encoder)
			continue;

		new_crtc_state =
			drm_atomic_get_new_crtc_state(state, new_conn_state->crtc);

		 
		encoder = new_conn_state->best_encoder;
		funcs = encoder->helper_private;

		bridge = drm_bridge_chain_get_first_bridge(encoder);
		ret = drm_atomic_bridge_chain_check(bridge,
						    new_crtc_state,
						    new_conn_state);
		if (ret) {
			drm_dbg_atomic(encoder->dev, "Bridge atomic check failed\n");
			return ret;
		}

		if (funcs && funcs->atomic_check) {
			ret = funcs->atomic_check(encoder, new_crtc_state,
						  new_conn_state);
			if (ret) {
				drm_dbg_atomic(encoder->dev,
					       "[ENCODER:%d:%s] check failed\n",
					       encoder->base.id, encoder->name);
				return ret;
			}
		} else if (funcs && funcs->mode_fixup) {
			ret = funcs->mode_fixup(encoder, &new_crtc_state->mode,
						&new_crtc_state->adjusted_mode);
			if (!ret) {
				drm_dbg_atomic(encoder->dev,
					       "[ENCODER:%d:%s] fixup failed\n",
					       encoder->base.id, encoder->name);
				return -EINVAL;
			}
		}
	}

	for_each_new_crtc_in_state(state, crtc, new_crtc_state, i) {
		const struct drm_crtc_helper_funcs *funcs;

		if (!new_crtc_state->enable)
			continue;

		if (!new_crtc_state->mode_changed &&
		    !new_crtc_state->connectors_changed)
			continue;

		funcs = crtc->helper_private;
		if (!funcs || !funcs->mode_fixup)
			continue;

		ret = funcs->mode_fixup(crtc, &new_crtc_state->mode,
					&new_crtc_state->adjusted_mode);
		if (!ret) {
			drm_dbg_atomic(crtc->dev, "[CRTC:%d:%s] fixup failed\n",
				       crtc->base.id, crtc->name);
			return -EINVAL;
		}
	}

	return 0;
}

static enum drm_mode_status mode_valid_path(struct drm_connector *connector,
					    struct drm_encoder *encoder,
					    struct drm_crtc *crtc,
					    const struct drm_display_mode *mode)
{
	struct drm_bridge *bridge;
	enum drm_mode_status ret;

	ret = drm_encoder_mode_valid(encoder, mode);
	if (ret != MODE_OK) {
		drm_dbg_atomic(encoder->dev,
			       "[ENCODER:%d:%s] mode_valid() failed\n",
			       encoder->base.id, encoder->name);
		return ret;
	}

	bridge = drm_bridge_chain_get_first_bridge(encoder);
	ret = drm_bridge_chain_mode_valid(bridge, &connector->display_info,
					  mode);
	if (ret != MODE_OK) {
		drm_dbg_atomic(encoder->dev, "[BRIDGE] mode_valid() failed\n");
		return ret;
	}

	ret = drm_crtc_mode_valid(crtc, mode);
	if (ret != MODE_OK) {
		drm_dbg_atomic(encoder->dev, "[CRTC:%d:%s] mode_valid() failed\n",
			       crtc->base.id, crtc->name);
		return ret;
	}

	return ret;
}

static int
mode_valid(struct drm_atomic_state *state)
{
	struct drm_connector_state *conn_state;
	struct drm_connector *connector;
	int i;

	for_each_new_connector_in_state(state, connector, conn_state, i) {
		struct drm_encoder *encoder = conn_state->best_encoder;
		struct drm_crtc *crtc = conn_state->crtc;
		struct drm_crtc_state *crtc_state;
		enum drm_mode_status mode_status;
		const struct drm_display_mode *mode;

		if (!crtc || !encoder)
			continue;

		crtc_state = drm_atomic_get_new_crtc_state(state, crtc);
		if (!crtc_state)
			continue;
		if (!crtc_state->mode_changed && !crtc_state->connectors_changed)
			continue;

		mode = &crtc_state->mode;

		mode_status = mode_valid_path(connector, encoder, crtc, mode);
		if (mode_status != MODE_OK)
			return -EINVAL;
	}

	return 0;
}

 
int
drm_atomic_helper_check_modeset(struct drm_device *dev,
				struct drm_atomic_state *state)
{
	struct drm_crtc *crtc;
	struct drm_crtc_state *old_crtc_state, *new_crtc_state;
	struct drm_connector *connector;
	struct drm_connector_state *old_connector_state, *new_connector_state;
	int i, ret;
	unsigned int connectors_mask = 0, user_connectors_mask = 0;

	for_each_oldnew_connector_in_state(state, connector, old_connector_state, new_connector_state, i)
		user_connectors_mask |= BIT(i);

	for_each_oldnew_crtc_in_state(state, crtc, old_crtc_state, new_crtc_state, i) {
		bool has_connectors =
			!!new_crtc_state->connector_mask;

		WARN_ON(!drm_modeset_is_locked(&crtc->mutex));

		if (!drm_mode_equal(&old_crtc_state->mode, &new_crtc_state->mode)) {
			drm_dbg_atomic(dev, "[CRTC:%d:%s] mode changed\n",
				       crtc->base.id, crtc->name);
			new_crtc_state->mode_changed = true;
		}

		if (old_crtc_state->enable != new_crtc_state->enable) {
			drm_dbg_atomic(dev, "[CRTC:%d:%s] enable changed\n",
				       crtc->base.id, crtc->name);

			 
			new_crtc_state->mode_changed = true;
			new_crtc_state->connectors_changed = true;
		}

		if (old_crtc_state->active != new_crtc_state->active) {
			drm_dbg_atomic(dev, "[CRTC:%d:%s] active changed\n",
				       crtc->base.id, crtc->name);
			new_crtc_state->active_changed = true;
		}

		if (new_crtc_state->enable != has_connectors) {
			drm_dbg_atomic(dev, "[CRTC:%d:%s] enabled/connectors mismatch\n",
				       crtc->base.id, crtc->name);

			return -EINVAL;
		}

		if (drm_dev_has_vblank(dev))
			new_crtc_state->no_vblank = false;
		else
			new_crtc_state->no_vblank = true;
	}

	ret = handle_conflicting_encoders(state, false);
	if (ret)
		return ret;

	for_each_oldnew_connector_in_state(state, connector, old_connector_state, new_connector_state, i) {
		const struct drm_connector_helper_funcs *funcs = connector->helper_private;

		WARN_ON(!drm_modeset_is_locked(&dev->mode_config.connection_mutex));

		 
		ret = update_connector_routing(state, connector,
					       old_connector_state,
					       new_connector_state,
					       BIT(i) & user_connectors_mask);
		if (ret)
			return ret;
		if (old_connector_state->crtc) {
			new_crtc_state = drm_atomic_get_new_crtc_state(state,
								       old_connector_state->crtc);
			if (old_connector_state->link_status !=
			    new_connector_state->link_status)
				new_crtc_state->connectors_changed = true;

			if (old_connector_state->max_requested_bpc !=
			    new_connector_state->max_requested_bpc)
				new_crtc_state->connectors_changed = true;
		}

		if (funcs->atomic_check)
			ret = funcs->atomic_check(connector, state);
		if (ret) {
			drm_dbg_atomic(dev,
				       "[CONNECTOR:%d:%s] driver check failed\n",
				       connector->base.id, connector->name);
			return ret;
		}

		connectors_mask |= BIT(i);
	}

	 
	for_each_oldnew_crtc_in_state(state, crtc, old_crtc_state, new_crtc_state, i) {
		if (!drm_atomic_crtc_needs_modeset(new_crtc_state))
			continue;

		drm_dbg_atomic(dev,
			       "[CRTC:%d:%s] needs all connectors, enable: %c, active: %c\n",
			       crtc->base.id, crtc->name,
			       new_crtc_state->enable ? 'y' : 'n',
			       new_crtc_state->active ? 'y' : 'n');

		ret = drm_atomic_add_affected_connectors(state, crtc);
		if (ret != 0)
			return ret;

		ret = drm_atomic_add_affected_planes(state, crtc);
		if (ret != 0)
			return ret;
	}

	 
	for_each_oldnew_connector_in_state(state, connector, old_connector_state, new_connector_state, i) {
		const struct drm_connector_helper_funcs *funcs = connector->helper_private;

		if (connectors_mask & BIT(i))
			continue;

		if (funcs->atomic_check)
			ret = funcs->atomic_check(connector, state);
		if (ret) {
			drm_dbg_atomic(dev,
				       "[CONNECTOR:%d:%s] driver check failed\n",
				       connector->base.id, connector->name);
			return ret;
		}
	}

	 
	for_each_oldnew_connector_in_state(state, connector,
					   old_connector_state,
					   new_connector_state, i) {
		struct drm_encoder *encoder;

		encoder = old_connector_state->best_encoder;
		ret = drm_atomic_add_encoder_bridges(state, encoder);
		if (ret)
			return ret;

		encoder = new_connector_state->best_encoder;
		ret = drm_atomic_add_encoder_bridges(state, encoder);
		if (ret)
			return ret;
	}

	ret = mode_valid(state);
	if (ret)
		return ret;

	return mode_fixup(state);
}
EXPORT_SYMBOL(drm_atomic_helper_check_modeset);

 
int
drm_atomic_helper_check_wb_encoder_state(struct drm_encoder *encoder,
					 struct drm_connector_state *conn_state)
{
	struct drm_writeback_job *wb_job = conn_state->writeback_job;
	struct drm_property_blob *pixel_format_blob;
	struct drm_framebuffer *fb;
	size_t i, nformats;
	u32 *formats;

	if (!wb_job || !wb_job->fb)
		return 0;

	pixel_format_blob = wb_job->connector->pixel_formats_blob_ptr;
	nformats = pixel_format_blob->length / sizeof(u32);
	formats = pixel_format_blob->data;
	fb = wb_job->fb;

	for (i = 0; i < nformats; i++)
		if (fb->format->format == formats[i])
			return 0;

	drm_dbg_kms(encoder->dev, "Invalid pixel format %p4cc\n", &fb->format->format);

	return -EINVAL;
}
EXPORT_SYMBOL(drm_atomic_helper_check_wb_encoder_state);

 
int drm_atomic_helper_check_plane_state(struct drm_plane_state *plane_state,
					const struct drm_crtc_state *crtc_state,
					int min_scale,
					int max_scale,
					bool can_position,
					bool can_update_disabled)
{
	struct drm_framebuffer *fb = plane_state->fb;
	struct drm_rect *src = &plane_state->src;
	struct drm_rect *dst = &plane_state->dst;
	unsigned int rotation = plane_state->rotation;
	struct drm_rect clip = {};
	int hscale, vscale;

	WARN_ON(plane_state->crtc && plane_state->crtc != crtc_state->crtc);

	*src = drm_plane_state_src(plane_state);
	*dst = drm_plane_state_dest(plane_state);

	if (!fb) {
		plane_state->visible = false;
		return 0;
	}

	 
	if (WARN_ON(!plane_state->crtc)) {
		plane_state->visible = false;
		return 0;
	}

	if (!crtc_state->enable && !can_update_disabled) {
		drm_dbg_kms(plane_state->plane->dev,
			    "Cannot update plane of a disabled CRTC.\n");
		return -EINVAL;
	}

	drm_rect_rotate(src, fb->width << 16, fb->height << 16, rotation);

	 
	hscale = drm_rect_calc_hscale(src, dst, min_scale, max_scale);
	vscale = drm_rect_calc_vscale(src, dst, min_scale, max_scale);
	if (hscale < 0 || vscale < 0) {
		drm_dbg_kms(plane_state->plane->dev,
			    "Invalid scaling of plane\n");
		drm_rect_debug_print("src: ", &plane_state->src, true);
		drm_rect_debug_print("dst: ", &plane_state->dst, false);
		return -ERANGE;
	}

	if (crtc_state->enable)
		drm_mode_get_hv_timing(&crtc_state->mode, &clip.x2, &clip.y2);

	plane_state->visible = drm_rect_clip_scaled(src, dst, &clip);

	drm_rect_rotate_inv(src, fb->width << 16, fb->height << 16, rotation);

	if (!plane_state->visible)
		 
		return 0;

	if (!can_position && !drm_rect_equals(dst, &clip)) {
		drm_dbg_kms(plane_state->plane->dev,
			    "Plane must cover entire CRTC\n");
		drm_rect_debug_print("dst: ", dst, false);
		drm_rect_debug_print("clip: ", &clip, false);
		return -EINVAL;
	}

	return 0;
}
EXPORT_SYMBOL(drm_atomic_helper_check_plane_state);

 
int drm_atomic_helper_check_crtc_primary_plane(struct drm_crtc_state *crtc_state)
{
	struct drm_crtc *crtc = crtc_state->crtc;
	struct drm_device *dev = crtc->dev;
	struct drm_plane *plane;

	 
	drm_for_each_plane_mask(plane, dev, crtc_state->plane_mask) {
		if (plane->type == DRM_PLANE_TYPE_PRIMARY)
			return 0;
	}

	drm_dbg_atomic(dev, "[CRTC:%d:%s] primary plane missing\n", crtc->base.id, crtc->name);

	return -EINVAL;
}
EXPORT_SYMBOL(drm_atomic_helper_check_crtc_primary_plane);

 
int
drm_atomic_helper_check_planes(struct drm_device *dev,
			       struct drm_atomic_state *state)
{
	struct drm_crtc *crtc;
	struct drm_crtc_state *new_crtc_state;
	struct drm_plane *plane;
	struct drm_plane_state *new_plane_state, *old_plane_state;
	int i, ret = 0;

	for_each_oldnew_plane_in_state(state, plane, old_plane_state, new_plane_state, i) {
		const struct drm_plane_helper_funcs *funcs;

		WARN_ON(!drm_modeset_is_locked(&plane->mutex));

		funcs = plane->helper_private;

		drm_atomic_helper_plane_changed(state, old_plane_state, new_plane_state, plane);

		drm_atomic_helper_check_plane_damage(state, new_plane_state);

		if (!funcs || !funcs->atomic_check)
			continue;

		ret = funcs->atomic_check(plane, state);
		if (ret) {
			drm_dbg_atomic(plane->dev,
				       "[PLANE:%d:%s] atomic driver check failed\n",
				       plane->base.id, plane->name);
			return ret;
		}
	}

	for_each_new_crtc_in_state(state, crtc, new_crtc_state, i) {
		const struct drm_crtc_helper_funcs *funcs;

		funcs = crtc->helper_private;

		if (!funcs || !funcs->atomic_check)
			continue;

		ret = funcs->atomic_check(crtc, state);
		if (ret) {
			drm_dbg_atomic(crtc->dev,
				       "[CRTC:%d:%s] atomic driver check failed\n",
				       crtc->base.id, crtc->name);
			return ret;
		}
	}

	return ret;
}
EXPORT_SYMBOL(drm_atomic_helper_check_planes);

 
int drm_atomic_helper_check(struct drm_device *dev,
			    struct drm_atomic_state *state)
{
	int ret;

	ret = drm_atomic_helper_check_modeset(dev, state);
	if (ret)
		return ret;

	if (dev->mode_config.normalize_zpos) {
		ret = drm_atomic_normalize_zpos(dev, state);
		if (ret)
			return ret;
	}

	ret = drm_atomic_helper_check_planes(dev, state);
	if (ret)
		return ret;

	if (state->legacy_cursor_update)
		state->async_update = !drm_atomic_helper_async_check(dev, state);

	drm_self_refresh_helper_alter_state(state);

	return ret;
}
EXPORT_SYMBOL(drm_atomic_helper_check);

static bool
crtc_needs_disable(struct drm_crtc_state *old_state,
		   struct drm_crtc_state *new_state)
{
	 
	if (!new_state)
		return drm_atomic_crtc_effectively_active(old_state);

	 
	if (old_state->self_refresh_active &&
	    old_state->crtc != new_state->crtc)
		return true;

	 
	return old_state->active ||
	       (old_state->self_refresh_active && !new_state->active) ||
	       new_state->self_refresh_active;
}

static void
disable_outputs(struct drm_device *dev, struct drm_atomic_state *old_state)
{
	struct drm_connector *connector;
	struct drm_connector_state *old_conn_state, *new_conn_state;
	struct drm_crtc *crtc;
	struct drm_crtc_state *old_crtc_state, *new_crtc_state;
	int i;

	for_each_oldnew_connector_in_state(old_state, connector, old_conn_state, new_conn_state, i) {
		const struct drm_encoder_helper_funcs *funcs;
		struct drm_encoder *encoder;
		struct drm_bridge *bridge;

		 
		if (!old_conn_state->crtc)
			continue;

		old_crtc_state = drm_atomic_get_old_crtc_state(old_state, old_conn_state->crtc);

		if (new_conn_state->crtc)
			new_crtc_state = drm_atomic_get_new_crtc_state(
						old_state,
						new_conn_state->crtc);
		else
			new_crtc_state = NULL;

		if (!crtc_needs_disable(old_crtc_state, new_crtc_state) ||
		    !drm_atomic_crtc_needs_modeset(old_conn_state->crtc->state))
			continue;

		encoder = old_conn_state->best_encoder;

		 
		if (WARN_ON(!encoder))
			continue;

		funcs = encoder->helper_private;

		drm_dbg_atomic(dev, "disabling [ENCODER:%d:%s]\n",
			       encoder->base.id, encoder->name);

		 
		bridge = drm_bridge_chain_get_first_bridge(encoder);
		drm_atomic_bridge_chain_disable(bridge, old_state);

		 
		if (funcs) {
			if (funcs->atomic_disable)
				funcs->atomic_disable(encoder, old_state);
			else if (new_conn_state->crtc && funcs->prepare)
				funcs->prepare(encoder);
			else if (funcs->disable)
				funcs->disable(encoder);
			else if (funcs->dpms)
				funcs->dpms(encoder, DRM_MODE_DPMS_OFF);
		}

		drm_atomic_bridge_chain_post_disable(bridge, old_state);
	}

	for_each_oldnew_crtc_in_state(old_state, crtc, old_crtc_state, new_crtc_state, i) {
		const struct drm_crtc_helper_funcs *funcs;
		int ret;

		 
		if (!drm_atomic_crtc_needs_modeset(new_crtc_state))
			continue;

		if (!crtc_needs_disable(old_crtc_state, new_crtc_state))
			continue;

		funcs = crtc->helper_private;

		drm_dbg_atomic(dev, "disabling [CRTC:%d:%s]\n",
			       crtc->base.id, crtc->name);


		 
		if (new_crtc_state->enable && funcs->prepare)
			funcs->prepare(crtc);
		else if (funcs->atomic_disable)
			funcs->atomic_disable(crtc, old_state);
		else if (funcs->disable)
			funcs->disable(crtc);
		else if (funcs->dpms)
			funcs->dpms(crtc, DRM_MODE_DPMS_OFF);

		if (!drm_dev_has_vblank(dev))
			continue;

		ret = drm_crtc_vblank_get(crtc);
		 
		if (new_crtc_state->self_refresh_active)
			WARN_ONCE(ret != 0,
				  "driver disabled vblank in self-refresh\n");
		else
			WARN_ONCE(ret != -EINVAL,
				  "driver forgot to call drm_crtc_vblank_off()\n");
		if (ret == 0)
			drm_crtc_vblank_put(crtc);
	}
}

 
void
drm_atomic_helper_update_legacy_modeset_state(struct drm_device *dev,
					      struct drm_atomic_state *old_state)
{
	struct drm_connector *connector;
	struct drm_connector_state *old_conn_state, *new_conn_state;
	struct drm_crtc *crtc;
	struct drm_crtc_state *new_crtc_state;
	int i;

	 
	for_each_oldnew_connector_in_state(old_state, connector, old_conn_state, new_conn_state, i) {
		if (connector->encoder) {
			WARN_ON(!connector->encoder->crtc);

			connector->encoder->crtc = NULL;
			connector->encoder = NULL;
		}

		crtc = new_conn_state->crtc;
		if ((!crtc && old_conn_state->crtc) ||
		    (crtc && drm_atomic_crtc_needs_modeset(crtc->state))) {
			int mode = DRM_MODE_DPMS_OFF;

			if (crtc && crtc->state->active)
				mode = DRM_MODE_DPMS_ON;

			connector->dpms = mode;
		}
	}

	 
	for_each_new_connector_in_state(old_state, connector, new_conn_state, i) {
		if (!new_conn_state->crtc)
			continue;

		if (WARN_ON(!new_conn_state->best_encoder))
			continue;

		connector->encoder = new_conn_state->best_encoder;
		connector->encoder->crtc = new_conn_state->crtc;
	}

	 
	for_each_new_crtc_in_state(old_state, crtc, new_crtc_state, i) {
		struct drm_plane *primary = crtc->primary;
		struct drm_plane_state *new_plane_state;

		crtc->mode = new_crtc_state->mode;
		crtc->enabled = new_crtc_state->enable;

		new_plane_state =
			drm_atomic_get_new_plane_state(old_state, primary);

		if (new_plane_state && new_plane_state->crtc == crtc) {
			crtc->x = new_plane_state->src_x >> 16;
			crtc->y = new_plane_state->src_y >> 16;
		}
	}
}
EXPORT_SYMBOL(drm_atomic_helper_update_legacy_modeset_state);

 
void drm_atomic_helper_calc_timestamping_constants(struct drm_atomic_state *state)
{
	struct drm_crtc_state *new_crtc_state;
	struct drm_crtc *crtc;
	int i;

	for_each_new_crtc_in_state(state, crtc, new_crtc_state, i) {
		if (new_crtc_state->enable)
			drm_calc_timestamping_constants(crtc,
							&new_crtc_state->adjusted_mode);
	}
}
EXPORT_SYMBOL(drm_atomic_helper_calc_timestamping_constants);

static void
crtc_set_mode(struct drm_device *dev, struct drm_atomic_state *old_state)
{
	struct drm_crtc *crtc;
	struct drm_crtc_state *new_crtc_state;
	struct drm_connector *connector;
	struct drm_connector_state *new_conn_state;
	int i;

	for_each_new_crtc_in_state(old_state, crtc, new_crtc_state, i) {
		const struct drm_crtc_helper_funcs *funcs;

		if (!new_crtc_state->mode_changed)
			continue;

		funcs = crtc->helper_private;

		if (new_crtc_state->enable && funcs->mode_set_nofb) {
			drm_dbg_atomic(dev, "modeset on [CRTC:%d:%s]\n",
				       crtc->base.id, crtc->name);

			funcs->mode_set_nofb(crtc);
		}
	}

	for_each_new_connector_in_state(old_state, connector, new_conn_state, i) {
		const struct drm_encoder_helper_funcs *funcs;
		struct drm_encoder *encoder;
		struct drm_display_mode *mode, *adjusted_mode;
		struct drm_bridge *bridge;

		if (!new_conn_state->best_encoder)
			continue;

		encoder = new_conn_state->best_encoder;
		funcs = encoder->helper_private;
		new_crtc_state = new_conn_state->crtc->state;
		mode = &new_crtc_state->mode;
		adjusted_mode = &new_crtc_state->adjusted_mode;

		if (!new_crtc_state->mode_changed)
			continue;

		drm_dbg_atomic(dev, "modeset on [ENCODER:%d:%s]\n",
			       encoder->base.id, encoder->name);

		 
		if (funcs && funcs->atomic_mode_set) {
			funcs->atomic_mode_set(encoder, new_crtc_state,
					       new_conn_state);
		} else if (funcs && funcs->mode_set) {
			funcs->mode_set(encoder, mode, adjusted_mode);
		}

		bridge = drm_bridge_chain_get_first_bridge(encoder);
		drm_bridge_chain_mode_set(bridge, mode, adjusted_mode);
	}
}

 
void drm_atomic_helper_commit_modeset_disables(struct drm_device *dev,
					       struct drm_atomic_state *old_state)
{
	disable_outputs(dev, old_state);

	drm_atomic_helper_update_legacy_modeset_state(dev, old_state);
	drm_atomic_helper_calc_timestamping_constants(old_state);

	crtc_set_mode(dev, old_state);
}
EXPORT_SYMBOL(drm_atomic_helper_commit_modeset_disables);

static void drm_atomic_helper_commit_writebacks(struct drm_device *dev,
						struct drm_atomic_state *old_state)
{
	struct drm_connector *connector;
	struct drm_connector_state *new_conn_state;
	int i;

	for_each_new_connector_in_state(old_state, connector, new_conn_state, i) {
		const struct drm_connector_helper_funcs *funcs;

		funcs = connector->helper_private;
		if (!funcs->atomic_commit)
			continue;

		if (new_conn_state->writeback_job && new_conn_state->writeback_job->fb) {
			WARN_ON(connector->connector_type != DRM_MODE_CONNECTOR_WRITEBACK);
			funcs->atomic_commit(connector, old_state);
		}
	}
}

 
void drm_atomic_helper_commit_modeset_enables(struct drm_device *dev,
					      struct drm_atomic_state *old_state)
{
	struct drm_crtc *crtc;
	struct drm_crtc_state *old_crtc_state;
	struct drm_crtc_state *new_crtc_state;
	struct drm_connector *connector;
	struct drm_connector_state *new_conn_state;
	int i;

	for_each_oldnew_crtc_in_state(old_state, crtc, old_crtc_state, new_crtc_state, i) {
		const struct drm_crtc_helper_funcs *funcs;

		 
		if (!drm_atomic_crtc_needs_modeset(new_crtc_state))
			continue;

		if (!new_crtc_state->active)
			continue;

		funcs = crtc->helper_private;

		if (new_crtc_state->enable) {
			drm_dbg_atomic(dev, "enabling [CRTC:%d:%s]\n",
				       crtc->base.id, crtc->name);
			if (funcs->atomic_enable)
				funcs->atomic_enable(crtc, old_state);
			else if (funcs->commit)
				funcs->commit(crtc);
		}
	}

	for_each_new_connector_in_state(old_state, connector, new_conn_state, i) {
		const struct drm_encoder_helper_funcs *funcs;
		struct drm_encoder *encoder;
		struct drm_bridge *bridge;

		if (!new_conn_state->best_encoder)
			continue;

		if (!new_conn_state->crtc->state->active ||
		    !drm_atomic_crtc_needs_modeset(new_conn_state->crtc->state))
			continue;

		encoder = new_conn_state->best_encoder;
		funcs = encoder->helper_private;

		drm_dbg_atomic(dev, "enabling [ENCODER:%d:%s]\n",
			       encoder->base.id, encoder->name);

		 
		bridge = drm_bridge_chain_get_first_bridge(encoder);
		drm_atomic_bridge_chain_pre_enable(bridge, old_state);

		if (funcs) {
			if (funcs->atomic_enable)
				funcs->atomic_enable(encoder, old_state);
			else if (funcs->enable)
				funcs->enable(encoder);
			else if (funcs->commit)
				funcs->commit(encoder);
		}

		drm_atomic_bridge_chain_enable(bridge, old_state);
	}

	drm_atomic_helper_commit_writebacks(dev, old_state);
}
EXPORT_SYMBOL(drm_atomic_helper_commit_modeset_enables);

 
static void set_fence_deadline(struct drm_device *dev,
			       struct drm_atomic_state *state)
{
	struct drm_crtc *crtc;
	struct drm_crtc_state *new_crtc_state;
	struct drm_plane *plane;
	struct drm_plane_state *new_plane_state;
	ktime_t vbltime = 0;
	int i;

	for_each_new_crtc_in_state (state, crtc, new_crtc_state, i) {
		ktime_t v;

		if (drm_atomic_crtc_needs_modeset(new_crtc_state))
			continue;

		if (!new_crtc_state->active)
			continue;

		if (drm_crtc_next_vblank_start(crtc, &v))
			continue;

		if (!vbltime || ktime_before(v, vbltime))
			vbltime = v;
	}

	 
	if (!vbltime)
		return;

	for_each_new_plane_in_state (state, plane, new_plane_state, i) {
		if (!new_plane_state->fence)
			continue;
		dma_fence_set_deadline(new_plane_state->fence, vbltime);
	}
}

 
int drm_atomic_helper_wait_for_fences(struct drm_device *dev,
				      struct drm_atomic_state *state,
				      bool pre_swap)
{
	struct drm_plane *plane;
	struct drm_plane_state *new_plane_state;
	int i, ret;

	set_fence_deadline(dev, state);

	for_each_new_plane_in_state(state, plane, new_plane_state, i) {
		if (!new_plane_state->fence)
			continue;

		WARN_ON(!new_plane_state->fb);

		 
		ret = dma_fence_wait(new_plane_state->fence, pre_swap);
		if (ret)
			return ret;

		dma_fence_put(new_plane_state->fence);
		new_plane_state->fence = NULL;
	}

	return 0;
}
EXPORT_SYMBOL(drm_atomic_helper_wait_for_fences);

 
void
drm_atomic_helper_wait_for_vblanks(struct drm_device *dev,
		struct drm_atomic_state *old_state)
{
	struct drm_crtc *crtc;
	struct drm_crtc_state *old_crtc_state, *new_crtc_state;
	int i, ret;
	unsigned int crtc_mask = 0;

	  
	if (old_state->legacy_cursor_update)
		return;

	for_each_oldnew_crtc_in_state(old_state, crtc, old_crtc_state, new_crtc_state, i) {
		if (!new_crtc_state->active)
			continue;

		ret = drm_crtc_vblank_get(crtc);
		if (ret != 0)
			continue;

		crtc_mask |= drm_crtc_mask(crtc);
		old_state->crtcs[i].last_vblank_count = drm_crtc_vblank_count(crtc);
	}

	for_each_old_crtc_in_state(old_state, crtc, old_crtc_state, i) {
		if (!(crtc_mask & drm_crtc_mask(crtc)))
			continue;

		ret = wait_event_timeout(dev->vblank[i].queue,
				old_state->crtcs[i].last_vblank_count !=
					drm_crtc_vblank_count(crtc),
				msecs_to_jiffies(100));

		WARN(!ret, "[CRTC:%d:%s] vblank wait timed out\n",
		     crtc->base.id, crtc->name);

		drm_crtc_vblank_put(crtc);
	}
}
EXPORT_SYMBOL(drm_atomic_helper_wait_for_vblanks);

 
void drm_atomic_helper_wait_for_flip_done(struct drm_device *dev,
					  struct drm_atomic_state *old_state)
{
	struct drm_crtc *crtc;
	int i;

	for (i = 0; i < dev->mode_config.num_crtc; i++) {
		struct drm_crtc_commit *commit = old_state->crtcs[i].commit;
		int ret;

		crtc = old_state->crtcs[i].ptr;

		if (!crtc || !commit)
			continue;

		ret = wait_for_completion_timeout(&commit->flip_done, 10 * HZ);
		if (ret == 0)
			drm_err(dev, "[CRTC:%d:%s] flip_done timed out\n",
				crtc->base.id, crtc->name);
	}

	if (old_state->fake_commit)
		complete_all(&old_state->fake_commit->flip_done);
}
EXPORT_SYMBOL(drm_atomic_helper_wait_for_flip_done);

 
void drm_atomic_helper_commit_tail(struct drm_atomic_state *old_state)
{
	struct drm_device *dev = old_state->dev;

	drm_atomic_helper_commit_modeset_disables(dev, old_state);

	drm_atomic_helper_commit_planes(dev, old_state, 0);

	drm_atomic_helper_commit_modeset_enables(dev, old_state);

	drm_atomic_helper_fake_vblank(old_state);

	drm_atomic_helper_commit_hw_done(old_state);

	drm_atomic_helper_wait_for_vblanks(dev, old_state);

	drm_atomic_helper_cleanup_planes(dev, old_state);
}
EXPORT_SYMBOL(drm_atomic_helper_commit_tail);

 
void drm_atomic_helper_commit_tail_rpm(struct drm_atomic_state *old_state)
{
	struct drm_device *dev = old_state->dev;

	drm_atomic_helper_commit_modeset_disables(dev, old_state);

	drm_atomic_helper_commit_modeset_enables(dev, old_state);

	drm_atomic_helper_commit_planes(dev, old_state,
					DRM_PLANE_COMMIT_ACTIVE_ONLY);

	drm_atomic_helper_fake_vblank(old_state);

	drm_atomic_helper_commit_hw_done(old_state);

	drm_atomic_helper_wait_for_vblanks(dev, old_state);

	drm_atomic_helper_cleanup_planes(dev, old_state);
}
EXPORT_SYMBOL(drm_atomic_helper_commit_tail_rpm);

static void commit_tail(struct drm_atomic_state *old_state)
{
	struct drm_device *dev = old_state->dev;
	const struct drm_mode_config_helper_funcs *funcs;
	struct drm_crtc_state *new_crtc_state;
	struct drm_crtc *crtc;
	ktime_t start;
	s64 commit_time_ms;
	unsigned int i, new_self_refresh_mask = 0;

	funcs = dev->mode_config.helper_private;

	 
	start = ktime_get();

	drm_atomic_helper_wait_for_fences(dev, old_state, false);

	drm_atomic_helper_wait_for_dependencies(old_state);

	 
	for_each_new_crtc_in_state(old_state, crtc, new_crtc_state, i)
		if (new_crtc_state->self_refresh_active)
			new_self_refresh_mask |= BIT(i);

	if (funcs && funcs->atomic_commit_tail)
		funcs->atomic_commit_tail(old_state);
	else
		drm_atomic_helper_commit_tail(old_state);

	commit_time_ms = ktime_ms_delta(ktime_get(), start);
	if (commit_time_ms > 0)
		drm_self_refresh_helper_update_avg_times(old_state,
						 (unsigned long)commit_time_ms,
						 new_self_refresh_mask);

	drm_atomic_helper_commit_cleanup_done(old_state);

	drm_atomic_state_put(old_state);
}

static void commit_work(struct work_struct *work)
{
	struct drm_atomic_state *state = container_of(work,
						      struct drm_atomic_state,
						      commit_work);
	commit_tail(state);
}

 
int drm_atomic_helper_async_check(struct drm_device *dev,
				   struct drm_atomic_state *state)
{
	struct drm_crtc *crtc;
	struct drm_crtc_state *crtc_state;
	struct drm_plane *plane = NULL;
	struct drm_plane_state *old_plane_state = NULL;
	struct drm_plane_state *new_plane_state = NULL;
	const struct drm_plane_helper_funcs *funcs;
	int i, ret, n_planes = 0;

	for_each_new_crtc_in_state(state, crtc, crtc_state, i) {
		if (drm_atomic_crtc_needs_modeset(crtc_state))
			return -EINVAL;
	}

	for_each_oldnew_plane_in_state(state, plane, old_plane_state, new_plane_state, i)
		n_planes++;

	 
	if (n_planes != 1) {
		drm_dbg_atomic(dev,
			       "only single plane async updates are supported\n");
		return -EINVAL;
	}

	if (!new_plane_state->crtc ||
	    old_plane_state->crtc != new_plane_state->crtc) {
		drm_dbg_atomic(dev,
			       "[PLANE:%d:%s] async update cannot change CRTC\n",
			       plane->base.id, plane->name);
		return -EINVAL;
	}

	funcs = plane->helper_private;
	if (!funcs->atomic_async_update) {
		drm_dbg_atomic(dev,
			       "[PLANE:%d:%s] driver does not support async updates\n",
			       plane->base.id, plane->name);
		return -EINVAL;
	}

	if (new_plane_state->fence) {
		drm_dbg_atomic(dev,
			       "[PLANE:%d:%s] missing fence for async update\n",
			       plane->base.id, plane->name);
		return -EINVAL;
	}

	 
	if (old_plane_state->commit &&
	    !try_wait_for_completion(&old_plane_state->commit->hw_done)) {
		drm_dbg_atomic(dev,
			       "[PLANE:%d:%s] inflight previous commit preventing async commit\n",
			       plane->base.id, plane->name);
		return -EBUSY;
	}

	ret = funcs->atomic_async_check(plane, state);
	if (ret != 0)
		drm_dbg_atomic(dev,
			       "[PLANE:%d:%s] driver async check failed\n",
			       plane->base.id, plane->name);
	return ret;
}
EXPORT_SYMBOL(drm_atomic_helper_async_check);

 
void drm_atomic_helper_async_commit(struct drm_device *dev,
				    struct drm_atomic_state *state)
{
	struct drm_plane *plane;
	struct drm_plane_state *plane_state;
	const struct drm_plane_helper_funcs *funcs;
	int i;

	for_each_new_plane_in_state(state, plane, plane_state, i) {
		struct drm_framebuffer *new_fb = plane_state->fb;
		struct drm_framebuffer *old_fb = plane->state->fb;

		funcs = plane->helper_private;
		funcs->atomic_async_update(plane, state);

		 
		WARN_ON_ONCE(plane->state->fb != new_fb);
		WARN_ON_ONCE(plane->state->crtc_x != plane_state->crtc_x);
		WARN_ON_ONCE(plane->state->crtc_y != plane_state->crtc_y);
		WARN_ON_ONCE(plane->state->src_x != plane_state->src_x);
		WARN_ON_ONCE(plane->state->src_y != plane_state->src_y);

		 
		WARN_ON_ONCE(plane_state->fb != old_fb);
	}
}
EXPORT_SYMBOL(drm_atomic_helper_async_commit);

 
int drm_atomic_helper_commit(struct drm_device *dev,
			     struct drm_atomic_state *state,
			     bool nonblock)
{
	int ret;

	if (state->async_update) {
		ret = drm_atomic_helper_prepare_planes(dev, state);
		if (ret)
			return ret;

		drm_atomic_helper_async_commit(dev, state);
		drm_atomic_helper_unprepare_planes(dev, state);

		return 0;
	}

	ret = drm_atomic_helper_setup_commit(state, nonblock);
	if (ret)
		return ret;

	INIT_WORK(&state->commit_work, commit_work);

	ret = drm_atomic_helper_prepare_planes(dev, state);
	if (ret)
		return ret;

	if (!nonblock) {
		ret = drm_atomic_helper_wait_for_fences(dev, state, true);
		if (ret)
			goto err;
	}

	 

	ret = drm_atomic_helper_swap_state(state, true);
	if (ret)
		goto err;

	 

	drm_atomic_state_get(state);
	if (nonblock)
		queue_work(system_unbound_wq, &state->commit_work);
	else
		commit_tail(state);

	return 0;

err:
	drm_atomic_helper_unprepare_planes(dev, state);
	return ret;
}
EXPORT_SYMBOL(drm_atomic_helper_commit);

 

static int stall_checks(struct drm_crtc *crtc, bool nonblock)
{
	struct drm_crtc_commit *commit, *stall_commit = NULL;
	bool completed = true;
	int i;
	long ret = 0;

	spin_lock(&crtc->commit_lock);
	i = 0;
	list_for_each_entry(commit, &crtc->commit_list, commit_entry) {
		if (i == 0) {
			completed = try_wait_for_completion(&commit->flip_done);
			 
			if (!completed && nonblock) {
				spin_unlock(&crtc->commit_lock);
				drm_dbg_atomic(crtc->dev,
					       "[CRTC:%d:%s] busy with a previous commit\n",
					       crtc->base.id, crtc->name);

				return -EBUSY;
			}
		} else if (i == 1) {
			stall_commit = drm_crtc_commit_get(commit);
			break;
		}

		i++;
	}
	spin_unlock(&crtc->commit_lock);

	if (!stall_commit)
		return 0;

	 
	ret = wait_for_completion_interruptible_timeout(&stall_commit->cleanup_done,
							10*HZ);
	if (ret == 0)
		drm_err(crtc->dev, "[CRTC:%d:%s] cleanup_done timed out\n",
			crtc->base.id, crtc->name);

	drm_crtc_commit_put(stall_commit);

	return ret < 0 ? ret : 0;
}

static void release_crtc_commit(struct completion *completion)
{
	struct drm_crtc_commit *commit = container_of(completion,
						      typeof(*commit),
						      flip_done);

	drm_crtc_commit_put(commit);
}

static void init_commit(struct drm_crtc_commit *commit, struct drm_crtc *crtc)
{
	init_completion(&commit->flip_done);
	init_completion(&commit->hw_done);
	init_completion(&commit->cleanup_done);
	INIT_LIST_HEAD(&commit->commit_entry);
	kref_init(&commit->ref);
	commit->crtc = crtc;
}

static struct drm_crtc_commit *
crtc_or_fake_commit(struct drm_atomic_state *state, struct drm_crtc *crtc)
{
	if (crtc) {
		struct drm_crtc_state *new_crtc_state;

		new_crtc_state = drm_atomic_get_new_crtc_state(state, crtc);

		return new_crtc_state->commit;
	}

	if (!state->fake_commit) {
		state->fake_commit = kzalloc(sizeof(*state->fake_commit), GFP_KERNEL);
		if (!state->fake_commit)
			return NULL;

		init_commit(state->fake_commit, NULL);
	}

	return state->fake_commit;
}

 
int drm_atomic_helper_setup_commit(struct drm_atomic_state *state,
				   bool nonblock)
{
	struct drm_crtc *crtc;
	struct drm_crtc_state *old_crtc_state, *new_crtc_state;
	struct drm_connector *conn;
	struct drm_connector_state *old_conn_state, *new_conn_state;
	struct drm_plane *plane;
	struct drm_plane_state *old_plane_state, *new_plane_state;
	struct drm_crtc_commit *commit;
	const struct drm_mode_config_helper_funcs *funcs;
	int i, ret;

	funcs = state->dev->mode_config.helper_private;

	for_each_oldnew_crtc_in_state(state, crtc, old_crtc_state, new_crtc_state, i) {
		commit = kzalloc(sizeof(*commit), GFP_KERNEL);
		if (!commit)
			return -ENOMEM;

		init_commit(commit, crtc);

		new_crtc_state->commit = commit;

		ret = stall_checks(crtc, nonblock);
		if (ret)
			return ret;

		 
		if (!old_crtc_state->active && !new_crtc_state->active) {
			complete_all(&commit->flip_done);
			continue;
		}

		 
		if (state->legacy_cursor_update) {
			complete_all(&commit->flip_done);
			continue;
		}

		if (!new_crtc_state->event) {
			commit->event = kzalloc(sizeof(*commit->event),
						GFP_KERNEL);
			if (!commit->event)
				return -ENOMEM;

			new_crtc_state->event = commit->event;
		}

		new_crtc_state->event->base.completion = &commit->flip_done;
		new_crtc_state->event->base.completion_release = release_crtc_commit;
		drm_crtc_commit_get(commit);

		commit->abort_completion = true;

		state->crtcs[i].commit = commit;
		drm_crtc_commit_get(commit);
	}

	for_each_oldnew_connector_in_state(state, conn, old_conn_state, new_conn_state, i) {
		 
		if (nonblock && old_conn_state->commit &&
		    !try_wait_for_completion(&old_conn_state->commit->flip_done)) {
			drm_dbg_atomic(conn->dev,
				       "[CONNECTOR:%d:%s] busy with a previous commit\n",
				       conn->base.id, conn->name);

			return -EBUSY;
		}

		 
		commit = crtc_or_fake_commit(state, new_conn_state->crtc ?: old_conn_state->crtc);
		if (!commit)
			return -ENOMEM;

		new_conn_state->commit = drm_crtc_commit_get(commit);
	}

	for_each_oldnew_plane_in_state(state, plane, old_plane_state, new_plane_state, i) {
		 
		if (nonblock && old_plane_state->commit &&
		    !try_wait_for_completion(&old_plane_state->commit->flip_done)) {
			drm_dbg_atomic(plane->dev,
				       "[PLANE:%d:%s] busy with a previous commit\n",
				       plane->base.id, plane->name);

			return -EBUSY;
		}

		 
		commit = crtc_or_fake_commit(state, new_plane_state->crtc ?: old_plane_state->crtc);
		if (!commit)
			return -ENOMEM;

		new_plane_state->commit = drm_crtc_commit_get(commit);
	}

	if (funcs && funcs->atomic_commit_setup)
		return funcs->atomic_commit_setup(state);

	return 0;
}
EXPORT_SYMBOL(drm_atomic_helper_setup_commit);

 
void drm_atomic_helper_wait_for_dependencies(struct drm_atomic_state *old_state)
{
	struct drm_crtc *crtc;
	struct drm_crtc_state *old_crtc_state;
	struct drm_plane *plane;
	struct drm_plane_state *old_plane_state;
	struct drm_connector *conn;
	struct drm_connector_state *old_conn_state;
	int i;
	long ret;

	for_each_old_crtc_in_state(old_state, crtc, old_crtc_state, i) {
		ret = drm_crtc_commit_wait(old_crtc_state->commit);
		if (ret)
			drm_err(crtc->dev,
				"[CRTC:%d:%s] commit wait timed out\n",
				crtc->base.id, crtc->name);
	}

	for_each_old_connector_in_state(old_state, conn, old_conn_state, i) {
		ret = drm_crtc_commit_wait(old_conn_state->commit);
		if (ret)
			drm_err(conn->dev,
				"[CONNECTOR:%d:%s] commit wait timed out\n",
				conn->base.id, conn->name);
	}

	for_each_old_plane_in_state(old_state, plane, old_plane_state, i) {
		ret = drm_crtc_commit_wait(old_plane_state->commit);
		if (ret)
			drm_err(plane->dev,
				"[PLANE:%d:%s] commit wait timed out\n",
				plane->base.id, plane->name);
	}
}
EXPORT_SYMBOL(drm_atomic_helper_wait_for_dependencies);

 
void drm_atomic_helper_fake_vblank(struct drm_atomic_state *old_state)
{
	struct drm_crtc_state *new_crtc_state;
	struct drm_crtc *crtc;
	int i;

	for_each_new_crtc_in_state(old_state, crtc, new_crtc_state, i) {
		unsigned long flags;

		if (!new_crtc_state->no_vblank)
			continue;

		spin_lock_irqsave(&old_state->dev->event_lock, flags);
		if (new_crtc_state->event) {
			drm_crtc_send_vblank_event(crtc,
						   new_crtc_state->event);
			new_crtc_state->event = NULL;
		}
		spin_unlock_irqrestore(&old_state->dev->event_lock, flags);
	}
}
EXPORT_SYMBOL(drm_atomic_helper_fake_vblank);

 
void drm_atomic_helper_commit_hw_done(struct drm_atomic_state *old_state)
{
	struct drm_crtc *crtc;
	struct drm_crtc_state *old_crtc_state, *new_crtc_state;
	struct drm_crtc_commit *commit;
	int i;

	for_each_oldnew_crtc_in_state(old_state, crtc, old_crtc_state, new_crtc_state, i) {
		commit = new_crtc_state->commit;
		if (!commit)
			continue;

		 
		if (old_crtc_state->commit)
			drm_crtc_commit_put(old_crtc_state->commit);

		old_crtc_state->commit = drm_crtc_commit_get(commit);

		 
		WARN_ON(new_crtc_state->event);
		complete_all(&commit->hw_done);
	}

	if (old_state->fake_commit) {
		complete_all(&old_state->fake_commit->hw_done);
		complete_all(&old_state->fake_commit->flip_done);
	}
}
EXPORT_SYMBOL(drm_atomic_helper_commit_hw_done);

 
void drm_atomic_helper_commit_cleanup_done(struct drm_atomic_state *old_state)
{
	struct drm_crtc *crtc;
	struct drm_crtc_state *old_crtc_state;
	struct drm_crtc_commit *commit;
	int i;

	for_each_old_crtc_in_state(old_state, crtc, old_crtc_state, i) {
		commit = old_crtc_state->commit;
		if (WARN_ON(!commit))
			continue;

		complete_all(&commit->cleanup_done);
		WARN_ON(!try_wait_for_completion(&commit->hw_done));

		spin_lock(&crtc->commit_lock);
		list_del(&commit->commit_entry);
		spin_unlock(&crtc->commit_lock);
	}

	if (old_state->fake_commit) {
		complete_all(&old_state->fake_commit->cleanup_done);
		WARN_ON(!try_wait_for_completion(&old_state->fake_commit->hw_done));
	}
}
EXPORT_SYMBOL(drm_atomic_helper_commit_cleanup_done);

 
int drm_atomic_helper_prepare_planes(struct drm_device *dev,
				     struct drm_atomic_state *state)
{
	struct drm_connector *connector;
	struct drm_connector_state *new_conn_state;
	struct drm_plane *plane;
	struct drm_plane_state *new_plane_state;
	int ret, i, j;

	for_each_new_connector_in_state(state, connector, new_conn_state, i) {
		if (!new_conn_state->writeback_job)
			continue;

		ret = drm_writeback_prepare_job(new_conn_state->writeback_job);
		if (ret < 0)
			return ret;
	}

	for_each_new_plane_in_state(state, plane, new_plane_state, i) {
		const struct drm_plane_helper_funcs *funcs;

		funcs = plane->helper_private;

		if (funcs->prepare_fb) {
			ret = funcs->prepare_fb(plane, new_plane_state);
			if (ret)
				goto fail_prepare_fb;
		} else {
			WARN_ON_ONCE(funcs->cleanup_fb);

			if (!drm_core_check_feature(dev, DRIVER_GEM))
				continue;

			ret = drm_gem_plane_helper_prepare_fb(plane, new_plane_state);
			if (ret)
				goto fail_prepare_fb;
		}
	}

	for_each_new_plane_in_state(state, plane, new_plane_state, i) {
		const struct drm_plane_helper_funcs *funcs = plane->helper_private;

		if (funcs->begin_fb_access) {
			ret = funcs->begin_fb_access(plane, new_plane_state);
			if (ret)
				goto fail_begin_fb_access;
		}
	}

	return 0;

fail_begin_fb_access:
	for_each_new_plane_in_state(state, plane, new_plane_state, j) {
		const struct drm_plane_helper_funcs *funcs = plane->helper_private;

		if (j >= i)
			continue;

		if (funcs->end_fb_access)
			funcs->end_fb_access(plane, new_plane_state);
	}
	i = j;  
fail_prepare_fb:
	for_each_new_plane_in_state(state, plane, new_plane_state, j) {
		const struct drm_plane_helper_funcs *funcs;

		if (j >= i)
			continue;

		funcs = plane->helper_private;

		if (funcs->cleanup_fb)
			funcs->cleanup_fb(plane, new_plane_state);
	}

	return ret;
}
EXPORT_SYMBOL(drm_atomic_helper_prepare_planes);

 
void drm_atomic_helper_unprepare_planes(struct drm_device *dev,
					struct drm_atomic_state *state)
{
	struct drm_plane *plane;
	struct drm_plane_state *new_plane_state;
	int i;

	for_each_new_plane_in_state(state, plane, new_plane_state, i) {
		const struct drm_plane_helper_funcs *funcs = plane->helper_private;

		if (funcs->end_fb_access)
			funcs->end_fb_access(plane, new_plane_state);
	}

	for_each_new_plane_in_state(state, plane, new_plane_state, i) {
		const struct drm_plane_helper_funcs *funcs = plane->helper_private;

		if (funcs->cleanup_fb)
			funcs->cleanup_fb(plane, new_plane_state);
	}
}
EXPORT_SYMBOL(drm_atomic_helper_unprepare_planes);

static bool plane_crtc_active(const struct drm_plane_state *state)
{
	return state->crtc && state->crtc->state->active;
}

 
void drm_atomic_helper_commit_planes(struct drm_device *dev,
				     struct drm_atomic_state *old_state,
				     uint32_t flags)
{
	struct drm_crtc *crtc;
	struct drm_crtc_state *old_crtc_state, *new_crtc_state;
	struct drm_plane *plane;
	struct drm_plane_state *old_plane_state, *new_plane_state;
	int i;
	bool active_only = flags & DRM_PLANE_COMMIT_ACTIVE_ONLY;
	bool no_disable = flags & DRM_PLANE_COMMIT_NO_DISABLE_AFTER_MODESET;

	for_each_oldnew_crtc_in_state(old_state, crtc, old_crtc_state, new_crtc_state, i) {
		const struct drm_crtc_helper_funcs *funcs;

		funcs = crtc->helper_private;

		if (!funcs || !funcs->atomic_begin)
			continue;

		if (active_only && !new_crtc_state->active)
			continue;

		funcs->atomic_begin(crtc, old_state);
	}

	for_each_oldnew_plane_in_state(old_state, plane, old_plane_state, new_plane_state, i) {
		const struct drm_plane_helper_funcs *funcs;
		bool disabling;

		funcs = plane->helper_private;

		if (!funcs)
			continue;

		disabling = drm_atomic_plane_disabling(old_plane_state,
						       new_plane_state);

		if (active_only) {
			 
			if (!disabling && !plane_crtc_active(new_plane_state))
				continue;
			if (disabling && !plane_crtc_active(old_plane_state))
				continue;
		}

		 
		if (disabling && funcs->atomic_disable) {
			struct drm_crtc_state *crtc_state;

			crtc_state = old_plane_state->crtc->state;

			if (drm_atomic_crtc_needs_modeset(crtc_state) &&
			    no_disable)
				continue;

			funcs->atomic_disable(plane, old_state);
		} else if (new_plane_state->crtc || disabling) {
			funcs->atomic_update(plane, old_state);

			if (!disabling && funcs->atomic_enable) {
				if (drm_atomic_plane_enabling(old_plane_state, new_plane_state))
					funcs->atomic_enable(plane, old_state);
			}
		}
	}

	for_each_oldnew_crtc_in_state(old_state, crtc, old_crtc_state, new_crtc_state, i) {
		const struct drm_crtc_helper_funcs *funcs;

		funcs = crtc->helper_private;

		if (!funcs || !funcs->atomic_flush)
			continue;

		if (active_only && !new_crtc_state->active)
			continue;

		funcs->atomic_flush(crtc, old_state);
	}

	 
	for_each_old_plane_in_state(old_state, plane, old_plane_state, i) {
		const struct drm_plane_helper_funcs *funcs = plane->helper_private;

		if (funcs->end_fb_access)
			funcs->end_fb_access(plane, old_plane_state);
	}
}
EXPORT_SYMBOL(drm_atomic_helper_commit_planes);

 
void
drm_atomic_helper_commit_planes_on_crtc(struct drm_crtc_state *old_crtc_state)
{
	const struct drm_crtc_helper_funcs *crtc_funcs;
	struct drm_crtc *crtc = old_crtc_state->crtc;
	struct drm_atomic_state *old_state = old_crtc_state->state;
	struct drm_crtc_state *new_crtc_state =
		drm_atomic_get_new_crtc_state(old_state, crtc);
	struct drm_plane *plane;
	unsigned int plane_mask;

	plane_mask = old_crtc_state->plane_mask;
	plane_mask |= new_crtc_state->plane_mask;

	crtc_funcs = crtc->helper_private;
	if (crtc_funcs && crtc_funcs->atomic_begin)
		crtc_funcs->atomic_begin(crtc, old_state);

	drm_for_each_plane_mask(plane, crtc->dev, plane_mask) {
		struct drm_plane_state *old_plane_state =
			drm_atomic_get_old_plane_state(old_state, plane);
		struct drm_plane_state *new_plane_state =
			drm_atomic_get_new_plane_state(old_state, plane);
		const struct drm_plane_helper_funcs *plane_funcs;
		bool disabling;

		plane_funcs = plane->helper_private;

		if (!old_plane_state || !plane_funcs)
			continue;

		WARN_ON(new_plane_state->crtc &&
			new_plane_state->crtc != crtc);

		disabling = drm_atomic_plane_disabling(old_plane_state, new_plane_state);

		if (disabling && plane_funcs->atomic_disable) {
			plane_funcs->atomic_disable(plane, old_state);
		} else if (new_plane_state->crtc || disabling) {
			plane_funcs->atomic_update(plane, old_state);

			if (!disabling && plane_funcs->atomic_enable) {
				if (drm_atomic_plane_enabling(old_plane_state, new_plane_state))
					plane_funcs->atomic_enable(plane, old_state);
			}
		}
	}

	if (crtc_funcs && crtc_funcs->atomic_flush)
		crtc_funcs->atomic_flush(crtc, old_state);
}
EXPORT_SYMBOL(drm_atomic_helper_commit_planes_on_crtc);

 
void
drm_atomic_helper_disable_planes_on_crtc(struct drm_crtc_state *old_crtc_state,
					 bool atomic)
{
	struct drm_crtc *crtc = old_crtc_state->crtc;
	const struct drm_crtc_helper_funcs *crtc_funcs =
		crtc->helper_private;
	struct drm_plane *plane;

	if (atomic && crtc_funcs && crtc_funcs->atomic_begin)
		crtc_funcs->atomic_begin(crtc, NULL);

	drm_atomic_crtc_state_for_each_plane(plane, old_crtc_state) {
		const struct drm_plane_helper_funcs *plane_funcs =
			plane->helper_private;

		if (!plane_funcs)
			continue;

		WARN_ON(!plane_funcs->atomic_disable);
		if (plane_funcs->atomic_disable)
			plane_funcs->atomic_disable(plane, NULL);
	}

	if (atomic && crtc_funcs && crtc_funcs->atomic_flush)
		crtc_funcs->atomic_flush(crtc, NULL);
}
EXPORT_SYMBOL(drm_atomic_helper_disable_planes_on_crtc);

 
void drm_atomic_helper_cleanup_planes(struct drm_device *dev,
				      struct drm_atomic_state *old_state)
{
	struct drm_plane *plane;
	struct drm_plane_state *old_plane_state;
	int i;

	for_each_old_plane_in_state(old_state, plane, old_plane_state, i) {
		const struct drm_plane_helper_funcs *funcs = plane->helper_private;

		if (funcs->cleanup_fb)
			funcs->cleanup_fb(plane, old_plane_state);
	}
}
EXPORT_SYMBOL(drm_atomic_helper_cleanup_planes);

 
int drm_atomic_helper_swap_state(struct drm_atomic_state *state,
				  bool stall)
{
	int i, ret;
	struct drm_connector *connector;
	struct drm_connector_state *old_conn_state, *new_conn_state;
	struct drm_crtc *crtc;
	struct drm_crtc_state *old_crtc_state, *new_crtc_state;
	struct drm_plane *plane;
	struct drm_plane_state *old_plane_state, *new_plane_state;
	struct drm_crtc_commit *commit;
	struct drm_private_obj *obj;
	struct drm_private_state *old_obj_state, *new_obj_state;

	if (stall) {
		 

		for_each_old_crtc_in_state(state, crtc, old_crtc_state, i) {
			commit = old_crtc_state->commit;

			if (!commit)
				continue;

			ret = wait_for_completion_interruptible(&commit->hw_done);
			if (ret)
				return ret;
		}

		for_each_old_connector_in_state(state, connector, old_conn_state, i) {
			commit = old_conn_state->commit;

			if (!commit)
				continue;

			ret = wait_for_completion_interruptible(&commit->hw_done);
			if (ret)
				return ret;
		}

		for_each_old_plane_in_state(state, plane, old_plane_state, i) {
			commit = old_plane_state->commit;

			if (!commit)
				continue;

			ret = wait_for_completion_interruptible(&commit->hw_done);
			if (ret)
				return ret;
		}
	}

	for_each_oldnew_connector_in_state(state, connector, old_conn_state, new_conn_state, i) {
		WARN_ON(connector->state != old_conn_state);

		old_conn_state->state = state;
		new_conn_state->state = NULL;

		state->connectors[i].state = old_conn_state;
		connector->state = new_conn_state;
	}

	for_each_oldnew_crtc_in_state(state, crtc, old_crtc_state, new_crtc_state, i) {
		WARN_ON(crtc->state != old_crtc_state);

		old_crtc_state->state = state;
		new_crtc_state->state = NULL;

		state->crtcs[i].state = old_crtc_state;
		crtc->state = new_crtc_state;

		if (new_crtc_state->commit) {
			spin_lock(&crtc->commit_lock);
			list_add(&new_crtc_state->commit->commit_entry,
				 &crtc->commit_list);
			spin_unlock(&crtc->commit_lock);

			new_crtc_state->commit->event = NULL;
		}
	}

	for_each_oldnew_plane_in_state(state, plane, old_plane_state, new_plane_state, i) {
		WARN_ON(plane->state != old_plane_state);

		old_plane_state->state = state;
		new_plane_state->state = NULL;

		state->planes[i].state = old_plane_state;
		plane->state = new_plane_state;
	}

	for_each_oldnew_private_obj_in_state(state, obj, old_obj_state, new_obj_state, i) {
		WARN_ON(obj->state != old_obj_state);

		old_obj_state->state = state;
		new_obj_state->state = NULL;

		state->private_objs[i].state = old_obj_state;
		obj->state = new_obj_state;
	}

	return 0;
}
EXPORT_SYMBOL(drm_atomic_helper_swap_state);

 
int drm_atomic_helper_update_plane(struct drm_plane *plane,
				   struct drm_crtc *crtc,
				   struct drm_framebuffer *fb,
				   int crtc_x, int crtc_y,
				   unsigned int crtc_w, unsigned int crtc_h,
				   uint32_t src_x, uint32_t src_y,
				   uint32_t src_w, uint32_t src_h,
				   struct drm_modeset_acquire_ctx *ctx)
{
	struct drm_atomic_state *state;
	struct drm_plane_state *plane_state;
	int ret = 0;

	state = drm_atomic_state_alloc(plane->dev);
	if (!state)
		return -ENOMEM;

	state->acquire_ctx = ctx;
	plane_state = drm_atomic_get_plane_state(state, plane);
	if (IS_ERR(plane_state)) {
		ret = PTR_ERR(plane_state);
		goto fail;
	}

	ret = drm_atomic_set_crtc_for_plane(plane_state, crtc);
	if (ret != 0)
		goto fail;
	drm_atomic_set_fb_for_plane(plane_state, fb);
	plane_state->crtc_x = crtc_x;
	plane_state->crtc_y = crtc_y;
	plane_state->crtc_w = crtc_w;
	plane_state->crtc_h = crtc_h;
	plane_state->src_x = src_x;
	plane_state->src_y = src_y;
	plane_state->src_w = src_w;
	plane_state->src_h = src_h;

	if (plane == crtc->cursor)
		state->legacy_cursor_update = true;

	ret = drm_atomic_commit(state);
fail:
	drm_atomic_state_put(state);
	return ret;
}
EXPORT_SYMBOL(drm_atomic_helper_update_plane);

 
int drm_atomic_helper_disable_plane(struct drm_plane *plane,
				    struct drm_modeset_acquire_ctx *ctx)
{
	struct drm_atomic_state *state;
	struct drm_plane_state *plane_state;
	int ret = 0;

	state = drm_atomic_state_alloc(plane->dev);
	if (!state)
		return -ENOMEM;

	state->acquire_ctx = ctx;
	plane_state = drm_atomic_get_plane_state(state, plane);
	if (IS_ERR(plane_state)) {
		ret = PTR_ERR(plane_state);
		goto fail;
	}

	if (plane_state->crtc && plane_state->crtc->cursor == plane)
		plane_state->state->legacy_cursor_update = true;

	ret = __drm_atomic_helper_disable_plane(plane, plane_state);
	if (ret != 0)
		goto fail;

	ret = drm_atomic_commit(state);
fail:
	drm_atomic_state_put(state);
	return ret;
}
EXPORT_SYMBOL(drm_atomic_helper_disable_plane);

 
int drm_atomic_helper_set_config(struct drm_mode_set *set,
				 struct drm_modeset_acquire_ctx *ctx)
{
	struct drm_atomic_state *state;
	struct drm_crtc *crtc = set->crtc;
	int ret = 0;

	state = drm_atomic_state_alloc(crtc->dev);
	if (!state)
		return -ENOMEM;

	state->acquire_ctx = ctx;
	ret = __drm_atomic_helper_set_config(set, state);
	if (ret != 0)
		goto fail;

	ret = handle_conflicting_encoders(state, true);
	if (ret)
		goto fail;

	ret = drm_atomic_commit(state);

fail:
	drm_atomic_state_put(state);
	return ret;
}
EXPORT_SYMBOL(drm_atomic_helper_set_config);

 
int drm_atomic_helper_disable_all(struct drm_device *dev,
				  struct drm_modeset_acquire_ctx *ctx)
{
	struct drm_atomic_state *state;
	struct drm_connector_state *conn_state;
	struct drm_connector *conn;
	struct drm_plane_state *plane_state;
	struct drm_plane *plane;
	struct drm_crtc_state *crtc_state;
	struct drm_crtc *crtc;
	int ret, i;

	state = drm_atomic_state_alloc(dev);
	if (!state)
		return -ENOMEM;

	state->acquire_ctx = ctx;

	drm_for_each_crtc(crtc, dev) {
		crtc_state = drm_atomic_get_crtc_state(state, crtc);
		if (IS_ERR(crtc_state)) {
			ret = PTR_ERR(crtc_state);
			goto free;
		}

		crtc_state->active = false;

		ret = drm_atomic_set_mode_prop_for_crtc(crtc_state, NULL);
		if (ret < 0)
			goto free;

		ret = drm_atomic_add_affected_planes(state, crtc);
		if (ret < 0)
			goto free;

		ret = drm_atomic_add_affected_connectors(state, crtc);
		if (ret < 0)
			goto free;
	}

	for_each_new_connector_in_state(state, conn, conn_state, i) {
		ret = drm_atomic_set_crtc_for_connector(conn_state, NULL);
		if (ret < 0)
			goto free;
	}

	for_each_new_plane_in_state(state, plane, plane_state, i) {
		ret = drm_atomic_set_crtc_for_plane(plane_state, NULL);
		if (ret < 0)
			goto free;

		drm_atomic_set_fb_for_plane(plane_state, NULL);
	}

	ret = drm_atomic_commit(state);
free:
	drm_atomic_state_put(state);
	return ret;
}
EXPORT_SYMBOL(drm_atomic_helper_disable_all);

 
void drm_atomic_helper_shutdown(struct drm_device *dev)
{
	struct drm_modeset_acquire_ctx ctx;
	int ret;

	DRM_MODESET_LOCK_ALL_BEGIN(dev, ctx, 0, ret);

	ret = drm_atomic_helper_disable_all(dev, &ctx);
	if (ret)
		drm_err(dev,
			"Disabling all crtc's during unload failed with %i\n",
			ret);

	DRM_MODESET_LOCK_ALL_END(dev, ctx, ret);
}
EXPORT_SYMBOL(drm_atomic_helper_shutdown);

 
struct drm_atomic_state *
drm_atomic_helper_duplicate_state(struct drm_device *dev,
				  struct drm_modeset_acquire_ctx *ctx)
{
	struct drm_atomic_state *state;
	struct drm_connector *conn;
	struct drm_connector_list_iter conn_iter;
	struct drm_plane *plane;
	struct drm_crtc *crtc;
	int err = 0;

	state = drm_atomic_state_alloc(dev);
	if (!state)
		return ERR_PTR(-ENOMEM);

	state->acquire_ctx = ctx;
	state->duplicated = true;

	drm_for_each_crtc(crtc, dev) {
		struct drm_crtc_state *crtc_state;

		crtc_state = drm_atomic_get_crtc_state(state, crtc);
		if (IS_ERR(crtc_state)) {
			err = PTR_ERR(crtc_state);
			goto free;
		}
	}

	drm_for_each_plane(plane, dev) {
		struct drm_plane_state *plane_state;

		plane_state = drm_atomic_get_plane_state(state, plane);
		if (IS_ERR(plane_state)) {
			err = PTR_ERR(plane_state);
			goto free;
		}
	}

	drm_connector_list_iter_begin(dev, &conn_iter);
	drm_for_each_connector_iter(conn, &conn_iter) {
		struct drm_connector_state *conn_state;

		conn_state = drm_atomic_get_connector_state(state, conn);
		if (IS_ERR(conn_state)) {
			err = PTR_ERR(conn_state);
			drm_connector_list_iter_end(&conn_iter);
			goto free;
		}
	}
	drm_connector_list_iter_end(&conn_iter);

	 
	state->acquire_ctx = NULL;

free:
	if (err < 0) {
		drm_atomic_state_put(state);
		state = ERR_PTR(err);
	}

	return state;
}
EXPORT_SYMBOL(drm_atomic_helper_duplicate_state);

 
struct drm_atomic_state *drm_atomic_helper_suspend(struct drm_device *dev)
{
	struct drm_modeset_acquire_ctx ctx;
	struct drm_atomic_state *state;
	int err;

	 
	state = ERR_PTR(-EINVAL);

	DRM_MODESET_LOCK_ALL_BEGIN(dev, ctx, 0, err);

	state = drm_atomic_helper_duplicate_state(dev, &ctx);
	if (IS_ERR(state))
		goto unlock;

	err = drm_atomic_helper_disable_all(dev, &ctx);
	if (err < 0) {
		drm_atomic_state_put(state);
		state = ERR_PTR(err);
		goto unlock;
	}

unlock:
	DRM_MODESET_LOCK_ALL_END(dev, ctx, err);
	if (err)
		return ERR_PTR(err);

	return state;
}
EXPORT_SYMBOL(drm_atomic_helper_suspend);

 
int drm_atomic_helper_commit_duplicated_state(struct drm_atomic_state *state,
					      struct drm_modeset_acquire_ctx *ctx)
{
	int i, ret;
	struct drm_plane *plane;
	struct drm_plane_state *new_plane_state;
	struct drm_connector *connector;
	struct drm_connector_state *new_conn_state;
	struct drm_crtc *crtc;
	struct drm_crtc_state *new_crtc_state;

	state->acquire_ctx = ctx;

	for_each_new_plane_in_state(state, plane, new_plane_state, i)
		state->planes[i].old_state = plane->state;

	for_each_new_crtc_in_state(state, crtc, new_crtc_state, i)
		state->crtcs[i].old_state = crtc->state;

	for_each_new_connector_in_state(state, connector, new_conn_state, i)
		state->connectors[i].old_state = connector->state;

	ret = drm_atomic_commit(state);

	state->acquire_ctx = NULL;

	return ret;
}
EXPORT_SYMBOL(drm_atomic_helper_commit_duplicated_state);

 
int drm_atomic_helper_resume(struct drm_device *dev,
			     struct drm_atomic_state *state)
{
	struct drm_modeset_acquire_ctx ctx;
	int err;

	drm_mode_config_reset(dev);

	DRM_MODESET_LOCK_ALL_BEGIN(dev, ctx, 0, err);

	err = drm_atomic_helper_commit_duplicated_state(state, &ctx);

	DRM_MODESET_LOCK_ALL_END(dev, ctx, err);
	drm_atomic_state_put(state);

	return err;
}
EXPORT_SYMBOL(drm_atomic_helper_resume);

static int page_flip_common(struct drm_atomic_state *state,
			    struct drm_crtc *crtc,
			    struct drm_framebuffer *fb,
			    struct drm_pending_vblank_event *event,
			    uint32_t flags)
{
	struct drm_plane *plane = crtc->primary;
	struct drm_plane_state *plane_state;
	struct drm_crtc_state *crtc_state;
	int ret = 0;

	crtc_state = drm_atomic_get_crtc_state(state, crtc);
	if (IS_ERR(crtc_state))
		return PTR_ERR(crtc_state);

	crtc_state->event = event;
	crtc_state->async_flip = flags & DRM_MODE_PAGE_FLIP_ASYNC;

	plane_state = drm_atomic_get_plane_state(state, plane);
	if (IS_ERR(plane_state))
		return PTR_ERR(plane_state);

	ret = drm_atomic_set_crtc_for_plane(plane_state, crtc);
	if (ret != 0)
		return ret;
	drm_atomic_set_fb_for_plane(plane_state, fb);

	 
	state->allow_modeset = false;
	if (!crtc_state->active) {
		drm_dbg_atomic(crtc->dev,
			       "[CRTC:%d:%s] disabled, rejecting legacy flip\n",
			       crtc->base.id, crtc->name);
		return -EINVAL;
	}

	return ret;
}

 
int drm_atomic_helper_page_flip(struct drm_crtc *crtc,
				struct drm_framebuffer *fb,
				struct drm_pending_vblank_event *event,
				uint32_t flags,
				struct drm_modeset_acquire_ctx *ctx)
{
	struct drm_plane *plane = crtc->primary;
	struct drm_atomic_state *state;
	int ret = 0;

	state = drm_atomic_state_alloc(plane->dev);
	if (!state)
		return -ENOMEM;

	state->acquire_ctx = ctx;

	ret = page_flip_common(state, crtc, fb, event, flags);
	if (ret != 0)
		goto fail;

	ret = drm_atomic_nonblocking_commit(state);
fail:
	drm_atomic_state_put(state);
	return ret;
}
EXPORT_SYMBOL(drm_atomic_helper_page_flip);

 
int drm_atomic_helper_page_flip_target(struct drm_crtc *crtc,
				       struct drm_framebuffer *fb,
				       struct drm_pending_vblank_event *event,
				       uint32_t flags,
				       uint32_t target,
				       struct drm_modeset_acquire_ctx *ctx)
{
	struct drm_plane *plane = crtc->primary;
	struct drm_atomic_state *state;
	struct drm_crtc_state *crtc_state;
	int ret = 0;

	state = drm_atomic_state_alloc(plane->dev);
	if (!state)
		return -ENOMEM;

	state->acquire_ctx = ctx;

	ret = page_flip_common(state, crtc, fb, event, flags);
	if (ret != 0)
		goto fail;

	crtc_state = drm_atomic_get_new_crtc_state(state, crtc);
	if (WARN_ON(!crtc_state)) {
		ret = -EINVAL;
		goto fail;
	}
	crtc_state->target_vblank = target;

	ret = drm_atomic_nonblocking_commit(state);
fail:
	drm_atomic_state_put(state);
	return ret;
}
EXPORT_SYMBOL(drm_atomic_helper_page_flip_target);

 
u32 *
drm_atomic_helper_bridge_propagate_bus_fmt(struct drm_bridge *bridge,
					struct drm_bridge_state *bridge_state,
					struct drm_crtc_state *crtc_state,
					struct drm_connector_state *conn_state,
					u32 output_fmt,
					unsigned int *num_input_fmts)
{
	u32 *input_fmts;

	input_fmts = kzalloc(sizeof(*input_fmts), GFP_KERNEL);
	if (!input_fmts) {
		*num_input_fmts = 0;
		return NULL;
	}

	*num_input_fmts = 1;
	input_fmts[0] = output_fmt;
	return input_fmts;
}
EXPORT_SYMBOL(drm_atomic_helper_bridge_propagate_bus_fmt);
