
 

#include <linux/device/driver.h>
#include <linux/device.h>
#include <linux/module.h>
#include <linux/errno.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/sysfs.h>
#include "base.h"

static struct device *next_device(struct klist_iter *i)
{
	struct klist_node *n = klist_next(i);
	struct device *dev = NULL;
	struct device_private *dev_prv;

	if (n) {
		dev_prv = to_device_private_driver(n);
		dev = dev_prv->device;
	}
	return dev;
}

 
int driver_set_override(struct device *dev, const char **override,
			const char *s, size_t len)
{
	const char *new, *old;
	char *cp;

	if (!override || !s)
		return -EINVAL;

	 
	if (len >= (PAGE_SIZE - 1))
		return -EINVAL;

	 
	len = strlen(s);

	if (!len) {
		 
		device_lock(dev);
		old = *override;
		*override = NULL;
		device_unlock(dev);
		kfree(old);

		return 0;
	}

	cp = strnchr(s, len, '\n');
	if (cp)
		len = cp - s;

	new = kstrndup(s, len, GFP_KERNEL);
	if (!new)
		return -ENOMEM;

	device_lock(dev);
	old = *override;
	if (cp != s) {
		*override = new;
	} else {
		 
		kfree(new);
		*override = NULL;
	}
	device_unlock(dev);

	kfree(old);

	return 0;
}
EXPORT_SYMBOL_GPL(driver_set_override);

 
int driver_for_each_device(struct device_driver *drv, struct device *start,
			   void *data, int (*fn)(struct device *, void *))
{
	struct klist_iter i;
	struct device *dev;
	int error = 0;

	if (!drv)
		return -EINVAL;

	klist_iter_init_node(&drv->p->klist_devices, &i,
			     start ? &start->p->knode_driver : NULL);
	while (!error && (dev = next_device(&i)))
		error = fn(dev, data);
	klist_iter_exit(&i);
	return error;
}
EXPORT_SYMBOL_GPL(driver_for_each_device);

 
struct device *driver_find_device(struct device_driver *drv,
				  struct device *start, const void *data,
				  int (*match)(struct device *dev, const void *data))
{
	struct klist_iter i;
	struct device *dev;

	if (!drv || !drv->p)
		return NULL;

	klist_iter_init_node(&drv->p->klist_devices, &i,
			     (start ? &start->p->knode_driver : NULL));
	while ((dev = next_device(&i)))
		if (match(dev, data) && get_device(dev))
			break;
	klist_iter_exit(&i);
	return dev;
}
EXPORT_SYMBOL_GPL(driver_find_device);

 
int driver_create_file(struct device_driver *drv,
		       const struct driver_attribute *attr)
{
	int error;

	if (drv)
		error = sysfs_create_file(&drv->p->kobj, &attr->attr);
	else
		error = -EINVAL;
	return error;
}
EXPORT_SYMBOL_GPL(driver_create_file);

 
void driver_remove_file(struct device_driver *drv,
			const struct driver_attribute *attr)
{
	if (drv)
		sysfs_remove_file(&drv->p->kobj, &attr->attr);
}
EXPORT_SYMBOL_GPL(driver_remove_file);

int driver_add_groups(struct device_driver *drv,
		      const struct attribute_group **groups)
{
	return sysfs_create_groups(&drv->p->kobj, groups);
}

void driver_remove_groups(struct device_driver *drv,
			  const struct attribute_group **groups)
{
	sysfs_remove_groups(&drv->p->kobj, groups);
}

 
int driver_register(struct device_driver *drv)
{
	int ret;
	struct device_driver *other;

	if (!bus_is_registered(drv->bus)) {
		pr_err("Driver '%s' was unable to register with bus_type '%s' because the bus was not initialized.\n",
			   drv->name, drv->bus->name);
		return -EINVAL;
	}

	if ((drv->bus->probe && drv->probe) ||
	    (drv->bus->remove && drv->remove) ||
	    (drv->bus->shutdown && drv->shutdown))
		pr_warn("Driver '%s' needs updating - please use "
			"bus_type methods\n", drv->name);

	other = driver_find(drv->name, drv->bus);
	if (other) {
		pr_err("Error: Driver '%s' is already registered, "
			"aborting...\n", drv->name);
		return -EBUSY;
	}

	ret = bus_add_driver(drv);
	if (ret)
		return ret;
	ret = driver_add_groups(drv, drv->groups);
	if (ret) {
		bus_remove_driver(drv);
		return ret;
	}
	kobject_uevent(&drv->p->kobj, KOBJ_ADD);
	deferred_probe_extend_timeout();

	return ret;
}
EXPORT_SYMBOL_GPL(driver_register);

 
void driver_unregister(struct device_driver *drv)
{
	if (!drv || !drv->p) {
		WARN(1, "Unexpected driver unregister!\n");
		return;
	}
	driver_remove_groups(drv, drv->groups);
	bus_remove_driver(drv);
}
EXPORT_SYMBOL_GPL(driver_unregister);
