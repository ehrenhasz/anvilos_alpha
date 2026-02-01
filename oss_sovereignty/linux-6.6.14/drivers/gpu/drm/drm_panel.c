 

#include <linux/backlight.h>
#include <linux/err.h>
#include <linux/module.h>

#include <drm/drm_crtc.h>
#include <drm/drm_panel.h>
#include <drm/drm_print.h>

static DEFINE_MUTEX(panel_lock);
static LIST_HEAD(panel_list);

 

 
void drm_panel_init(struct drm_panel *panel, struct device *dev,
		    const struct drm_panel_funcs *funcs, int connector_type)
{
	INIT_LIST_HEAD(&panel->list);
	INIT_LIST_HEAD(&panel->followers);
	mutex_init(&panel->follower_lock);
	panel->dev = dev;
	panel->funcs = funcs;
	panel->connector_type = connector_type;
}
EXPORT_SYMBOL(drm_panel_init);

 
void drm_panel_add(struct drm_panel *panel)
{
	mutex_lock(&panel_lock);
	list_add_tail(&panel->list, &panel_list);
	mutex_unlock(&panel_lock);
}
EXPORT_SYMBOL(drm_panel_add);

 
void drm_panel_remove(struct drm_panel *panel)
{
	mutex_lock(&panel_lock);
	list_del_init(&panel->list);
	mutex_unlock(&panel_lock);
}
EXPORT_SYMBOL(drm_panel_remove);

 
int drm_panel_prepare(struct drm_panel *panel)
{
	struct drm_panel_follower *follower;
	int ret;

	if (!panel)
		return -EINVAL;

	if (panel->prepared) {
		dev_warn(panel->dev, "Skipping prepare of already prepared panel\n");
		return 0;
	}

	mutex_lock(&panel->follower_lock);

	if (panel->funcs && panel->funcs->prepare) {
		ret = panel->funcs->prepare(panel);
		if (ret < 0)
			goto exit;
	}
	panel->prepared = true;

	list_for_each_entry(follower, &panel->followers, list) {
		ret = follower->funcs->panel_prepared(follower);
		if (ret < 0)
			dev_info(panel->dev, "%ps failed: %d\n",
				 follower->funcs->panel_prepared, ret);
	}

	ret = 0;
exit:
	mutex_unlock(&panel->follower_lock);

	return ret;
}
EXPORT_SYMBOL(drm_panel_prepare);

 
int drm_panel_unprepare(struct drm_panel *panel)
{
	struct drm_panel_follower *follower;
	int ret;

	if (!panel)
		return -EINVAL;

	if (!panel->prepared) {
		dev_warn(panel->dev, "Skipping unprepare of already unprepared panel\n");
		return 0;
	}

	mutex_lock(&panel->follower_lock);

	list_for_each_entry(follower, &panel->followers, list) {
		ret = follower->funcs->panel_unpreparing(follower);
		if (ret < 0)
			dev_info(panel->dev, "%ps failed: %d\n",
				 follower->funcs->panel_unpreparing, ret);
	}

	if (panel->funcs && panel->funcs->unprepare) {
		ret = panel->funcs->unprepare(panel);
		if (ret < 0)
			goto exit;
	}
	panel->prepared = false;

	ret = 0;
exit:
	mutex_unlock(&panel->follower_lock);

	return ret;
}
EXPORT_SYMBOL(drm_panel_unprepare);

 
int drm_panel_enable(struct drm_panel *panel)
{
	int ret;

	if (!panel)
		return -EINVAL;

	if (panel->enabled) {
		dev_warn(panel->dev, "Skipping enable of already enabled panel\n");
		return 0;
	}

	if (panel->funcs && panel->funcs->enable) {
		ret = panel->funcs->enable(panel);
		if (ret < 0)
			return ret;
	}
	panel->enabled = true;

	ret = backlight_enable(panel->backlight);
	if (ret < 0)
		DRM_DEV_INFO(panel->dev, "failed to enable backlight: %d\n",
			     ret);

	return 0;
}
EXPORT_SYMBOL(drm_panel_enable);

 
int drm_panel_disable(struct drm_panel *panel)
{
	int ret;

	if (!panel)
		return -EINVAL;

	if (!panel->enabled) {
		dev_warn(panel->dev, "Skipping disable of already disabled panel\n");
		return 0;
	}

	ret = backlight_disable(panel->backlight);
	if (ret < 0)
		DRM_DEV_INFO(panel->dev, "failed to disable backlight: %d\n",
			     ret);

	if (panel->funcs && panel->funcs->disable) {
		ret = panel->funcs->disable(panel);
		if (ret < 0)
			return ret;
	}
	panel->enabled = false;

	return 0;
}
EXPORT_SYMBOL(drm_panel_disable);

 
int drm_panel_get_modes(struct drm_panel *panel,
			struct drm_connector *connector)
{
	if (!panel)
		return -EINVAL;

