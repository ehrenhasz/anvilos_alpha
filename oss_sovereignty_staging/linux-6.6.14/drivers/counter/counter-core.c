
 
#include <linux/cdev.h>
#include <linux/counter.h>
#include <linux/device.h>
#include <linux/device/bus.h>
#include <linux/export.h>
#include <linux/fs.h>
#include <linux/gfp.h>
#include <linux/idr.h>
#include <linux/init.h>
#include <linux/kdev_t.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <linux/wait.h>

#include "counter-chrdev.h"
#include "counter-sysfs.h"

#define COUNTER_NAME	"counter"

 
static DEFINE_IDA(counter_ida);

struct counter_device_allochelper {
	struct counter_device counter;

	 
	unsigned long privdata[] ____cacheline_aligned;
};

static void counter_device_release(struct device *dev)
{
	struct counter_device *const counter =
		container_of(dev, struct counter_device, dev);

	counter_chrdev_remove(counter);
	ida_free(&counter_ida, dev->id);

	kfree(container_of(counter, struct counter_device_allochelper, counter));
}

static struct device_type counter_device_type = {
	.name = "counter_device",
	.release = counter_device_release,
};

static struct bus_type counter_bus_type = {
	.name = "counter",
	.dev_name = "counter",
};

static dev_t counter_devt;

 
void *counter_priv(const struct counter_device *const counter)
{
	struct counter_device_allochelper *ch =
		container_of(counter, struct counter_device_allochelper, counter);

	return &ch->privdata;
}
EXPORT_SYMBOL_NS_GPL(counter_priv, COUNTER);

 
struct counter_device *counter_alloc(size_t sizeof_priv)
{
	struct counter_device_allochelper *ch;
	struct counter_device *counter;
	struct device *dev;
	int err;

	ch = kzalloc(sizeof(*ch) + sizeof_priv, GFP_KERNEL);
	if (!ch)
		return NULL;

	counter = &ch->counter;
	dev = &counter->dev;

	 
	err = ida_alloc(&counter_ida, GFP_KERNEL);
	if (err < 0)
		goto err_ida_alloc;
	dev->id = err;

	mutex_init(&counter->ops_exist_lock);
	dev->type = &counter_device_type;
	dev->bus = &counter_bus_type;
	dev->devt = MKDEV(MAJOR(counter_devt), dev->id);

	err = counter_chrdev_add(counter);
	if (err < 0)
		goto err_chrdev_add;

	device_initialize(dev);

	err = dev_set_name(dev, COUNTER_NAME "%d", dev->id);
	if (err)
		goto err_dev_set_name;

	return counter;

err_dev_set_name:

	counter_chrdev_remove(counter);
err_chrdev_add:

	ida_free(&counter_ida, dev->id);
err_ida_alloc:

	kfree(ch);

	return NULL;
}
EXPORT_SYMBOL_NS_GPL(counter_alloc, COUNTER);

void counter_put(struct counter_device *counter)
{
	put_device(&counter->dev);
}
EXPORT_SYMBOL_NS_GPL(counter_put, COUNTER);

 
int counter_add(struct counter_device *counter)
{
	int err;
	struct device *dev = &counter->dev;

	if (counter->parent) {
		dev->parent = counter->parent;
		dev->of_node = counter->parent->of_node;
	}

	err = counter_sysfs_add(counter);
	if (err < 0)
		return err;

	 
	return cdev_device_add(&counter->chrdev, dev);
}
EXPORT_SYMBOL_NS_GPL(counter_add, COUNTER);

 
void counter_unregister(struct counter_device *const counter)
{
	if (!counter)
		return;

	cdev_device_del(&counter->chrdev, &counter->dev);

	mutex_lock(&counter->ops_exist_lock);

	counter->ops = NULL;
	wake_up(&counter->events_wait);

	mutex_unlock(&counter->ops_exist_lock);
}
EXPORT_SYMBOL_NS_GPL(counter_unregister, COUNTER);

static void devm_counter_release(void *counter)
{
	counter_unregister(counter);
}

static void devm_counter_put(void *counter)
{
	counter_put(counter);
}

 
struct counter_device *devm_counter_alloc(struct device *dev, size_t sizeof_priv)
{
	struct counter_device *counter;
	int err;

	counter = counter_alloc(sizeof_priv);
	if (!counter)
		return NULL;

	err = devm_add_action_or_reset(dev, devm_counter_put, counter);
	if (err < 0)
		return NULL;

	return counter;
}
EXPORT_SYMBOL_NS_GPL(devm_counter_alloc, COUNTER);

 
int devm_counter_add(struct device *dev,
		     struct counter_device *const counter)
{
	int err;

	err = counter_add(counter);
	if (err < 0)
		return err;

	return devm_add_action_or_reset(dev, devm_counter_release, counter);
}
EXPORT_SYMBOL_NS_GPL(devm_counter_add, COUNTER);

#define COUNTER_DEV_MAX 256

static int __init counter_init(void)
{
	int err;

	err = bus_register(&counter_bus_type);
	if (err < 0)
		return err;

	err = alloc_chrdev_region(&counter_devt, 0, COUNTER_DEV_MAX,
				  COUNTER_NAME);
	if (err < 0)
		goto err_unregister_bus;

	return 0;

err_unregister_bus:
	bus_unregister(&counter_bus_type);
	return err;
}

static void __exit counter_exit(void)
{
	unregister_chrdev_region(counter_devt, COUNTER_DEV_MAX);
	bus_unregister(&counter_bus_type);
}

subsys_initcall(counter_init);
module_exit(counter_exit);

MODULE_AUTHOR("William Breathitt Gray <vilhelm.gray@gmail.com>");
MODULE_DESCRIPTION("Generic Counter interface");
MODULE_LICENSE("GPL v2");
