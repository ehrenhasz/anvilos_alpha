
 

#include <linux/kernel.h>
#include <linux/export.h>
#include <linux/module.h>
#include <linux/err.h>
#include <linux/debugfs.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/of.h>
#include <linux/phy/phy.h>
#include <linux/idr.h>
#include <linux/pm_runtime.h>
#include <linux/regulator/consumer.h>

static struct class *phy_class;
static struct dentry *phy_debugfs_root;
static DEFINE_MUTEX(phy_provider_mutex);
static LIST_HEAD(phy_provider_list);
static LIST_HEAD(phys);
static DEFINE_IDA(phy_ida);

static void devm_phy_release(struct device *dev, void *res)
{
	struct phy *phy = *(struct phy **)res;

	phy_put(dev, phy);
}

static void devm_phy_provider_release(struct device *dev, void *res)
{
	struct phy_provider *phy_provider = *(struct phy_provider **)res;

	of_phy_provider_unregister(phy_provider);
}

static void devm_phy_consume(struct device *dev, void *res)
{
	struct phy *phy = *(struct phy **)res;

	phy_destroy(phy);
}

static int devm_phy_match(struct device *dev, void *res, void *match_data)
{
	struct phy **phy = res;

	return *phy == match_data;
}

 
int phy_create_lookup(struct phy *phy, const char *con_id, const char *dev_id)
{
	struct phy_lookup *pl;

	if (!phy || !dev_id || !con_id)
		return -EINVAL;

	pl = kzalloc(sizeof(*pl), GFP_KERNEL);
	if (!pl)
		return -ENOMEM;

	pl->dev_id = dev_id;
	pl->con_id = con_id;
	pl->phy = phy;

	mutex_lock(&phy_provider_mutex);
	list_add_tail(&pl->node, &phys);
	mutex_unlock(&phy_provider_mutex);

	return 0;
}
EXPORT_SYMBOL_GPL(phy_create_lookup);

 
void phy_remove_lookup(struct phy *phy, const char *con_id, const char *dev_id)
{
	struct phy_lookup *pl;

	if (!phy || !dev_id || !con_id)
		return;

	mutex_lock(&phy_provider_mutex);
	list_for_each_entry(pl, &phys, node)
		if (pl->phy == phy && !strcmp(pl->dev_id, dev_id) &&
		    !strcmp(pl->con_id, con_id)) {
			list_del(&pl->node);
			kfree(pl);
			break;
		}
	mutex_unlock(&phy_provider_mutex);
}
EXPORT_SYMBOL_GPL(phy_remove_lookup);

static struct phy *phy_find(struct device *dev, const char *con_id)
{
	const char *dev_id = dev_name(dev);
	struct phy_lookup *p, *pl = NULL;

	mutex_lock(&phy_provider_mutex);
	list_for_each_entry(p, &phys, node)
		if (!strcmp(p->dev_id, dev_id) && !strcmp(p->con_id, con_id)) {
			pl = p;
			break;
		}
	mutex_unlock(&phy_provider_mutex);

	return pl ? pl->phy : ERR_PTR(-ENODEV);
}

static struct phy_provider *of_phy_provider_lookup(struct device_node *node)
{
	struct phy_provider *phy_provider;
	struct device_node *child;

	list_for_each_entry(phy_provider, &phy_provider_list, list) {
		if (phy_provider->dev->of_node == node)
			return phy_provider;

		for_each_child_of_node(phy_provider->children, child)
			if (child == node)
				return phy_provider;
	}

	return ERR_PTR(-EPROBE_DEFER);
}

int phy_pm_runtime_get(struct phy *phy)
{
	int ret;

	if (!phy)
		return 0;

	if (!pm_runtime_enabled(&phy->dev))
		return -ENOTSUPP;

	ret = pm_runtime_get(&phy->dev);
	if (ret < 0 && ret != -EINPROGRESS)
		pm_runtime_put_noidle(&phy->dev);

	return ret;
}
EXPORT_SYMBOL_GPL(phy_pm_runtime_get);

int phy_pm_runtime_get_sync(struct phy *phy)
{
	int ret;

	if (!phy)
		return 0;

	if (!pm_runtime_enabled(&phy->dev))
		return -ENOTSUPP;

	ret = pm_runtime_get_sync(&phy->dev);
	if (ret < 0)
		pm_runtime_put_sync(&phy->dev);

	return ret;
}
EXPORT_SYMBOL_GPL(phy_pm_runtime_get_sync);

