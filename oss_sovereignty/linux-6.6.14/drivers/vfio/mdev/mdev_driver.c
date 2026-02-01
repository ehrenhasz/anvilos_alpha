
 

#include <linux/iommu.h>
#include <linux/mdev.h>

#include "mdev_private.h"

static int mdev_probe(struct device *dev)
{
	struct mdev_driver *drv =
		container_of(dev->driver, struct mdev_driver, driver);

	if (!drv->probe)
		return 0;
	return drv->probe(to_mdev_device(dev));
}

static void mdev_remove(struct device *dev)
{
	struct mdev_driver *drv =
		container_of(dev->driver, struct mdev_driver, driver);

	if (drv->remove)
		drv->remove(to_mdev_device(dev));
}

static int mdev_match(struct device *dev, struct device_driver *drv)
{
	 
	return 0;
}

struct bus_type mdev_bus_type = {
	.name		= "mdev",
	.probe		= mdev_probe,
	.remove		= mdev_remove,
	.match		= mdev_match,
};

 
int mdev_register_driver(struct mdev_driver *drv)
{
	if (!drv->device_api)
		return -EINVAL;

	 
	drv->driver.bus = &mdev_bus_type;
	return driver_register(&drv->driver);
}
EXPORT_SYMBOL(mdev_register_driver);

 
void mdev_unregister_driver(struct mdev_driver *drv)
{
	driver_unregister(&drv->driver);
}
EXPORT_SYMBOL(mdev_unregister_driver);
