 

#include <linux/i2c.h>
#include <linux/slab.h>

#include <drm/drm_atomic_helper.h>
#include <drm/drm_edid.h>

#include "i915_drv.h"
#include "intel_backlight.h"
#include "intel_connector.h"
#include "intel_display_debugfs.h"
#include "intel_display_types.h"
#include "intel_hdcp.h"
#include "intel_panel.h"

int intel_connector_init(struct intel_connector *connector)
{
	struct intel_digital_connector_state *conn_state;

	 
	conn_state = kzalloc(sizeof(*conn_state), GFP_KERNEL);
	if (!conn_state)
		return -ENOMEM;

	__drm_atomic_helper_connector_reset(&connector->base,
					    &conn_state->base);

	intel_panel_init_alloc(connector);

	return 0;
}

struct intel_connector *intel_connector_alloc(void)
{
	struct intel_connector *connector;

	connector = kzalloc(sizeof(*connector), GFP_KERNEL);
	if (!connector)
		return NULL;

	if (intel_connector_init(connector) < 0) {
		kfree(connector);
		return NULL;
	}

	return connector;
}

 
void intel_connector_free(struct intel_connector *connector)
{
	kfree(to_intel_digital_connector_state(connector->base.state));
	kfree(connector);
}

 
void intel_connector_destroy(struct drm_connector *connector)
{
	struct intel_connector *intel_connector = to_intel_connector(connector);

	drm_edid_free(intel_connector->detect_edid);

	intel_hdcp_cleanup(intel_connector);

	intel_panel_fini(intel_connector);

	drm_connector_cleanup(connector);

	if (intel_connector->port)
		drm_dp_mst_put_port_malloc(intel_connector->port);

	kfree(connector);
}

int intel_connector_register(struct drm_connector *connector)
{
	struct intel_connector *intel_connector = to_intel_connector(connector);
	int ret;

	ret = intel_backlight_device_register(intel_connector);
	if (ret)
		goto err;

	if (i915_inject_probe_failure(to_i915(connector->dev))) {
		ret = -EFAULT;
		goto err_backlight;
	}

	intel_connector_debugfs_add(intel_connector);

	return 0;

err_backlight:
	intel_backlight_device_unregister(intel_connector);
err:
	return ret;
}

void intel_connector_unregister(struct drm_connector *connector)
{
	struct intel_connector *intel_connector = to_intel_connector(connector);

	intel_backlight_device_unregister(intel_connector);
}

void intel_connector_attach_encoder(struct intel_connector *connector,
				    struct intel_encoder *encoder)
{
	connector->encoder = encoder;
	drm_connector_attach_encoder(&connector->base, &encoder->base);
}

 
bool intel_connector_get_hw_state(struct intel_connector *connector)
{
	enum pipe pipe = 0;
	struct intel_encoder *encoder = intel_attached_encoder(connector);

	return encoder->get_hw_state(encoder, &pipe);
}

enum pipe intel_connector_get_pipe(struct intel_connector *connector)
{
	struct drm_device *dev = connector->base.dev;

	drm_WARN_ON(dev,
		    !drm_modeset_is_locked(&dev->mode_config.connection_mutex));

	if (!connector->base.state->crtc)
		return INVALID_PIPE;

	return to_intel_crtc(connector->base.state->crtc)->pipe;
}

 
int intel_connector_update_modes(struct drm_connector *connector,
				 const struct drm_edid *drm_edid)
{
	int ret;

	drm_edid_connector_update(connector, drm_edid);
	ret = drm_edid_connector_add_modes(connector);

	return ret;
}

 
int intel_ddc_get_modes(struct drm_connector *connector,
			struct i2c_adapter *adapter)
{
	const struct drm_edid *drm_edid;
	int ret;

	drm_edid = drm_edid_read_ddc(connector, adapter);
	if (!drm_edid)
		return 0;

	ret = intel_connector_update_modes(connector, drm_edid);
	drm_edid_free(drm_edid);

	return ret;
}

static const struct drm_prop_enum_list force_audio_names[] = {
	{ HDMI_AUDIO_OFF_DVI, "force-dvi" },
	{ HDMI_AUDIO_OFF, "off" },
	{ HDMI_AUDIO_AUTO, "auto" },
	{ HDMI_AUDIO_ON, "on" },
};

void
intel_attach_force_audio_property(struct drm_connector *connector)
{
	struct drm_device *dev = connector->dev;
	struct drm_i915_private *dev_priv = to_i915(dev);
	struct drm_property *prop;

	prop = dev_priv->display.properties.force_audio;
	if (prop == NULL) {
		prop = drm_property_create_enum(dev, 0,
					   "audio",
					   force_audio_names,
					   ARRAY_SIZE(force_audio_names));
		if (prop == NULL)
			return;

		dev_priv->display.properties.force_audio = prop;
	}
	drm_object_attach_property(&connector->base, prop, 0);
}

static const struct drm_prop_enum_list broadcast_rgb_names[] = {
	{ INTEL_BROADCAST_RGB_AUTO, "Automatic" },
	{ INTEL_BROADCAST_RGB_FULL, "Full" },
	{ INTEL_BROADCAST_RGB_LIMITED, "Limited 16:235" },
};

void
intel_attach_broadcast_rgb_property(struct drm_connector *connector)
{
	struct drm_device *dev = connector->dev;
	struct drm_i915_private *dev_priv = to_i915(dev);
	struct drm_property *prop;

	prop = dev_priv->display.properties.broadcast_rgb;
	if (prop == NULL) {
		prop = drm_property_create_enum(dev, DRM_MODE_PROP_ENUM,
					   "Broadcast RGB",
					   broadcast_rgb_names,
					   ARRAY_SIZE(broadcast_rgb_names));
		if (prop == NULL)
			return;

		dev_priv->display.properties.broadcast_rgb = prop;
	}

	drm_object_attach_property(&connector->base, prop, 0);
}

void
intel_attach_aspect_ratio_property(struct drm_connector *connector)
{
	if (!drm_mode_create_aspect_ratio_property(connector->dev))
		drm_object_attach_property(&connector->base,
			connector->dev->mode_config.aspect_ratio_property,
			DRM_MODE_PICTURE_ASPECT_NONE);
}

void
intel_attach_hdmi_colorspace_property(struct drm_connector *connector)
{
	if (!drm_mode_create_hdmi_colorspace_property(connector, 0))
		drm_connector_attach_colorspace_property(connector);
}

void
intel_attach_dp_colorspace_property(struct drm_connector *connector)
{
	if (!drm_mode_create_dp_colorspace_property(connector, 0))
		drm_connector_attach_colorspace_property(connector);
}

void
intel_attach_scaling_mode_property(struct drm_connector *connector)
{
	struct drm_i915_private *i915 = to_i915(connector->dev);
	u32 scaling_modes;

	scaling_modes = BIT(DRM_MODE_SCALE_ASPECT) |
		BIT(DRM_MODE_SCALE_FULLSCREEN);

	 
	if (!HAS_GMCH(i915) || connector->connector_type == DRM_MODE_CONNECTOR_LVDS)
		scaling_modes |= BIT(DRM_MODE_SCALE_CENTER);

	drm_connector_attach_scaling_mode_property(connector, scaling_modes);

	connector->state->scaling_mode = DRM_MODE_SCALE_ASPECT;
}