int phy_pm_runtime_put(struct phy *phy)
{
	if (!phy)
		return 0;

	if (!pm_runtime_enabled(&phy->dev))
		return -ENOTSUPP;

	return pm_runtime_put(&phy->dev);
}
EXPORT_SYMBOL_GPL(phy_pm_runtime_put);

int phy_pm_runtime_put_sync(struct phy *phy)
{
	if (!phy)
		return 0;

	if (!pm_runtime_enabled(&phy->dev))
		return -ENOTSUPP;

	return pm_runtime_put_sync(&phy->dev);
}
EXPORT_SYMBOL_GPL(phy_pm_runtime_put_sync);

void phy_pm_runtime_allow(struct phy *phy)
{
	if (!phy)
		return;

	if (!pm_runtime_enabled(&phy->dev))
		return;

	pm_runtime_allow(&phy->dev);
}
EXPORT_SYMBOL_GPL(phy_pm_runtime_allow);

void phy_pm_runtime_forbid(struct phy *phy)
{
	if (!phy)
		return;

	if (!pm_runtime_enabled(&phy->dev))
		return;

	pm_runtime_forbid(&phy->dev);
}
EXPORT_SYMBOL_GPL(phy_pm_runtime_forbid);

 
int phy_init(struct phy *phy)
{
	int ret;

	if (!phy)
		return 0;

	ret = phy_pm_runtime_get_sync(phy);
	if (ret < 0 && ret != -ENOTSUPP)
		return ret;
	ret = 0;  

	mutex_lock(&phy->mutex);
	if (phy->power_count > phy->init_count)
		dev_warn(&phy->dev, "phy_power_on was called before phy_init\n");

	if (phy->init_count == 0 && phy->ops->init) {
		ret = phy->ops->init(phy);
		if (ret < 0) {
			dev_err(&phy->dev, "phy init failed --> %d\n", ret);
			goto out;
		}
	}
	++phy->init_count;

out:
	mutex_unlock(&phy->mutex);
	phy_pm_runtime_put(phy);
	return ret;
}
EXPORT_SYMBOL_GPL(phy_init);

 
int phy_exit(struct phy *phy)
{
	int ret;

	if (!phy)
		return 0;

	ret = phy_pm_runtime_get_sync(phy);
	if (ret < 0 && ret != -ENOTSUPP)
		return ret;
	ret = 0;  

	mutex_lock(&phy->mutex);
	if (phy->init_count == 1 && phy->ops->exit) {
		ret = phy->ops->exit(phy);
		if (ret < 0) {
			dev_err(&phy->dev, "phy exit failed --> %d\n", ret);
			goto out;
		}
	}
	--phy->init_count;

out:
	mutex_unlock(&phy->mutex);
	phy_pm_runtime_put(phy);
	return ret;
}
EXPORT_SYMBOL_GPL(phy_exit);

 
int phy_power_on(struct phy *phy)
{
	int ret = 0;

	if (!phy)
		goto out;

	if (phy->pwr) {
		ret = regulator_enable(phy->pwr);
		if (ret)
			goto out;
	}

	ret = phy_pm_runtime_get_sync(phy);
	if (ret < 0 && ret != -ENOTSUPP)
		goto err_pm_sync;

	ret = 0;  

	mutex_lock(&phy->mutex);
	if (phy->power_count == 0 && phy->ops->power_on) {
		ret = phy->ops->power_on(phy);
		if (ret < 0) {
			dev_err(&phy->dev, "phy poweron failed --> %d\n", ret);
			goto err_pwr_on;
		}
	}
	++phy->power_count;
	mutex_unlock(&phy->mutex);
	return 0;

err_pwr_on:
	mutex_unlock(&phy->mutex);
	phy_pm_runtime_put_sync(phy);
err_pm_sync:
	if (phy->pwr)
		regulator_disable(phy->pwr);
out:
	return ret;
}
EXPORT_SYMBOL_GPL(phy_power_on);

 
int phy_power_off(struct phy *phy)
{
	int ret;

	if (!phy)
		return 0;

	mutex_lock(&phy->mutex);
	if (phy->power_count == 1 && phy->ops->power_off) {
		ret =  phy->ops->power_off(phy);
		if (ret < 0) {
			dev_err(&phy->dev, "phy poweroff failed --> %d\n", ret);
			mutex_unlock(&phy->mutex);
			return ret;
		}
	}
	--phy->power_count;
	mutex_unlock(&phy->mutex);
	phy_pm_runtime_put(phy);

	if (phy->pwr)
		regulator_disable(phy->pwr);

	return 0;
}
EXPORT_SYMBOL_GPL(phy_power_off);

