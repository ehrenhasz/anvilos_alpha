 

#include <linux/export.h>

#include <drm/drm_bridge.h>
#include <drm/drm_device.h>
#include <drm/drm_drv.h>
#include <drm/drm_encoder.h>
#include <drm/drm_managed.h>
#include <drm/drm_print.h>

#include "drm_crtc_internal.h"

 
static const struct drm_prop_enum_list drm_encoder_enum_list[] = {
	{ DRM_MODE_ENCODER_NONE, "None" },
	{ DRM_MODE_ENCODER_DAC, "DAC" },
	{ DRM_MODE_ENCODER_TMDS, "TMDS" },
	{ DRM_MODE_ENCODER_LVDS, "LVDS" },
	{ DRM_MODE_ENCODER_TVDAC, "TV" },
	{ DRM_MODE_ENCODER_VIRTUAL, "Virtual" },
	{ DRM_MODE_ENCODER_DSI, "DSI" },
	{ DRM_MODE_ENCODER_DPMST, "DP MST" },
	{ DRM_MODE_ENCODER_DPI, "DPI" },
};

int drm_encoder_register_all(struct drm_device *dev)
{
	struct drm_encoder *encoder;
	int ret = 0;

	drm_for_each_encoder(encoder, dev) {
		if (encoder->funcs && encoder->funcs->late_register)
			ret = encoder->funcs->late_register(encoder);
		if (ret)
			return ret;
	}

	return 0;
}

void drm_encoder_unregister_all(struct drm_device *dev)
{
	struct drm_encoder *encoder;

	drm_for_each_encoder(encoder, dev) {
		if (encoder->funcs && encoder->funcs->early_unregister)
			encoder->funcs->early_unregister(encoder);
	}
}

__printf(5, 0)
static int __drm_encoder_init(struct drm_device *dev,
			      struct drm_encoder *encoder,
			      const struct drm_encoder_funcs *funcs,
			      int encoder_type, const char *name, va_list ap)
{
	int ret;

	 
	if (WARN_ON(dev->mode_config.num_encoder >= 32))
		return -EINVAL;

	ret = drm_mode_object_add(dev, &encoder->base, DRM_MODE_OBJECT_ENCODER);
	if (ret)
		return ret;

	encoder->dev = dev;
	encoder->encoder_type = encoder_type;
	encoder->funcs = funcs;
	if (name) {
		encoder->name = kvasprintf(GFP_KERNEL, name, ap);
	} else {
		encoder->name = kasprintf(GFP_KERNEL, "%s-%d",
					  drm_encoder_enum_list[encoder_type].name,
					  encoder->base.id);
	}
	if (!encoder->name) {
		ret = -ENOMEM;
		goto out_put;
	}

	INIT_LIST_HEAD(&encoder->bridge_chain);
	list_add_tail(&encoder->head, &dev->mode_config.encoder_list);
	encoder->index = dev->mode_config.num_encoder++;

out_put:
	if (ret)
		drm_mode_object_unregister(dev, &encoder->base);

	return ret;
}

 
int drm_encoder_init(struct drm_device *dev,
		     struct drm_encoder *encoder,
		     const struct drm_encoder_funcs *funcs,
		     int encoder_type, const char *name, ...)
{
	va_list ap;
	int ret;

	WARN_ON(!funcs->destroy);

	va_start(ap, name);
	ret = __drm_encoder_init(dev, encoder, funcs, encoder_type, name, ap);
	va_end(ap);

	return ret;
}
EXPORT_SYMBOL(drm_encoder_init);

 
void drm_encoder_cleanup(struct drm_encoder *encoder)
{
	struct drm_device *dev = encoder->dev;
	struct drm_bridge *bridge, *next;

	 

	list_for_each_entry_safe(bridge, next, &encoder->bridge_chain,
				 chain_node)
		drm_bridge_detach(bridge);

	drm_mode_object_unregister(dev, &encoder->base);
	kfree(encoder->name);
	list_del(&encoder->head);
	dev->mode_config.num_encoder--;

	memset(encoder, 0, sizeof(*encoder));
}
EXPORT_SYMBOL(drm_encoder_cleanup);

