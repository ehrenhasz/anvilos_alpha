
 

#include <linux/atomic.h>
#include <linux/bug.h>
#include <linux/completion.h>
#include <linux/device.h>
#include <linux/mutex.h>
#include <linux/slab.h>

#include "internals.h"

 
int i3c_device_do_priv_xfers(struct i3c_device *dev,
			     struct i3c_priv_xfer *xfers,
			     int nxfers)
{
	int ret, i;

	if (nxfers < 1)
		return 0;

	for (i = 0; i < nxfers; i++) {
		if (!xfers[i].len || !xfers[i].data.in)
			return -EINVAL;
	}

	i3c_bus_normaluse_lock(dev->bus);
	ret = i3c_dev_do_priv_xfers_locked(dev->desc, xfers, nxfers);
	i3c_bus_normaluse_unlock(dev->bus);

	return ret;
}
EXPORT_SYMBOL_GPL(i3c_device_do_priv_xfers);

 
int i3c_device_do_setdasa(struct i3c_device *dev)
{
	int ret;

	i3c_bus_normaluse_lock(dev->bus);
	ret = i3c_dev_setdasa_locked(dev->desc);
	i3c_bus_normaluse_unlock(dev->bus);

	return ret;
}
EXPORT_SYMBOL_GPL(i3c_device_do_setdasa);

 
void i3c_device_get_info(const struct i3c_device *dev,
			 struct i3c_device_info *info)
{
	if (!info)
		return;

	i3c_bus_normaluse_lock(dev->bus);
	if (dev->desc)
		*info = dev->desc->info;
	i3c_bus_normaluse_unlock(dev->bus);
}
EXPORT_SYMBOL_GPL(i3c_device_get_info);

 
int i3c_device_disable_ibi(struct i3c_device *dev)
{
	int ret = -ENOENT;

	i3c_bus_normaluse_lock(dev->bus);
	if (dev->desc) {
		mutex_lock(&dev->desc->ibi_lock);
		ret = i3c_dev_disable_ibi_locked(dev->desc);
		mutex_unlock(&dev->desc->ibi_lock);
	}
	i3c_bus_normaluse_unlock(dev->bus);

	return ret;
}
EXPORT_SYMBOL_GPL(i3c_device_disable_ibi);

 
int i3c_device_enable_ibi(struct i3c_device *dev)
{
	int ret = -ENOENT;

	i3c_bus_normaluse_lock(dev->bus);
	if (dev->desc) {
		mutex_lock(&dev->desc->ibi_lock);
		ret = i3c_dev_enable_ibi_locked(dev->desc);
		mutex_unlock(&dev->desc->ibi_lock);
	}
	i3c_bus_normaluse_unlock(dev->bus);

	return ret;
}
EXPORT_SYMBOL_GPL(i3c_device_enable_ibi);

 
int i3c_device_request_ibi(struct i3c_device *dev,
			   const struct i3c_ibi_setup *req)
{
	int ret = -ENOENT;

	if (!req->handler || !req->num_slots)
		return -EINVAL;

	i3c_bus_normaluse_lock(dev->bus);
	if (dev->desc) {
		mutex_lock(&dev->desc->ibi_lock);
		ret = i3c_dev_request_ibi_locked(dev->desc, req);
		mutex_unlock(&dev->desc->ibi_lock);
	}
	i3c_bus_normaluse_unlock(dev->bus);

	return ret;
}
EXPORT_SYMBOL_GPL(i3c_device_request_ibi);

 
void i3c_device_free_ibi(struct i3c_device *dev)
{
	i3c_bus_normaluse_lock(dev->bus);
	if (dev->desc) {
		mutex_lock(&dev->desc->ibi_lock);
		i3c_dev_free_ibi_locked(dev->desc);
		mutex_unlock(&dev->desc->ibi_lock);
	}
	i3c_bus_normaluse_unlock(dev->bus);
}
EXPORT_SYMBOL_GPL(i3c_device_free_ibi);

 
struct device *i3cdev_to_dev(struct i3c_device *i3cdev)
{
	return &i3cdev->dev;
}
EXPORT_SYMBOL_GPL(i3cdev_to_dev);

 
const struct i3c_device_id *
i3c_device_match_id(struct i3c_device *i3cdev,
		    const struct i3c_device_id *id_table)
{
	struct i3c_device_info devinfo;
	const struct i3c_device_id *id;
	u16 manuf, part, ext_info;
	bool rndpid;

	i3c_device_get_info(i3cdev, &devinfo);

	manuf = I3C_PID_MANUF_ID(devinfo.pid);
	part = I3C_PID_PART_ID(devinfo.pid);
	ext_info = I3C_PID_EXTRA_INFO(devinfo.pid);
	rndpid = I3C_PID_RND_LOWER_32BITS(devinfo.pid);

	for (id = id_table; id->match_flags != 0; id++) {
		if ((id->match_flags & I3C_MATCH_DCR) &&
		    id->dcr != devinfo.dcr)
			continue;

		if ((id->match_flags & I3C_MATCH_MANUF) &&
		    id->manuf_id != manuf)
			continue;

		if ((id->match_flags & I3C_MATCH_PART) &&
		    (rndpid || id->part_id != part))
			continue;

		if ((id->match_flags & I3C_MATCH_EXTRA_INFO) &&
		    (rndpid || id->extra_info != ext_info))
			continue;

		return id;
	}

	return NULL;
}
EXPORT_SYMBOL_GPL(i3c_device_match_id);

 
int i3c_driver_register_with_owner(struct i3c_driver *drv, struct module *owner)
{
	drv->driver.owner = owner;
	drv->driver.bus = &i3c_bus_type;

	if (!drv->probe) {
		pr_err("Trying to register an i3c driver without probe callback\n");
		return -EINVAL;
	}

	return driver_register(&drv->driver);
}
EXPORT_SYMBOL_GPL(i3c_driver_register_with_owner);

 
void i3c_driver_unregister(struct i3c_driver *drv)
{
	driver_unregister(&drv->driver);
}
EXPORT_SYMBOL_GPL(i3c_driver_unregister);