int phy_set_mode_ext(struct phy *phy, enum phy_mode mode, int submode)
{
	int ret;

	if (!phy || !phy->ops->set_mode)
		return 0;

	mutex_lock(&phy->mutex);
	ret = phy->ops->set_mode(phy, mode, submode);
	if (!ret)
		phy->attrs.mode = mode;
	mutex_unlock(&phy->mutex);

	return ret;
}
EXPORT_SYMBOL_GPL(phy_set_mode_ext);

int phy_set_media(struct phy *phy, enum phy_media media)
{
	int ret;

	if (!phy || !phy->ops->set_media)
		return 0;

	mutex_lock(&phy->mutex);
	ret = phy->ops->set_media(phy, media);
	mutex_unlock(&phy->mutex);

	return ret;
}
EXPORT_SYMBOL_GPL(phy_set_media);

int phy_set_speed(struct phy *phy, int speed)
{
	int ret;

	if (!phy || !phy->ops->set_speed)
		return 0;

	mutex_lock(&phy->mutex);
	ret = phy->ops->set_speed(phy, speed);
	mutex_unlock(&phy->mutex);

	return ret;
}
EXPORT_SYMBOL_GPL(phy_set_speed);

int phy_reset(struct phy *phy)
{
	int ret;

	if (!phy || !phy->ops->reset)
		return 0;

	ret = phy_pm_runtime_get_sync(phy);
	if (ret < 0 && ret != -ENOTSUPP)
		return ret;

	mutex_lock(&phy->mutex);
	ret = phy->ops->reset(phy);
	mutex_unlock(&phy->mutex);

	phy_pm_runtime_put(phy);

	return ret;
}
EXPORT_SYMBOL_GPL(phy_reset);

 
int phy_calibrate(struct phy *phy)
{
	int ret;

	if (!phy || !phy->ops->calibrate)
		return 0;

	mutex_lock(&phy->mutex);
	ret = phy->ops->calibrate(phy);
	mutex_unlock(&phy->mutex);

	return ret;
}
EXPORT_SYMBOL_GPL(phy_calibrate);

 
int phy_configure(struct phy *phy, union phy_configure_opts *opts)
{
	int ret;

	if (!phy)
		return -EINVAL;

	if (!phy->ops->configure)
		return -EOPNOTSUPP;

	mutex_lock(&phy->mutex);
	ret = phy->ops->configure(phy, opts);
	mutex_unlock(&phy->mutex);

	return ret;
}
EXPORT_SYMBOL_GPL(phy_configure);

 
int phy_validate(struct phy *phy, enum phy_mode mode, int submode,
		 union phy_configure_opts *opts)
{
	int ret;

	if (!phy)
		return -EINVAL;

	if (!phy->ops->validate)
		return -EOPNOTSUPP;

	mutex_lock(&phy->mutex);
	ret = phy->ops->validate(phy, mode, submode, opts);
	mutex_unlock(&phy->mutex);

	return ret;
}
EXPORT_SYMBOL_GPL(phy_validate);

 
static struct phy *_of_phy_get(struct device_node *np, int index)
{
	int ret;
	struct phy_provider *phy_provider;
	struct phy *phy = NULL;
	struct of_phandle_args args;

	ret = of_parse_phandle_with_args(np, "phys", "#phy-cells",
		index, &args);
	if (ret)
		return ERR_PTR(-ENODEV);

	 
	if (of_device_is_compatible(args.np, "usb-nop-xceiv"))
		return ERR_PTR(-ENODEV);

	mutex_lock(&phy_provider_mutex);
	phy_provider = of_phy_provider_lookup(args.np);
	if (IS_ERR(phy_provider) || !try_module_get(phy_provider->owner)) {
		phy = ERR_PTR(-EPROBE_DEFER);
		goto out_unlock;
	}

	if (!of_device_is_available(args.np)) {
		dev_warn(phy_provider->dev, "Requested PHY is disabled\n");
		phy = ERR_PTR(-ENODEV);
		goto out_put_module;
	}

	phy = phy_provider->of_xlate(phy_provider->dev, &args);

out_put_module:
	module_put(phy_provider->owner);

out_unlock:
	mutex_unlock(&phy_provider_mutex);
	of_node_put(args.np);

