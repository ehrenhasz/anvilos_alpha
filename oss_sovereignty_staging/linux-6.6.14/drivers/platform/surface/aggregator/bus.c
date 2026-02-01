
 

#include <linux/device.h>
#include <linux/property.h>
#include <linux/slab.h>

#include <linux/surface_aggregator/controller.h>
#include <linux/surface_aggregator/device.h>

#include "bus.h"
#include "controller.h"


 

static ssize_t modalias_show(struct device *dev, struct device_attribute *attr,
			     char *buf)
{
	struct ssam_device *sdev = to_ssam_device(dev);

	return sysfs_emit(buf, "ssam:d%02Xc%02Xt%02Xi%02Xf%02X\n",
			sdev->uid.domain, sdev->uid.category, sdev->uid.target,
			sdev->uid.instance, sdev->uid.function);
}
static DEVICE_ATTR_RO(modalias);

static struct attribute *ssam_device_attrs[] = {
	&dev_attr_modalias.attr,
	NULL,
};
ATTRIBUTE_GROUPS(ssam_device);

static int ssam_device_uevent(const struct device *dev, struct kobj_uevent_env *env)
{
	const struct ssam_device *sdev = to_ssam_device(dev);

	return add_uevent_var(env, "MODALIAS=ssam:d%02Xc%02Xt%02Xi%02Xf%02X",
			      sdev->uid.domain, sdev->uid.category,
			      sdev->uid.target, sdev->uid.instance,
			      sdev->uid.function);
}

static void ssam_device_release(struct device *dev)
{
	struct ssam_device *sdev = to_ssam_device(dev);

	ssam_controller_put(sdev->ctrl);
	fwnode_handle_put(sdev->dev.fwnode);
	kfree(sdev);
}

const struct device_type ssam_device_type = {
	.name    = "surface_aggregator_device",
	.groups  = ssam_device_groups,
	.uevent  = ssam_device_uevent,
	.release = ssam_device_release,
};
EXPORT_SYMBOL_GPL(ssam_device_type);

 
struct ssam_device *ssam_device_alloc(struct ssam_controller *ctrl,
				      struct ssam_device_uid uid)
{
	struct ssam_device *sdev;

	sdev = kzalloc(sizeof(*sdev), GFP_KERNEL);
	if (!sdev)
		return NULL;

	device_initialize(&sdev->dev);
	sdev->dev.bus = &ssam_bus_type;
	sdev->dev.type = &ssam_device_type;
	sdev->dev.parent = ssam_controller_device(ctrl);
	sdev->ctrl = ssam_controller_get(ctrl);
	sdev->uid = uid;

	dev_set_name(&sdev->dev, "%02x:%02x:%02x:%02x:%02x",
		     sdev->uid.domain, sdev->uid.category, sdev->uid.target,
		     sdev->uid.instance, sdev->uid.function);

	return sdev;
}
EXPORT_SYMBOL_GPL(ssam_device_alloc);

 
int ssam_device_add(struct ssam_device *sdev)
{
	int status;

	 
	ssam_controller_statelock(sdev->ctrl);

	if (sdev->ctrl->state != SSAM_CONTROLLER_STARTED) {
		ssam_controller_stateunlock(sdev->ctrl);
		return -ENODEV;
	}

	status = device_add(&sdev->dev);

	ssam_controller_stateunlock(sdev->ctrl);
	return status;
}
EXPORT_SYMBOL_GPL(ssam_device_add);

 
void ssam_device_remove(struct ssam_device *sdev)
{
	device_unregister(&sdev->dev);
}
EXPORT_SYMBOL_GPL(ssam_device_remove);

 
static bool ssam_device_id_compatible(const struct ssam_device_id *id,
				      struct ssam_device_uid uid)
{
	if (id->domain != uid.domain || id->category != uid.category)
		return false;

	if ((id->match_flags & SSAM_MATCH_TARGET) && id->target != uid.target)
		return false;

	if ((id->match_flags & SSAM_MATCH_INSTANCE) && id->instance != uid.instance)
		return false;

	if ((id->match_flags & SSAM_MATCH_FUNCTION) && id->function != uid.function)
		return false;

	return true;
}

 
static bool ssam_device_id_is_null(const struct ssam_device_id *id)
{
	return id->match_flags == 0 &&
		id->domain == 0 &&
		id->category == 0 &&
		id->target == 0 &&
		id->instance == 0 &&
		id->function == 0 &&
		id->driver_data == 0;
}

 
const struct ssam_device_id *ssam_device_id_match(const struct ssam_device_id *table,
						  const struct ssam_device_uid uid)
{
	const struct ssam_device_id *id;

	for (id = table; !ssam_device_id_is_null(id); ++id)
		if (ssam_device_id_compatible(id, uid))
			return id;

	return NULL;
}
EXPORT_SYMBOL_GPL(ssam_device_id_match);

 
const struct ssam_device_id *ssam_device_get_match(const struct ssam_device *dev)
{
	const struct ssam_device_driver *sdrv;

	sdrv = to_ssam_device_driver(dev->dev.driver);
	if (!sdrv)
		return NULL;

	if (!sdrv->match_table)
		return NULL;

	return ssam_device_id_match(sdrv->match_table, dev->uid);
}
EXPORT_SYMBOL_GPL(ssam_device_get_match);

 
const void *ssam_device_get_match_data(const struct ssam_device *dev)
{
	const struct ssam_device_id *id;

	id = ssam_device_get_match(dev);
	if (!id)
		return NULL;

	return (const void *)id->driver_data;
}
EXPORT_SYMBOL_GPL(ssam_device_get_match_data);

