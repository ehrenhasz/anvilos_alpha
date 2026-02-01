
 

#include <linux/device.h>
#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <drm/drm_privacy_screen_machine.h>
#include <drm/drm_privacy_screen_consumer.h>
#include <drm/drm_privacy_screen_driver.h>
#include "drm_internal.h"

 

#define to_drm_privacy_screen(dev) \
	container_of(dev, struct drm_privacy_screen, dev)

static DEFINE_MUTEX(drm_privacy_screen_lookup_lock);
static LIST_HEAD(drm_privacy_screen_lookup_list);

static DEFINE_MUTEX(drm_privacy_screen_devs_lock);
static LIST_HEAD(drm_privacy_screen_devs);

 

 
void drm_privacy_screen_lookup_add(struct drm_privacy_screen_lookup *lookup)
{
	mutex_lock(&drm_privacy_screen_lookup_lock);
	list_add(&lookup->list, &drm_privacy_screen_lookup_list);
	mutex_unlock(&drm_privacy_screen_lookup_lock);
}
EXPORT_SYMBOL(drm_privacy_screen_lookup_add);

 
void drm_privacy_screen_lookup_remove(struct drm_privacy_screen_lookup *lookup)
{
	mutex_lock(&drm_privacy_screen_lookup_lock);
	list_del(&lookup->list);
	mutex_unlock(&drm_privacy_screen_lookup_lock);
}
EXPORT_SYMBOL(drm_privacy_screen_lookup_remove);

 

static struct drm_privacy_screen *drm_privacy_screen_get_by_name(
	const char *name)
{
	struct drm_privacy_screen *priv;
	struct device *dev = NULL;

	mutex_lock(&drm_privacy_screen_devs_lock);

	list_for_each_entry(priv, &drm_privacy_screen_devs, list) {
		if (strcmp(dev_name(&priv->dev), name) == 0) {
			dev = get_device(&priv->dev);
			break;
		}
	}

	mutex_unlock(&drm_privacy_screen_devs_lock);

	return dev ? to_drm_privacy_screen(dev) : NULL;
}

 
struct drm_privacy_screen *drm_privacy_screen_get(struct device *dev,
						  const char *con_id)
{
	const char *dev_id = dev ? dev_name(dev) : NULL;
	struct drm_privacy_screen_lookup *l;
	struct drm_privacy_screen *priv;
	const char *provider = NULL;
	int match, best = -1;

	 
	mutex_lock(&drm_privacy_screen_lookup_lock);

	list_for_each_entry(l, &drm_privacy_screen_lookup_list, list) {
		match = 0;

		if (l->dev_id) {
			if (!dev_id || strcmp(l->dev_id, dev_id))
				continue;

			match += 2;
		}

		if (l->con_id) {
			if (!con_id || strcmp(l->con_id, con_id))
				continue;

			match += 1;
		}

		if (match > best) {
			provider = l->provider;
			best = match;
		}
	}

	mutex_unlock(&drm_privacy_screen_lookup_lock);

	if (!provider)
		return ERR_PTR(-ENODEV);

	priv = drm_privacy_screen_get_by_name(provider);
	if (!priv)
		return ERR_PTR(-EPROBE_DEFER);

	return priv;
}
EXPORT_SYMBOL(drm_privacy_screen_get);

 
void drm_privacy_screen_put(struct drm_privacy_screen *priv)
{
	if (IS_ERR_OR_NULL(priv))
		return;

	put_device(&priv->dev);
}
EXPORT_SYMBOL(drm_privacy_screen_put);

 
int drm_privacy_screen_set_sw_state(struct drm_privacy_screen *priv,
				    enum drm_privacy_screen_status sw_state)
{
	int ret = 0;

	mutex_lock(&priv->lock);

	if (!priv->ops) {
		ret = -ENODEV;
		goto out;
	}

	 
	if (priv->hw_state >= PRIVACY_SCREEN_DISABLED_LOCKED ||
	    priv->hw_state == sw_state) {
		priv->sw_state = sw_state;
		goto out;
	}

	ret = priv->ops->set_sw_state(priv, sw_state);
out:
	mutex_unlock(&priv->lock);
	return ret;
}
EXPORT_SYMBOL(drm_privacy_screen_set_sw_state);

 
void drm_privacy_screen_get_state(struct drm_privacy_screen *priv,
				  enum drm_privacy_screen_status *sw_state_ret,
				  enum drm_privacy_screen_status *hw_state_ret)
{
	mutex_lock(&priv->lock);
	*sw_state_ret = priv->sw_state;
	*hw_state_ret = priv->hw_state;
	mutex_unlock(&priv->lock);
}
EXPORT_SYMBOL(drm_privacy_screen_get_state);

 
int drm_privacy_screen_register_notifier(struct drm_privacy_screen *priv,
					 struct notifier_block *nb)
{
	return blocking_notifier_chain_register(&priv->notifier_head, nb);
}
EXPORT_SYMBOL(drm_privacy_screen_register_notifier);

 
int drm_privacy_screen_unregister_notifier(struct drm_privacy_screen *priv,
					   struct notifier_block *nb)
{
	return blocking_notifier_chain_unregister(&priv->notifier_head, nb);
}
EXPORT_SYMBOL(drm_privacy_screen_unregister_notifier);

 