	return phy;
}

 
struct phy *of_phy_get(struct device_node *np, const char *con_id)
{
	struct phy *phy = NULL;
	int index = 0;

	if (con_id)
		index = of_property_match_string(np, "phy-names", con_id);

	phy = _of_phy_get(np, index);
	if (IS_ERR(phy))
		return phy;

	if (!try_module_get(phy->ops->owner))
		return ERR_PTR(-EPROBE_DEFER);

	get_device(&phy->dev);

	return phy;
}
EXPORT_SYMBOL_GPL(of_phy_get);

 
void of_phy_put(struct phy *phy)
{
	if (!phy || IS_ERR(phy))
		return;

	mutex_lock(&phy->mutex);
	if (phy->ops->release)
		phy->ops->release(phy);
	mutex_unlock(&phy->mutex);

	module_put(phy->ops->owner);
	put_device(&phy->dev);
}
EXPORT_SYMBOL_GPL(of_phy_put);

 
void phy_put(struct device *dev, struct phy *phy)
{
	device_link_remove(dev, &phy->dev);
	of_phy_put(phy);
}
EXPORT_SYMBOL_GPL(phy_put);

 
void devm_phy_put(struct device *dev, struct phy *phy)
{
	int r;

	if (!phy)
		return;

	r = devres_destroy(dev, devm_phy_release, devm_phy_match, phy);
	dev_WARN_ONCE(dev, r, "couldn't find PHY resource\n");
}
EXPORT_SYMBOL_GPL(devm_phy_put);

 
struct phy *of_phy_simple_xlate(struct device *dev, struct of_phandle_args
	*args)
{
	struct phy *phy;
	struct class_dev_iter iter;

	class_dev_iter_init(&iter, phy_class, NULL, NULL);
	while ((dev = class_dev_iter_next(&iter))) {
		phy = to_phy(dev);
		if (args->np != phy->dev.of_node)
			continue;

		class_dev_iter_exit(&iter);
		return phy;
	}

	class_dev_iter_exit(&iter);
	return ERR_PTR(-ENODEV);
}
EXPORT_SYMBOL_GPL(of_phy_simple_xlate);

 
struct phy *phy_get(struct device *dev, const char *string)
{
	int index = 0;
	struct phy *phy;
	struct device_link *link;

	if (dev->of_node) {
		if (string)
			index = of_property_match_string(dev->of_node, "phy-names",
				string);
		else
			index = 0;
		phy = _of_phy_get(dev->of_node, index);
	} else {
		if (string == NULL) {
			dev_WARN(dev, "missing string\n");
			return ERR_PTR(-EINVAL);
		}
		phy = phy_find(dev, string);
	}
	if (IS_ERR(phy))
		return phy;

	if (!try_module_get(phy->ops->owner))
		return ERR_PTR(-EPROBE_DEFER);

	get_device(&phy->dev);

	link = device_link_add(dev, &phy->dev, DL_FLAG_STATELESS);
	if (!link)
		dev_dbg(dev, "failed to create device link to %s\n",
			dev_name(phy->dev.parent));

	return phy;
}
EXPORT_SYMBOL_GPL(phy_get);

 
struct phy *devm_phy_get(struct device *dev, const char *string)
{
	struct phy **ptr, *phy;

	ptr = devres_alloc(devm_phy_release, sizeof(*ptr), GFP_KERNEL);
	if (!ptr)
		return ERR_PTR(-ENOMEM);

	phy = phy_get(dev, string);
	if (!IS_ERR(phy)) {
		*ptr = phy;
		devres_add(dev, ptr);
	} else {
		devres_free(ptr);
	}

	return phy;
}
EXPORT_SYMBOL_GPL(devm_phy_get);

 
struct phy *devm_phy_optional_get(struct device *dev, const char *string)
{
	struct phy *phy = devm_phy_get(dev, string);

	if (PTR_ERR(phy) == -ENODEV)
		phy = NULL;

