
 

#include <drm/drm_atomic_helper.h>
#include <drm/drm_atomic.h>
#include <drm/drm_bridge.h>
#include <drm/drm_bridge_connector.h>
#include <drm/drm_crtc.h>

#include "msm_drv.h"
#include "msm_kms.h"
#include "dp_drm.h"

 
static enum drm_connector_status dp_bridge_detect(struct drm_bridge *bridge)
{
	struct msm_dp *dp;

	dp = to_dp_bridge(bridge)->dp_display;

	drm_dbg_dp(dp->drm_dev, "is_connected = %s\n",
		(dp->is_connected) ? "true" : "false");

	return (dp->is_connected) ? connector_status_connected :
					connector_status_disconnected;
}

static int dp_bridge_atomic_check(struct drm_bridge *bridge,
			    struct drm_bridge_state *bridge_state,
			    struct drm_crtc_state *crtc_state,
			    struct drm_connector_state *conn_state)
{
	struct msm_dp *dp;

	dp = to_dp_bridge(bridge)->dp_display;

	drm_dbg_dp(dp->drm_dev, "is_connected = %s\n",
		(dp->is_connected) ? "true" : "false");

	 
	if (bridge->ops & DRM_BRIDGE_OP_HPD)
		return (dp->is_connected) ? 0 : -ENOTCONN;

	return 0;
}


 
static int dp_bridge_get_modes(struct drm_bridge *bridge, struct drm_connector *connector)
{
	int rc = 0;
	struct msm_dp *dp;

	if (!connector)
		return 0;

	dp = to_dp_bridge(bridge)->dp_display;

	 
	if (dp->is_connected) {
		rc = dp_display_get_modes(dp);
		if (rc <= 0) {
			DRM_ERROR("failed to get DP sink modes, rc=%d\n", rc);
			return rc;
		}
	} else {
		drm_dbg_dp(connector->dev, "No sink connected\n");
	}
	return rc;
}

static const struct drm_bridge_funcs dp_bridge_ops = {
	.atomic_duplicate_state = drm_atomic_helper_bridge_duplicate_state,
	.atomic_destroy_state   = drm_atomic_helper_bridge_destroy_state,
	.atomic_reset           = drm_atomic_helper_bridge_reset,
	.atomic_enable          = dp_bridge_atomic_enable,
	.atomic_disable         = dp_bridge_atomic_disable,
	.atomic_post_disable    = dp_bridge_atomic_post_disable,
	.mode_set     = dp_bridge_mode_set,
	.mode_valid   = dp_bridge_mode_valid,
	.get_modes    = dp_bridge_get_modes,
	.detect       = dp_bridge_detect,
	.atomic_check = dp_bridge_atomic_check,
	.hpd_enable   = dp_bridge_hpd_enable,
	.hpd_disable  = dp_bridge_hpd_disable,
	.hpd_notify   = dp_bridge_hpd_notify,
};

static int edp_bridge_atomic_check(struct drm_bridge *drm_bridge,
				   struct drm_bridge_state *bridge_state,
				   struct drm_crtc_state *crtc_state,
				   struct drm_connector_state *conn_state)
{
	struct msm_dp *dp = to_dp_bridge(drm_bridge)->dp_display;

	if (WARN_ON(!conn_state))
		return -ENODEV;

	conn_state->self_refresh_aware = dp->psr_supported;

	if (!conn_state->crtc || !crtc_state)
		return 0;

	if (crtc_state->self_refresh_active && !dp->psr_supported)
		return -EINVAL;

	return 0;
}

static void edp_bridge_atomic_enable(struct drm_bridge *drm_bridge,
				     struct drm_bridge_state *old_bridge_state)
{
	struct drm_atomic_state *atomic_state = old_bridge_state->base.state;
	struct drm_crtc *crtc;
	struct drm_crtc_state *old_crtc_state;
	struct msm_dp_bridge *dp_bridge = to_dp_bridge(drm_bridge);
	struct msm_dp *dp = dp_bridge->dp_display;

	 
	crtc = drm_atomic_get_new_crtc_for_encoder(atomic_state,
						   drm_bridge->encoder);
	if (!crtc)
		return;

	old_crtc_state = drm_atomic_get_old_crtc_state(atomic_state, crtc);

	if (old_crtc_state && old_crtc_state->self_refresh_active) {
		dp_display_set_psr(dp, false);
		return;
	}

	dp_bridge_atomic_enable(drm_bridge, old_bridge_state);
}

static void edp_bridge_atomic_disable(struct drm_bridge *drm_bridge,
				      struct drm_bridge_state *old_bridge_state)
{
	struct drm_atomic_state *atomic_state = old_bridge_state->base.state;
	struct drm_crtc *crtc;
	struct drm_crtc_state *new_crtc_state = NULL, *old_crtc_state = NULL;
	struct msm_dp_bridge *dp_bridge = to_dp_bridge(drm_bridge);
	struct msm_dp *dp = dp_bridge->dp_display;

