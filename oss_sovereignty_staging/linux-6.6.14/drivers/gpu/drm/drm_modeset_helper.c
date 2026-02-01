 

#include <drm/drm_atomic_helper.h>
#include <drm/drm_fb_helper.h>
#include <drm/drm_fourcc.h>
#include <drm/drm_framebuffer.h>
#include <drm/drm_modeset_helper.h>
#include <drm/drm_plane_helper.h>
#include <drm/drm_print.h>
#include <drm/drm_probe_helper.h>

 

 
void drm_helper_move_panel_connectors_to_head(struct drm_device *dev)
{
	struct drm_connector *connector, *tmp;
	struct list_head panel_list;

	INIT_LIST_HEAD(&panel_list);

	spin_lock_irq(&dev->mode_config.connector_list_lock);
	list_for_each_entry_safe(connector, tmp,
				 &dev->mode_config.connector_list, head) {
		if (connector->connector_type == DRM_MODE_CONNECTOR_LVDS ||
		    connector->connector_type == DRM_MODE_CONNECTOR_eDP ||
		    connector->connector_type == DRM_MODE_CONNECTOR_DSI)
			list_move_tail(&connector->head, &panel_list);
	}

	list_splice(&panel_list, &dev->mode_config.connector_list);
	spin_unlock_irq(&dev->mode_config.connector_list_lock);
}
EXPORT_SYMBOL(drm_helper_move_panel_connectors_to_head);

 
void drm_helper_mode_fill_fb_struct(struct drm_device *dev,
				    struct drm_framebuffer *fb,
				    const struct drm_mode_fb_cmd2 *mode_cmd)
{
	int i;

	fb->dev = dev;
	fb->format = drm_get_format_info(dev, mode_cmd);
	fb->width = mode_cmd->width;
	fb->height = mode_cmd->height;
	for (i = 0; i < 4; i++) {
		fb->pitches[i] = mode_cmd->pitches[i];
		fb->offsets[i] = mode_cmd->offsets[i];
	}
	fb->modifier = mode_cmd->modifier[0];
	fb->flags = mode_cmd->flags;
}
EXPORT_SYMBOL(drm_helper_mode_fill_fb_struct);

 
static const uint32_t safe_modeset_formats[] = {
	DRM_FORMAT_XRGB8888,
	DRM_FORMAT_ARGB8888,
};

static const struct drm_plane_funcs primary_plane_funcs = {
	DRM_PLANE_NON_ATOMIC_FUNCS,
};

 
int drm_crtc_init(struct drm_device *dev, struct drm_crtc *crtc,
		  const struct drm_crtc_funcs *funcs)
{
	struct drm_plane *primary;
	int ret;

	 
	primary = __drm_universal_plane_alloc(dev, sizeof(*primary), 0, 0,
					      &primary_plane_funcs,
					      safe_modeset_formats,
					      ARRAY_SIZE(safe_modeset_formats),
					      NULL, DRM_PLANE_TYPE_PRIMARY, NULL);
	if (IS_ERR(primary))
		return PTR_ERR(primary);

	 
	primary->format_default = true;

	ret = drm_crtc_init_with_planes(dev, crtc, primary, NULL, funcs, NULL);
	if (ret)
		goto err_drm_plane_cleanup;

	return 0;

err_drm_plane_cleanup:
	drm_plane_cleanup(primary);
	kfree(primary);
	return ret;
}
EXPORT_SYMBOL(drm_crtc_init);

 
int drm_mode_config_helper_suspend(struct drm_device *dev)
{
	struct drm_atomic_state *state;

	if (!dev)
		return 0;

	drm_kms_helper_poll_disable(dev);
	drm_fb_helper_set_suspend_unlocked(dev->fb_helper, 1);
	state = drm_atomic_helper_suspend(dev);
	if (IS_ERR(state)) {
		drm_fb_helper_set_suspend_unlocked(dev->fb_helper, 0);
		drm_kms_helper_poll_enable(dev);
		return PTR_ERR(state);
	}

	dev->mode_config.suspend_state = state;

	return 0;
}
EXPORT_SYMBOL(drm_mode_config_helper_suspend);

 
int drm_mode_config_helper_resume(struct drm_device *dev)
{
	int ret;

	if (!dev)
		return 0;

	if (WARN_ON(!dev->mode_config.suspend_state))
		return -EINVAL;

	ret = drm_atomic_helper_resume(dev, dev->mode_config.suspend_state);
	if (ret)
		DRM_ERROR("Failed to resume (%d)\n", ret);
	dev->mode_config.suspend_state = NULL;

	drm_fb_helper_set_suspend_unlocked(dev->fb_helper, 0);
	drm_kms_helper_poll_enable(dev);

	return ret;
}
EXPORT_SYMBOL(drm_mode_config_helper_resume);