static ssize_t sw_state_show(struct device *dev,
			     struct device_attribute *attr, char *buf)
{
	struct drm_privacy_screen *priv = to_drm_privacy_screen(dev);
	const char * const sw_state_names[] = {
		"Disabled",
		"Enabled",
	};
	ssize_t ret;

	mutex_lock(&priv->lock);

	if (!priv->ops)
		ret = -ENODEV;
	else if (WARN_ON(priv->sw_state >= ARRAY_SIZE(sw_state_names)))
		ret = -ENXIO;
	else
		ret = sprintf(buf, "%s\n", sw_state_names[priv->sw_state]);

	mutex_unlock(&priv->lock);
	return ret;
}
 
static DEVICE_ATTR_RO(sw_state);

static ssize_t hw_state_show(struct device *dev,
			     struct device_attribute *attr, char *buf)
{
	struct drm_privacy_screen *priv = to_drm_privacy_screen(dev);
	const char * const hw_state_names[] = {
		"Disabled",
		"Enabled",
		"Disabled, locked",
		"Enabled, locked",
	};
	ssize_t ret;

	mutex_lock(&priv->lock);

	if (!priv->ops)
		ret = -ENODEV;
	else if (WARN_ON(priv->hw_state >= ARRAY_SIZE(hw_state_names)))
		ret = -ENXIO;
	else
		ret = sprintf(buf, "%s\n", hw_state_names[priv->hw_state]);

	mutex_unlock(&priv->lock);
	return ret;
}
static DEVICE_ATTR_RO(hw_state);

static struct attribute *drm_privacy_screen_attrs[] = {
	&dev_attr_sw_state.attr,
	&dev_attr_hw_state.attr,
	NULL
};
ATTRIBUTE_GROUPS(drm_privacy_screen);

static struct device_type drm_privacy_screen_type = {
	.name = "privacy_screen",
	.groups = drm_privacy_screen_groups,
};

static void drm_privacy_screen_device_release(struct device *dev)
{
	struct drm_privacy_screen *priv = to_drm_privacy_screen(dev);

	kfree(priv);
}

 
struct drm_privacy_screen *drm_privacy_screen_register(
	struct device *parent, const struct drm_privacy_screen_ops *ops,
	void *data)
{
	struct drm_privacy_screen *priv;
	int ret;

	priv = kzalloc(sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return ERR_PTR(-ENOMEM);

	mutex_init(&priv->lock);
	BLOCKING_INIT_NOTIFIER_HEAD(&priv->notifier_head);

	priv->dev.class = drm_class;
	priv->dev.type = &drm_privacy_screen_type;
	priv->dev.parent = parent;
	priv->dev.release = drm_privacy_screen_device_release;
	dev_set_name(&priv->dev, "privacy_screen-%s", dev_name(parent));
	priv->drvdata = data;
	priv->ops = ops;

	priv->ops->get_hw_state(priv);

	ret = device_register(&priv->dev);
	if (ret) {
		put_device(&priv->dev);
		return ERR_PTR(ret);
	}

	mutex_lock(&drm_privacy_screen_devs_lock);
	list_add(&priv->list, &drm_privacy_screen_devs);
	mutex_unlock(&drm_privacy_screen_devs_lock);

	return priv;
}
EXPORT_SYMBOL(drm_privacy_screen_register);

 
void drm_privacy_screen_unregister(struct drm_privacy_screen *priv)
{
	if (IS_ERR_OR_NULL(priv))
		return;

	mutex_lock(&drm_privacy_screen_devs_lock);
	list_del(&priv->list);
	mutex_unlock(&drm_privacy_screen_devs_lock);

	mutex_lock(&priv->lock);
	priv->drvdata = NULL;
	priv->ops = NULL;
	mutex_unlock(&priv->lock);

	device_unregister(&priv->dev);
}
EXPORT_SYMBOL(drm_privacy_screen_unregister);

 
void drm_privacy_screen_call_notifier_chain(struct drm_privacy_screen *priv)
{
	blocking_notifier_call_chain(&priv->notifier_head, 0, priv);
}
EXPORT_SYMBOL(drm_privacy_screen_call_notifier_chain);