	return phy;
}
EXPORT_SYMBOL_GPL(devm_phy_optional_get);

 
struct phy *devm_of_phy_get(struct device *dev, struct device_node *np,
			    const char *con_id)
{
	struct phy **ptr, *phy;
	struct device_link *link;

	ptr = devres_alloc(devm_phy_release, sizeof(*ptr), GFP_KERNEL);
	if (!ptr)
		return ERR_PTR(-ENOMEM);

	phy = of_phy_get(np, con_id);
	if (!IS_ERR(phy)) {
		*ptr = phy;
		devres_add(dev, ptr);
	} else {
		devres_free(ptr);
		return phy;
	}

	link = device_link_add(dev, &phy->dev, DL_FLAG_STATELESS);
	if (!link)
		dev_dbg(dev, "failed to create device link to %s\n",
			dev_name(phy->dev.parent));

	return phy;
}
EXPORT_SYMBOL_GPL(devm_of_phy_get);

 
struct phy *devm_of_phy_optional_get(struct device *dev, struct device_node *np,
				     const char *con_id)
{
	struct phy *phy = devm_of_phy_get(dev, np, con_id);

	if (PTR_ERR(phy) == -ENODEV)
		phy = NULL;

	if (IS_ERR(phy))
		dev_err_probe(dev, PTR_ERR(phy), "failed to get PHY %pOF:%s",
			      np, con_id);

	return phy;
}
EXPORT_SYMBOL_GPL(devm_of_phy_optional_get);

 
struct phy *devm_of_phy_get_by_index(struct device *dev, struct device_node *np,
				     int index)
{
	struct phy **ptr, *phy;
	struct device_link *link;

	ptr = devres_alloc(devm_phy_release, sizeof(*ptr), GFP_KERNEL);
	if (!ptr)
		return ERR_PTR(-ENOMEM);

	phy = _of_phy_get(np, index);
	if (IS_ERR(phy)) {
		devres_free(ptr);
		return phy;
	}

	if (!try_module_get(phy->ops->owner)) {
		devres_free(ptr);
		return ERR_PTR(-EPROBE_DEFER);
	}

	get_device(&phy->dev);

	*ptr = phy;
	devres_add(dev, ptr);

	link = device_link_add(dev, &phy->dev, DL_FLAG_STATELESS);
	if (!link)
		dev_dbg(dev, "failed to create device link to %s\n",
			dev_name(phy->dev.parent));

	return phy;
}
EXPORT_SYMBOL_GPL(devm_of_phy_get_by_index);

 
struct phy *phy_create(struct device *dev, struct device_node *node,
		       const struct phy_ops *ops)
{
	int ret;
	int id;
	struct phy *phy;

	if (WARN_ON(!dev))
		return ERR_PTR(-EINVAL);

	phy = kzalloc(sizeof(*phy), GFP_KERNEL);
	if (!phy)
		return ERR_PTR(-ENOMEM);

	id = ida_simple_get(&phy_ida, 0, 0, GFP_KERNEL);
	if (id < 0) {
		dev_err(dev, "unable to get id\n");
		ret = id;
		goto free_phy;
	}

	device_initialize(&phy->dev);
	mutex_init(&phy->mutex);

	phy->dev.class = phy_class;
	phy->dev.parent = dev;
	phy->dev.of_node = node ?: dev->of_node;
	phy->id = id;
	phy->ops = ops;

	ret = dev_set_name(&phy->dev, "phy-%s.%d", dev_name(dev), id);
	if (ret)
		goto put_dev;

	 
	phy->pwr = regulator_get_optional(&phy->dev, "phy");
	if (IS_ERR(phy->pwr)) {
		ret = PTR_ERR(phy->pwr);
		if (ret == -EPROBE_DEFER)
			goto put_dev;

		phy->pwr = NULL;
	}

	ret = device_add(&phy->dev);
	if (ret)
		goto put_dev;

	if (pm_runtime_enabled(dev)) {
		pm_runtime_enable(&phy->dev);
		pm_runtime_no_callbacks(&phy->dev);
	}

	phy->debugfs = debugfs_create_dir(dev_name(&phy->dev), phy_debugfs_root);

	return phy;

put_dev:
	put_device(&phy->dev);   
	return ERR_PTR(ret);

free_phy:
	kfree(phy);
	return ERR_PTR(ret);
}
EXPORT_SYMBOL_GPL(phy_create);

 
struct phy *devm_phy_create(struct device *dev, struct device_node *node,
			    const struct phy_ops *ops)
{
	struct phy **ptr, *phy;

	ptr = devres_alloc(devm_phy_consume, sizeof(*ptr), GFP_KERNEL);
	if (!ptr)
		return ERR_PTR(-ENOMEM);

	phy = phy_create(dev, node, ops);
	if (!IS_ERR(phy)) {
		*ptr = phy;
		devres_add(dev, ptr);
	} else {
		devres_free(ptr);
	}

	return phy;
}
EXPORT_SYMBOL_GPL(devm_phy_create);

 
void phy_destroy(struct phy *phy)
{
	pm_runtime_disable(&phy->dev);
	device_unregister(&phy->dev);
}
EXPORT_SYMBOL_GPL(phy_destroy);

 
void devm_phy_destroy(struct device *dev, struct phy *phy)
{
	int r;

	r = devres_destroy(dev, devm_phy_consume, devm_phy_match, phy);
	dev_WARN_ONCE(dev, r, "couldn't find PHY resource\n");
}
EXPORT_SYMBOL_GPL(devm_phy_destroy);

 
struct phy_provider *__of_phy_provider_register(struct device *dev,
	struct device_node *children, struct module *owner,
	struct phy * (*of_xlate)(struct device *dev,
				 struct of_phandle_args *args))
{
	struct phy_provider *phy_provider;

	 
	if (children) {
		struct device_node *parent = of_node_get(children), *next;

		while (parent) {
			if (parent == dev->of_node)
				break;

			next = of_get_parent(parent);
			of_node_put(parent);
			parent = next;
		}

		if (!parent)
			return ERR_PTR(-EINVAL);

		of_node_put(parent);
	} else {
		children = dev->of_node;
	}