static int ssam_bus_match(struct device *dev, struct device_driver *drv)
{
	struct ssam_device_driver *sdrv = to_ssam_device_driver(drv);
	struct ssam_device *sdev = to_ssam_device(dev);

	if (!is_ssam_device(dev))
		return 0;

	return !!ssam_device_id_match(sdrv->match_table, sdev->uid);
}

static int ssam_bus_probe(struct device *dev)
{
	return to_ssam_device_driver(dev->driver)
		->probe(to_ssam_device(dev));
}

static void ssam_bus_remove(struct device *dev)
{
	struct ssam_device_driver *sdrv = to_ssam_device_driver(dev->driver);

	if (sdrv->remove)
		sdrv->remove(to_ssam_device(dev));
}

struct bus_type ssam_bus_type = {
	.name   = "surface_aggregator",
	.match  = ssam_bus_match,
	.probe  = ssam_bus_probe,
	.remove = ssam_bus_remove,
};
EXPORT_SYMBOL_GPL(ssam_bus_type);

 
int __ssam_device_driver_register(struct ssam_device_driver *sdrv,
				  struct module *owner)
{
	sdrv->driver.owner = owner;
	sdrv->driver.bus = &ssam_bus_type;

	 
	sdrv->driver.probe_type = PROBE_PREFER_ASYNCHRONOUS;

	return driver_register(&sdrv->driver);
}
EXPORT_SYMBOL_GPL(__ssam_device_driver_register);

 
void ssam_device_driver_unregister(struct ssam_device_driver *sdrv)
{
	driver_unregister(&sdrv->driver);
}
EXPORT_SYMBOL_GPL(ssam_device_driver_unregister);


 

 
int ssam_bus_register(void)
{
	return bus_register(&ssam_bus_type);
}

 
void ssam_bus_unregister(void)
{
	return bus_unregister(&ssam_bus_type);
}


 

static int ssam_device_uid_from_string(const char *str, struct ssam_device_uid *uid)
{
	u8 d, tc, tid, iid, fn;
	int n;

	n = sscanf(str, "%hhx:%hhx:%hhx:%hhx:%hhx", &d, &tc, &tid, &iid, &fn);
	if (n != 5)
		return -EINVAL;

	uid->domain = d;
	uid->category = tc;
	uid->target = tid;
	uid->instance = iid;
	uid->function = fn;

	return 0;
}

static int ssam_get_uid_for_node(struct fwnode_handle *node, struct ssam_device_uid *uid)
{
	const char *str = fwnode_get_name(node);

	 
	if (strncmp(str, "ssam:", strlen("ssam:")) != 0)
		return -ENODEV;

	str += strlen("ssam:");
	return ssam_device_uid_from_string(str, uid);
}

static int ssam_add_client_device(struct device *parent, struct ssam_controller *ctrl,
				  struct fwnode_handle *node)
{
	struct ssam_device_uid uid;
	struct ssam_device *sdev;
	int status;

	status = ssam_get_uid_for_node(node, &uid);
	if (status)
		return status;

	sdev = ssam_device_alloc(ctrl, uid);
	if (!sdev)
		return -ENOMEM;

	sdev->dev.parent = parent;
	sdev->dev.fwnode = fwnode_handle_get(node);

	status = ssam_device_add(sdev);
	if (status)
		ssam_device_put(sdev);

	return status;
}

 
int __ssam_register_clients(struct device *parent, struct ssam_controller *ctrl,
			    struct fwnode_handle *node)
{
	struct fwnode_handle *child;
	int status;

	fwnode_for_each_child_node(node, child) {
		 
		status = ssam_add_client_device(parent, ctrl, child);
		if (status && status != -ENODEV) {
			fwnode_handle_put(child);
			goto err;
		}
	}

	return 0;
err:
	ssam_remove_clients(parent);
	return status;
}
EXPORT_SYMBOL_GPL(__ssam_register_clients);

static int ssam_remove_device(struct device *dev, void *_data)
{
	struct ssam_device *sdev = to_ssam_device(dev);

	if (is_ssam_device(dev))
		ssam_device_remove(sdev);

	return 0;
}

 
void ssam_remove_clients(struct device *dev)
{
	device_for_each_child_reverse(dev, NULL, ssam_remove_device);
}
EXPORT_SYMBOL_GPL(ssam_remove_clients);