	crtc = drm_atomic_get_old_crtc_for_encoder(atomic_state,
						   drm_bridge->encoder);
	if (!crtc)
		goto out;

	new_crtc_state = drm_atomic_get_new_crtc_state(atomic_state, crtc);
	if (!new_crtc_state)
		goto out;

	old_crtc_state = drm_atomic_get_old_crtc_state(atomic_state, crtc);
	if (!old_crtc_state)
		goto out;

	 
	if (new_crtc_state->self_refresh_active) {
		dp_display_set_psr(dp, true);
		return;
	} else if (old_crtc_state->self_refresh_active) {
		dp_display_set_psr(dp, false);
		return;
	}

out:
	dp_bridge_atomic_disable(drm_bridge, old_bridge_state);
}

static void edp_bridge_atomic_post_disable(struct drm_bridge *drm_bridge,
				struct drm_bridge_state *old_bridge_state)
{
	struct drm_atomic_state *atomic_state = old_bridge_state->base.state;
	struct drm_crtc *crtc;
	struct drm_crtc_state *new_crtc_state = NULL;

	crtc = drm_atomic_get_old_crtc_for_encoder(atomic_state,
						   drm_bridge->encoder);
	if (!crtc)
		return;

	new_crtc_state = drm_atomic_get_new_crtc_state(atomic_state, crtc);
	if (!new_crtc_state)
		return;

	 
	if (new_crtc_state->self_refresh_active)
		return;

	dp_bridge_atomic_post_disable(drm_bridge, old_bridge_state);
}

 
static enum drm_mode_status edp_bridge_mode_valid(struct drm_bridge *bridge,
					  const struct drm_display_info *info,
					  const struct drm_display_mode *mode)
{
	struct msm_dp *dp;
	int mode_pclk_khz = mode->clock;

	dp = to_dp_bridge(bridge)->dp_display;

	if (!dp || !mode_pclk_khz || !dp->connector) {
		DRM_ERROR("invalid params\n");
		return -EINVAL;
	}

	if (mode->clock > DP_MAX_PIXEL_CLK_KHZ)
		return MODE_CLOCK_HIGH;

	 
	return MODE_OK;
}

static const struct drm_bridge_funcs edp_bridge_ops = {
	.atomic_enable = edp_bridge_atomic_enable,
	.atomic_disable = edp_bridge_atomic_disable,
	.atomic_post_disable = edp_bridge_atomic_post_disable,
	.mode_set = dp_bridge_mode_set,
	.mode_valid = edp_bridge_mode_valid,
	.atomic_reset = drm_atomic_helper_bridge_reset,
	.atomic_duplicate_state = drm_atomic_helper_bridge_duplicate_state,
	.atomic_destroy_state = drm_atomic_helper_bridge_destroy_state,
	.atomic_check = edp_bridge_atomic_check,
};

struct drm_bridge *dp_bridge_init(struct msm_dp *dp_display, struct drm_device *dev,
			struct drm_encoder *encoder)
{
	int rc;
	struct msm_dp_bridge *dp_bridge;
	struct drm_bridge *bridge;

	dp_bridge = devm_kzalloc(dev->dev, sizeof(*dp_bridge), GFP_KERNEL);
	if (!dp_bridge)
		return ERR_PTR(-ENOMEM);

	dp_bridge->dp_display = dp_display;

	bridge = &dp_bridge->bridge;
	bridge->funcs = dp_display->is_edp ? &edp_bridge_ops : &dp_bridge_ops;
	bridge->type = dp_display->connector_type;

	 
	if (!dp_display->is_edp) {
		bridge->ops =
			DRM_BRIDGE_OP_DETECT |
			DRM_BRIDGE_OP_HPD |
			DRM_BRIDGE_OP_MODES;
	}

	drm_bridge_add(bridge);

	rc = drm_bridge_attach(encoder, bridge, NULL, DRM_BRIDGE_ATTACH_NO_CONNECTOR);
	if (rc) {
		DRM_ERROR("failed to attach bridge, rc=%d\n", rc);
		drm_bridge_remove(bridge);

		return ERR_PTR(rc);
	}

	if (dp_display->next_bridge) {
		rc = drm_bridge_attach(encoder,
					dp_display->next_bridge, bridge,
					DRM_BRIDGE_ATTACH_NO_CONNECTOR);
		if (rc < 0) {
			DRM_ERROR("failed to attach panel bridge: %d\n", rc);
			drm_bridge_remove(bridge);
			return ERR_PTR(rc);
		}
	}

	return bridge;
}

 
struct drm_connector *dp_drm_connector_init(struct msm_dp *dp_display, struct drm_encoder *encoder)
{
	struct drm_connector *connector = NULL;

	connector = drm_bridge_connector_init(dp_display->drm_dev, encoder);
	if (IS_ERR(connector))
		return connector;

	drm_connector_attach_encoder(connector, encoder);

	return connector;
}