static void drmm_encoder_alloc_release(struct drm_device *dev, void *ptr)
{
	struct drm_encoder *encoder = ptr;

	if (WARN_ON(!encoder->dev))
		return;

	drm_encoder_cleanup(encoder);
}

__printf(5, 0)
static int __drmm_encoder_init(struct drm_device *dev,
			       struct drm_encoder *encoder,
			       const struct drm_encoder_funcs *funcs,
			       int encoder_type,
			       const char *name,
			       va_list args)
{
	int ret;

	if (drm_WARN_ON(dev, funcs && funcs->destroy))
		return -EINVAL;

	ret = __drm_encoder_init(dev, encoder, funcs, encoder_type, name, args);
	if (ret)
		return ret;

	ret = drmm_add_action_or_reset(dev, drmm_encoder_alloc_release, encoder);
	if (ret)
		return ret;

	return 0;
}

void *__drmm_encoder_alloc(struct drm_device *dev, size_t size, size_t offset,
			   const struct drm_encoder_funcs *funcs,
			   int encoder_type, const char *name, ...)
{
	void *container;
	struct drm_encoder *encoder;
	va_list ap;
	int ret;

	container = drmm_kzalloc(dev, size, GFP_KERNEL);
	if (!container)
		return ERR_PTR(-ENOMEM);

	encoder = container + offset;

	va_start(ap, name);
	ret = __drmm_encoder_init(dev, encoder, funcs, encoder_type, name, ap);
	va_end(ap);
	if (ret)
		return ERR_PTR(ret);

	return container;
}
EXPORT_SYMBOL(__drmm_encoder_alloc);

 
int drmm_encoder_init(struct drm_device *dev, struct drm_encoder *encoder,
		      const struct drm_encoder_funcs *funcs,
		      int encoder_type, const char *name, ...)
{
	va_list ap;
	int ret;

	va_start(ap, name);
	ret = __drmm_encoder_init(dev, encoder, funcs, encoder_type, name, ap);
	va_end(ap);
	if (ret)
		return ret;

	return 0;
}
EXPORT_SYMBOL(drmm_encoder_init);

static struct drm_crtc *drm_encoder_get_crtc(struct drm_encoder *encoder)
{
	struct drm_connector *connector;
	struct drm_device *dev = encoder->dev;
	bool uses_atomic = false;
	struct drm_connector_list_iter conn_iter;

	 
	drm_connector_list_iter_begin(dev, &conn_iter);
	drm_for_each_connector_iter(connector, &conn_iter) {
		if (!connector->state)
			continue;

		uses_atomic = true;

		if (connector->state->best_encoder != encoder)
			continue;

		drm_connector_list_iter_end(&conn_iter);
		return connector->state->crtc;
	}
	drm_connector_list_iter_end(&conn_iter);

	 
	if (uses_atomic)
		return NULL;

	return encoder->crtc;
}

int drm_mode_getencoder(struct drm_device *dev, void *data,
			struct drm_file *file_priv)
{
	struct drm_mode_get_encoder *enc_resp = data;
	struct drm_encoder *encoder;
	struct drm_crtc *crtc;

	if (!drm_core_check_feature(dev, DRIVER_MODESET))
		return -EOPNOTSUPP;

	encoder = drm_encoder_find(dev, file_priv, enc_resp->encoder_id);
	if (!encoder)
		return -ENOENT;

	drm_modeset_lock(&dev->mode_config.connection_mutex, NULL);
	crtc = drm_encoder_get_crtc(encoder);
	if (crtc && drm_lease_held(file_priv, crtc->base.id))
		enc_resp->crtc_id = crtc->base.id;
	else
		enc_resp->crtc_id = 0;
	drm_modeset_unlock(&dev->mode_config.connection_mutex);

	enc_resp->encoder_type = encoder->encoder_type;
	enc_resp->encoder_id = encoder->base.id;
	enc_resp->possible_crtcs = drm_lease_filter_crtcs(file_priv,
							  encoder->possible_crtcs);
	enc_resp->possible_clones = encoder->possible_clones;

	return 0;
}