	phy_provider = kzalloc(sizeof(*phy_provider), GFP_KERNEL);
	if (!phy_provider)
		return ERR_PTR(-ENOMEM);

	phy_provider->dev = dev;
	phy_provider->children = of_node_get(children);
	phy_provider->owner = owner;
	phy_provider->of_xlate = of_xlate;

	mutex_lock(&phy_provider_mutex);
	list_add_tail(&phy_provider->list, &phy_provider_list);
	mutex_unlock(&phy_provider_mutex);

	return phy_provider;
}
EXPORT_SYMBOL_GPL(__of_phy_provider_register);

 
struct phy_provider *__devm_of_phy_provider_register(struct device *dev,
	struct device_node *children, struct module *owner,
	struct phy * (*of_xlate)(struct device *dev,
				 struct of_phandle_args *args))
{
	struct phy_provider **ptr, *phy_provider;

	ptr = devres_alloc(devm_phy_provider_release, sizeof(*ptr), GFP_KERNEL);
	if (!ptr)
		return ERR_PTR(-ENOMEM);

	phy_provider = __of_phy_provider_register(dev, children, owner,
						  of_xlate);
	if (!IS_ERR(phy_provider)) {
		*ptr = phy_provider;
		devres_add(dev, ptr);
	} else {
		devres_free(ptr);
	}

	return phy_provider;
}
EXPORT_SYMBOL_GPL(__devm_of_phy_provider_register);

 
void of_phy_provider_unregister(struct phy_provider *phy_provider)
{
	if (IS_ERR(phy_provider))
		return;

	mutex_lock(&phy_provider_mutex);
	list_del(&phy_provider->list);
	of_node_put(phy_provider->children);
	kfree(phy_provider);
	mutex_unlock(&phy_provider_mutex);
}
EXPORT_SYMBOL_GPL(of_phy_provider_unregister);

 
void devm_of_phy_provider_unregister(struct device *dev,
	struct phy_provider *phy_provider)
{
	int r;

	r = devres_destroy(dev, devm_phy_provider_release, devm_phy_match,
		phy_provider);
	dev_WARN_ONCE(dev, r, "couldn't find PHY provider device resource\n");
}
EXPORT_SYMBOL_GPL(devm_of_phy_provider_unregister);

 
static void phy_release(struct device *dev)
{
	struct phy *phy;

	phy = to_phy(dev);
	dev_vdbg(dev, "releasing '%s'\n", dev_name(dev));
	debugfs_remove_recursive(phy->debugfs);
	regulator_put(phy->pwr);
	ida_simple_remove(&phy_ida, phy->id);
	kfree(phy);
}

static int __init phy_core_init(void)
{
	phy_class = class_create("phy");
	if (IS_ERR(phy_class)) {
		pr_err("failed to create phy class --> %ld\n",
			PTR_ERR(phy_class));
		return PTR_ERR(phy_class);
	}

	phy_class->dev_release = phy_release;

	phy_debugfs_root = debugfs_create_dir("phy", NULL);

	return 0;
}
device_initcall(phy_core_init);

static void __exit phy_core_exit(void)
{
	debugfs_remove_recursive(phy_debugfs_root);
	class_destroy(phy_class);
}
module_exit(phy_core_exit);