	if (panel->funcs && panel->funcs->get_modes)
		return panel->funcs->get_modes(panel, connector);

	return -EOPNOTSUPP;
}
EXPORT_SYMBOL(drm_panel_get_modes);

#ifdef CONFIG_OF
 
struct drm_panel *of_drm_find_panel(const struct device_node *np)
{
	struct drm_panel *panel;

	if (!of_device_is_available(np))
		return ERR_PTR(-ENODEV);

	mutex_lock(&panel_lock);

	list_for_each_entry(panel, &panel_list, list) {
		if (panel->dev->of_node == np) {
			mutex_unlock(&panel_lock);
			return panel;
		}
	}

	mutex_unlock(&panel_lock);
	return ERR_PTR(-EPROBE_DEFER);
}
EXPORT_SYMBOL(of_drm_find_panel);

 
int of_drm_get_panel_orientation(const struct device_node *np,
				 enum drm_panel_orientation *orientation)
{
	int rotation, ret;

	ret = of_property_read_u32(np, "rotation", &rotation);
	if (ret == -EINVAL) {
		 
		*orientation = DRM_MODE_PANEL_ORIENTATION_UNKNOWN;
		return 0;
	}

	if (ret < 0)
		return ret;

	if (rotation == 0)
		*orientation = DRM_MODE_PANEL_ORIENTATION_NORMAL;
	else if (rotation == 90)
		*orientation = DRM_MODE_PANEL_ORIENTATION_RIGHT_UP;
	else if (rotation == 180)
		*orientation = DRM_MODE_PANEL_ORIENTATION_BOTTOM_UP;
	else if (rotation == 270)
		*orientation = DRM_MODE_PANEL_ORIENTATION_LEFT_UP;
	else
		return -EINVAL;

	return 0;
}
EXPORT_SYMBOL(of_drm_get_panel_orientation);
#endif

 
bool drm_is_panel_follower(struct device *dev)
{
	 
	return of_property_read_bool(dev->of_node, "panel");
}
EXPORT_SYMBOL(drm_is_panel_follower);

 
int drm_panel_add_follower(struct device *follower_dev,
			   struct drm_panel_follower *follower)
{
	struct device_node *panel_np;
	struct drm_panel *panel;
	int ret;

	panel_np = of_parse_phandle(follower_dev->of_node, "panel", 0);
	if (!panel_np)
		return -ENODEV;

	panel = of_drm_find_panel(panel_np);
	of_node_put(panel_np);
	if (IS_ERR(panel))
		return PTR_ERR(panel);

	get_device(panel->dev);
	follower->panel = panel;

	mutex_lock(&panel->follower_lock);

	list_add_tail(&follower->list, &panel->followers);
	if (panel->prepared) {
		ret = follower->funcs->panel_prepared(follower);
		if (ret < 0)
			dev_info(panel->dev, "%ps failed: %d\n",
				 follower->funcs->panel_prepared, ret);
	}

	mutex_unlock(&panel->follower_lock);

	return 0;
}
EXPORT_SYMBOL(drm_panel_add_follower);

 
void drm_panel_remove_follower(struct drm_panel_follower *follower)
{
	struct drm_panel *panel = follower->panel;
	int ret;

	mutex_lock(&panel->follower_lock);

	if (panel->prepared) {
		ret = follower->funcs->panel_unpreparing(follower);
		if (ret < 0)
			dev_info(panel->dev, "%ps failed: %d\n",
				 follower->funcs->panel_unpreparing, ret);
	}
	list_del_init(&follower->list);

	mutex_unlock(&panel->follower_lock);

	put_device(panel->dev);
}
EXPORT_SYMBOL(drm_panel_remove_follower);

static void drm_panel_remove_follower_void(void *follower)
{
	drm_panel_remove_follower(follower);
}

 
int devm_drm_panel_add_follower(struct device *follower_dev,
				struct drm_panel_follower *follower)
{
	int ret;

	ret = drm_panel_add_follower(follower_dev, follower);
	if (ret)
		return ret;

	return devm_add_action_or_reset(follower_dev,
					drm_panel_remove_follower_void, follower);
}
EXPORT_SYMBOL(devm_drm_panel_add_follower);

#if IS_REACHABLE(CONFIG_BACKLIGHT_CLASS_DEVICE)
 
int drm_panel_of_backlight(struct drm_panel *panel)
{
	struct backlight_device *backlight;

	if (!panel || !panel->dev)
		return -EINVAL;

	backlight = devm_of_find_backlight(panel->dev);

	if (IS_ERR(backlight))
		return PTR_ERR(backlight);

	panel->backlight = backlight;
	return 0;
}
EXPORT_SYMBOL(drm_panel_of_backlight);
#endif

MODULE_AUTHOR("Thierry Reding <treding@nvidia.com>");
MODULE_DESCRIPTION("DRM panel infrastructure");
MODULE_LICENSE("GPL and additional rights");
