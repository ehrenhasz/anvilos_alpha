
 

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>

#include <drm/drm_atomic_state_helper.h>
#include <drm/drm_bridge.h>
#include <drm/drm_bridge_connector.h>
#include <drm/drm_connector.h>
#include <drm/drm_device.h>
#include <drm/drm_edid.h>
#include <drm/drm_modeset_helper_vtables.h>
#include <drm/drm_probe_helper.h>

 

 
struct drm_bridge_connector {
	 
	struct drm_connector base;
	 
	struct drm_encoder *encoder;
	 
	struct drm_bridge *bridge_edid;
	 
	struct drm_bridge *bridge_hpd;
	 
	struct drm_bridge *bridge_detect;
	 
	struct drm_bridge *bridge_modes;
};

#define to_drm_bridge_connector(x) \
	container_of(x, struct drm_bridge_connector, base)

 

static void drm_bridge_connector_hpd_notify(struct drm_connector *connector,
					    enum drm_connector_status status)
{
	struct drm_bridge_connector *bridge_connector =
		to_drm_bridge_connector(connector);
	struct drm_bridge *bridge;

	 
	drm_for_each_bridge_in_chain(bridge_connector->encoder, bridge) {
		if (bridge->funcs->hpd_notify)
			bridge->funcs->hpd_notify(bridge, status);
	}
}

static void drm_bridge_connector_hpd_cb(void *cb_data,
					enum drm_connector_status status)
{
	struct drm_bridge_connector *drm_bridge_connector = cb_data;
	struct drm_connector *connector = &drm_bridge_connector->base;
	struct drm_device *dev = connector->dev;
	enum drm_connector_status old_status;

	mutex_lock(&dev->mode_config.mutex);
	old_status = connector->status;
	connector->status = status;
	mutex_unlock(&dev->mode_config.mutex);

	if (old_status == status)
		return;

	drm_bridge_connector_hpd_notify(connector, status);

	drm_kms_helper_connector_hotplug_event(connector);
}

static void drm_bridge_connector_enable_hpd(struct drm_connector *connector)
{
	struct drm_bridge_connector *bridge_connector =
		to_drm_bridge_connector(connector);
	struct drm_bridge *hpd = bridge_connector->bridge_hpd;

	if (hpd)
		drm_bridge_hpd_enable(hpd, drm_bridge_connector_hpd_cb,
				      bridge_connector);
}

static void drm_bridge_connector_disable_hpd(struct drm_connector *connector)
{
	struct drm_bridge_connector *bridge_connector =
		to_drm_bridge_connector(connector);
	struct drm_bridge *hpd = bridge_connector->bridge_hpd;

	if (hpd)
		drm_bridge_hpd_disable(hpd);
}

 

static enum drm_connector_status
drm_bridge_connector_detect(struct drm_connector *connector, bool force)
{
	struct drm_bridge_connector *bridge_connector =
		to_drm_bridge_connector(connector);
	struct drm_bridge *detect = bridge_connector->bridge_detect;
	enum drm_connector_status status;

	if (detect) {
		status = detect->funcs->detect(detect);

		drm_bridge_connector_hpd_notify(connector, status);
	} else {
		switch (connector->connector_type) {
		case DRM_MODE_CONNECTOR_DPI:
		case DRM_MODE_CONNECTOR_LVDS:
		case DRM_MODE_CONNECTOR_DSI:
		case DRM_MODE_CONNECTOR_eDP:
			status = connector_status_connected;
			break;
		default:
			status = connector_status_unknown;
			break;
		}
	}

	return status;
}

static void drm_bridge_connector_destroy(struct drm_connector *connector)
{
	struct drm_bridge_connector *bridge_connector =
		to_drm_bridge_connector(connector);

	if (bridge_connector->bridge_hpd) {
		struct drm_bridge *hpd = bridge_connector->bridge_hpd;

		drm_bridge_hpd_disable(hpd);
	}

	drm_connector_unregister(connector);
	drm_connector_cleanup(connector);

	kfree(bridge_connector);
}

static void drm_bridge_connector_debugfs_init(struct drm_connector *connector,
					      struct dentry *root)
{
	struct drm_bridge_connector *bridge_connector =
		to_drm_bridge_connector(connector);
	struct drm_encoder *encoder = bridge_connector->encoder;
	struct drm_bridge *bridge;

	list_for_each_entry(bridge, &encoder->bridge_chain, chain_node) {
		if (bridge->funcs->debugfs_init)
			bridge->funcs->debugfs_init(bridge, root);
	}
}

