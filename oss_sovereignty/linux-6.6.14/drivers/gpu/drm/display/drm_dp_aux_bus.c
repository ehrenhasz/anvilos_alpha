
 

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/pm_domain.h>
#include <linux/pm_runtime.h>

#include <drm/display/drm_dp_aux_bus.h>
#include <drm/display/drm_dp_helper.h>

struct dp_aux_ep_device_with_data {
	struct dp_aux_ep_device aux_ep;
	int (*done_probing)(struct drm_dp_aux *aux);
};

 
static int dp_aux_ep_match(struct device *dev, struct device_driver *drv)
{
	return !!of_match_device(drv->of_match_table, dev);
}

 
static int dp_aux_ep_probe(struct device *dev)
{
	struct dp_aux_ep_driver *aux_ep_drv = to_dp_aux_ep_drv(dev->driver);
	struct dp_aux_ep_device *aux_ep = to_dp_aux_ep_dev(dev);
	struct dp_aux_ep_device_with_data *aux_ep_with_data =
		container_of(aux_ep, struct dp_aux_ep_device_with_data, aux_ep);
	int ret;

	ret = dev_pm_domain_attach(dev, true);
	if (ret)
		return dev_err_probe(dev, ret, "Failed to attach to PM Domain\n");

	ret = aux_ep_drv->probe(aux_ep);
	if (ret)
		goto err_attached;

	if (aux_ep_with_data->done_probing) {
		ret = aux_ep_with_data->done_probing(aux_ep->aux);
		if (ret) {
			 
			if (ret == -EPROBE_DEFER) {
				dev_err(dev,
					"DP AUX done_probing() can't defer\n");
				ret = -EINVAL;
			}
			goto err_probed;
		}
	}

	return 0;
err_probed:
	if (aux_ep_drv->remove)
		aux_ep_drv->remove(aux_ep);
err_attached:
	dev_pm_domain_detach(dev, true);

	return ret;
}

 
static void dp_aux_ep_remove(struct device *dev)
{
	struct dp_aux_ep_driver *aux_ep_drv = to_dp_aux_ep_drv(dev->driver);
	struct dp_aux_ep_device *aux_ep = to_dp_aux_ep_dev(dev);

	if (aux_ep_drv->remove)
		aux_ep_drv->remove(aux_ep);
	dev_pm_domain_detach(dev, true);
}

 
static void dp_aux_ep_shutdown(struct device *dev)
{
	struct dp_aux_ep_driver *aux_ep_drv;

	if (!dev->driver)
		return;

	aux_ep_drv = to_dp_aux_ep_drv(dev->driver);
	if (aux_ep_drv->shutdown)
		aux_ep_drv->shutdown(to_dp_aux_ep_dev(dev));
}

static struct bus_type dp_aux_bus_type = {
	.name		= "dp-aux",
	.match		= dp_aux_ep_match,
	.probe		= dp_aux_ep_probe,
	.remove		= dp_aux_ep_remove,
	.shutdown	= dp_aux_ep_shutdown,
};

static ssize_t modalias_show(struct device *dev,
			     struct device_attribute *attr, char *buf)
{
	return of_device_modalias(dev, buf, PAGE_SIZE);
}
static DEVICE_ATTR_RO(modalias);

static struct attribute *dp_aux_ep_dev_attrs[] = {
	&dev_attr_modalias.attr,
	NULL,
};
ATTRIBUTE_GROUPS(dp_aux_ep_dev);

 
static void dp_aux_ep_dev_release(struct device *dev)
{
	struct dp_aux_ep_device *aux_ep = to_dp_aux_ep_dev(dev);
	struct dp_aux_ep_device_with_data *aux_ep_with_data =
		container_of(aux_ep, struct dp_aux_ep_device_with_data, aux_ep);

	kfree(aux_ep_with_data);
}

static int dp_aux_ep_dev_modalias(const struct device *dev, struct kobj_uevent_env *env)
{
	return of_device_uevent_modalias(dev, env);
}

static struct device_type dp_aux_device_type_type = {
	.groups		= dp_aux_ep_dev_groups,
	.uevent		= dp_aux_ep_dev_modalias,
	.release	= dp_aux_ep_dev_release,
};

 
static int of_dp_aux_ep_destroy(struct device *dev, void *data)
{
	struct device_node *np = dev->of_node;

	if (dev->bus != &dp_aux_bus_type)
		return 0;

	if (!of_node_check_flag(np, OF_POPULATED))
		return 0;

	of_node_clear_flag(np, OF_POPULATED);
	of_node_put(np);

	device_unregister(dev);

	return 0;
}

 
void of_dp_aux_depopulate_bus(struct drm_dp_aux *aux)
{
	device_for_each_child_reverse(aux->dev, NULL, of_dp_aux_ep_destroy);
}
EXPORT_SYMBOL_GPL(of_dp_aux_depopulate_bus);

 
int of_dp_aux_populate_bus(struct drm_dp_aux *aux,
			   int (*done_probing)(struct drm_dp_aux *aux))
{
	struct device_node *bus = NULL, *np = NULL;
	struct dp_aux_ep_device *aux_ep;
	struct dp_aux_ep_device_with_data *aux_ep_with_data;
	int ret;

	 
	WARN_ON_ONCE(!aux->ddc.algo);

	if (!aux->dev->of_node)
		return -ENODEV;
	bus = of_get_child_by_name(aux->dev->of_node, "aux-bus");
	if (!bus)
		return -ENODEV;

	np = of_get_next_available_child(bus, NULL);
	of_node_put(bus);
	if (!np)
		return -ENODEV;

	if (of_node_test_and_set_flag(np, OF_POPULATED)) {
		dev_err(aux->dev, "DP AUX EP device already populated\n");
		ret = -EINVAL;
		goto err_did_get_np;
	}

	aux_ep_with_data = kzalloc(sizeof(*aux_ep_with_data), GFP_KERNEL);
	if (!aux_ep_with_data) {
		ret = -ENOMEM;
		goto err_did_set_populated;
	}

	aux_ep_with_data->done_probing = done_probing;

	aux_ep = &aux_ep_with_data->aux_ep;
	aux_ep->aux = aux;
	aux_ep->dev.parent = aux->dev;
	aux_ep->dev.bus = &dp_aux_bus_type;
	aux_ep->dev.type = &dp_aux_device_type_type;
	aux_ep->dev.of_node = of_node_get(np);
	dev_set_name(&aux_ep->dev, "aux-%s", dev_name(aux->dev));

	ret = device_register(&aux_ep->dev);
	if (ret) {
		dev_err(aux->dev, "Failed to create AUX EP for %pOF: %d\n", np, ret);

		 
		put_device(&aux_ep->dev);

		goto err_did_set_populated;
	}

	return 0;

err_did_set_populated:
	of_node_clear_flag(np, OF_POPULATED);

err_did_get_np:
	of_node_put(np);

	return ret;
}
EXPORT_SYMBOL_GPL(of_dp_aux_populate_bus);

static void of_dp_aux_depopulate_bus_void(void *data)
{
	of_dp_aux_depopulate_bus(data);
}

 
int devm_of_dp_aux_populate_bus(struct drm_dp_aux *aux,
				int (*done_probing)(struct drm_dp_aux *aux))
{
	int ret;

	ret = of_dp_aux_populate_bus(aux, done_probing);
	if (ret)
		return ret;

	return devm_add_action_or_reset(aux->dev,
					of_dp_aux_depopulate_bus_void, aux);
}
EXPORT_SYMBOL_GPL(devm_of_dp_aux_populate_bus);

int __dp_aux_dp_driver_register(struct dp_aux_ep_driver *drv, struct module *owner)
{
	drv->driver.owner = owner;
	drv->driver.bus = &dp_aux_bus_type;

	return driver_register(&drv->driver);
}
EXPORT_SYMBOL_GPL(__dp_aux_dp_driver_register);

void dp_aux_dp_driver_unregister(struct dp_aux_ep_driver *drv)
{
	driver_unregister(&drv->driver);
}
EXPORT_SYMBOL_GPL(dp_aux_dp_driver_unregister);

static int __init dp_aux_bus_init(void)
{
	int ret;

	ret = bus_register(&dp_aux_bus_type);
	if (ret)
		return ret;

	return 0;
}

static void __exit dp_aux_bus_exit(void)
{
	bus_unregister(&dp_aux_bus_type);
}

subsys_initcall(dp_aux_bus_init);
module_exit(dp_aux_bus_exit);

MODULE_AUTHOR("Douglas Anderson <dianders@chromium.org>");
MODULE_DESCRIPTION("DRM DisplayPort AUX bus");
MODULE_LICENSE("GPL v2");