static const struct drm_connector_funcs drm_bridge_connector_funcs = {
	.reset = drm_atomic_helper_connector_reset,
	.detect = drm_bridge_connector_detect,
	.fill_modes = drm_helper_probe_single_connector_modes,
	.destroy = drm_bridge_connector_destroy,
	.atomic_duplicate_state = drm_atomic_helper_connector_duplicate_state,
	.atomic_destroy_state = drm_atomic_helper_connector_destroy_state,
	.debugfs_init = drm_bridge_connector_debugfs_init,
};

 

static int drm_bridge_connector_get_modes_edid(struct drm_connector *connector,
					       struct drm_bridge *bridge)
{
	enum drm_connector_status status;
	struct edid *edid;
	int n;

	status = drm_bridge_connector_detect(connector, false);
	if (status != connector_status_connected)
		goto no_edid;

	edid = bridge->funcs->get_edid(bridge, connector);
	if (!drm_edid_is_valid(edid)) {
		kfree(edid);
		goto no_edid;
	}

	drm_connector_update_edid_property(connector, edid);
	n = drm_add_edid_modes(connector, edid);

	kfree(edid);
	return n;

no_edid:
	drm_connector_update_edid_property(connector, NULL);
	return 0;
}

static int drm_bridge_connector_get_modes(struct drm_connector *connector)
{
	struct drm_bridge_connector *bridge_connector =
		to_drm_bridge_connector(connector);
	struct drm_bridge *bridge;

	 
	bridge = bridge_connector->bridge_edid;
	if (bridge)
		return drm_bridge_connector_get_modes_edid(connector, bridge);

	 
	bridge = bridge_connector->bridge_modes;
	if (bridge)
		return bridge->funcs->get_modes(bridge, connector);

	 
	return 0;
}

static const struct drm_connector_helper_funcs drm_bridge_connector_helper_funcs = {
	.get_modes = drm_bridge_connector_get_modes,
	 
	.enable_hpd = drm_bridge_connector_enable_hpd,
	.disable_hpd = drm_bridge_connector_disable_hpd,
};

 

 
struct drm_connector *drm_bridge_connector_init(struct drm_device *drm,
						struct drm_encoder *encoder)
{
	struct drm_bridge_connector *bridge_connector;
	struct drm_connector *connector;
	struct i2c_adapter *ddc = NULL;
	struct drm_bridge *bridge, *panel_bridge = NULL;
	int connector_type;
	int ret;

	bridge_connector = kzalloc(sizeof(*bridge_connector), GFP_KERNEL);
	if (!bridge_connector)
		return ERR_PTR(-ENOMEM);

	bridge_connector->encoder = encoder;

	 
	connector = &bridge_connector->base;
	connector->interlace_allowed = true;

	 
	connector_type = DRM_MODE_CONNECTOR_Unknown;
	drm_for_each_bridge_in_chain(encoder, bridge) {
		if (!bridge->interlace_allowed)
			connector->interlace_allowed = false;

		if (bridge->ops & DRM_BRIDGE_OP_EDID)
			bridge_connector->bridge_edid = bridge;
		if (bridge->ops & DRM_BRIDGE_OP_HPD)
			bridge_connector->bridge_hpd = bridge;
		if (bridge->ops & DRM_BRIDGE_OP_DETECT)
			bridge_connector->bridge_detect = bridge;
		if (bridge->ops & DRM_BRIDGE_OP_MODES)
			bridge_connector->bridge_modes = bridge;

		if (!drm_bridge_get_next_bridge(bridge))
			connector_type = bridge->type;

		if (bridge->ddc)
			ddc = bridge->ddc;

		if (drm_bridge_is_panel(bridge))
			panel_bridge = bridge;
	}

	if (connector_type == DRM_MODE_CONNECTOR_Unknown) {
		kfree(bridge_connector);
		return ERR_PTR(-EINVAL);
	}

	ret = drm_connector_init_with_ddc(drm, connector,
					  &drm_bridge_connector_funcs,
					  connector_type, ddc);
	if (ret) {
		kfree(bridge_connector);
		return ERR_PTR(ret);
	}

	drm_connector_helper_add(connector, &drm_bridge_connector_helper_funcs);

	if (bridge_connector->bridge_hpd)
		connector->polled = DRM_CONNECTOR_POLL_HPD;
	else if (bridge_connector->bridge_detect)
		connector->polled = DRM_CONNECTOR_POLL_CONNECT
				  | DRM_CONNECTOR_POLL_DISCONNECT;

	if (panel_bridge)
		drm_panel_bridge_set_orientation(connector, panel_bridge);

	return connector;
}
EXPORT_SYMBOL_GPL(drm_bridge_connector_init);
