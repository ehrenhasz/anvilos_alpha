

#include <linux/acpi.h>
#include <linux/bitmap.h>
#include <linux/compat.h>
#include <linux/debugfs.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/errno.h>
#include <linux/file.h>
#include <linux/fs.h>
#include <linux/idr.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/pinctrl/consumer.h>
#include <linux/seq_file.h>
#include <linux/slab.h>
#include <linux/spinlock.h>

#include <linux/gpio.h>
#include <linux/gpio/driver.h>
#include <linux/gpio/machine.h>

#include <uapi/linux/gpio.h>

#include "gpiolib-acpi.h"
#include "gpiolib-cdev.h"
#include "gpiolib-of.h"
#include "gpiolib-swnode.h"
#include "gpiolib-sysfs.h"
#include "gpiolib.h"

#define CREATE_TRACE_POINTS
#include <trace/events/gpio.h>

 


 
#ifdef	DEBUG
#define	extra_checks	1
#else
#define	extra_checks	0
#endif

 
static DEFINE_IDA(gpio_ida);
static dev_t gpio_devt;
#define GPIO_DEV_MAX 256  

static int gpio_bus_match(struct device *dev, struct device_driver *drv)
{
	struct fwnode_handle *fwnode = dev_fwnode(dev);

	 
	if (fwnode && fwnode->dev != dev)
		return 0;
	return 1;
}

static struct bus_type gpio_bus_type = {
	.name = "gpio",
	.match = gpio_bus_match,
};

 
#define FASTPATH_NGPIO CONFIG_GPIOLIB_FASTPATH_LIMIT

 
DEFINE_SPINLOCK(gpio_lock);

static DEFINE_MUTEX(gpio_lookup_lock);
static LIST_HEAD(gpio_lookup_list);
LIST_HEAD(gpio_devices);

static DEFINE_MUTEX(gpio_machine_hogs_mutex);
static LIST_HEAD(gpio_machine_hogs);

static void gpiochip_free_hogs(struct gpio_chip *gc);
static int gpiochip_add_irqchip(struct gpio_chip *gc,
				struct lock_class_key *lock_key,
				struct lock_class_key *request_key);
static void gpiochip_irqchip_remove(struct gpio_chip *gc);
static int gpiochip_irqchip_init_hw(struct gpio_chip *gc);
static int gpiochip_irqchip_init_valid_mask(struct gpio_chip *gc);
static void gpiochip_irqchip_free_valid_mask(struct gpio_chip *gc);

static bool gpiolib_initialized;

static inline void desc_set_label(struct gpio_desc *d, const char *label)
{
	d->label = label;
}

 
struct gpio_desc *gpio_to_desc(unsigned gpio)
{
	struct gpio_device *gdev;
	unsigned long flags;

	spin_lock_irqsave(&gpio_lock, flags);

	list_for_each_entry(gdev, &gpio_devices, list) {
		if (gdev->base <= gpio &&
		    gdev->base + gdev->ngpio > gpio) {
			spin_unlock_irqrestore(&gpio_lock, flags);
			return &gdev->descs[gpio - gdev->base];
		}
	}

	spin_unlock_irqrestore(&gpio_lock, flags);

	if (!gpio_is_valid(gpio))
		pr_warn("invalid GPIO %d\n", gpio);

	return NULL;
}
EXPORT_SYMBOL_GPL(gpio_to_desc);

 
struct gpio_desc *gpiochip_get_desc(struct gpio_chip *gc,
				    unsigned int hwnum)
{
	struct gpio_device *gdev = gc->gpiodev;

	if (hwnum >= gdev->ngpio)
		return ERR_PTR(-EINVAL);

	return &gdev->descs[hwnum];
}
EXPORT_SYMBOL_GPL(gpiochip_get_desc);

 
int desc_to_gpio(const struct gpio_desc *desc)
{
	return desc->gdev->base + (desc - &desc->gdev->descs[0]);
}
EXPORT_SYMBOL_GPL(desc_to_gpio);


 
struct gpio_chip *gpiod_to_chip(const struct gpio_desc *desc)
{
	if (!desc || !desc->gdev)
		return NULL;
	return desc->gdev->chip;
}
EXPORT_SYMBOL_GPL(gpiod_to_chip);

 
static int gpiochip_find_base(int ngpio)
{
	struct gpio_device *gdev;
	int base = GPIO_DYNAMIC_BASE;

	list_for_each_entry(gdev, &gpio_devices, list) {
		 
		if (gdev->base >= base + ngpio)
			break;
		 
		base = gdev->base + gdev->ngpio;
		if (base < GPIO_DYNAMIC_BASE)
			base = GPIO_DYNAMIC_BASE;
	}

	if (gpio_is_valid(base)) {
		pr_debug("%s: found new base at %d\n", __func__, base);
		return base;
	} else {
		pr_err("%s: cannot find free range\n", __func__);
		return -ENOSPC;
	}
}

 
int gpiod_get_direction(struct gpio_desc *desc)
{
	struct gpio_chip *gc;
	unsigned int offset;
	int ret;

	gc = gpiod_to_chip(desc);
	offset = gpio_chip_hwgpio(desc);

	 
	if (test_bit(FLAG_OPEN_DRAIN, &desc->flags) &&
	    test_bit(FLAG_IS_OUT, &desc->flags))
		return 0;

	if (!gc->get_direction)
		return -ENOTSUPP;

	ret = gc->get_direction(gc, offset);
	if (ret < 0)
		return ret;

	 
	if (ret > 0)
		ret = 1;

	assign_bit(FLAG_IS_OUT, &desc->flags, !ret);

	return ret;
}
EXPORT_SYMBOL_GPL(gpiod_get_direction);

 
static int gpiodev_add_to_list(struct gpio_device *gdev)
{
	struct gpio_device *prev, *next;

	if (list_empty(&gpio_devices)) {
		 
		list_add_tail(&gdev->list, &gpio_devices);
		return 0;
	}

	next = list_first_entry(&gpio_devices, struct gpio_device, list);
	if (gdev->base + gdev->ngpio <= next->base) {
		 
		list_add(&gdev->list, &gpio_devices);
		return 0;
	}

	prev = list_last_entry(&gpio_devices, struct gpio_device, list);
	if (prev->base + prev->ngpio <= gdev->base) {
		 
		list_add_tail(&gdev->list, &gpio_devices);
		return 0;
	}

	list_for_each_entry_safe(prev, next, &gpio_devices, list) {
		 
		if (&next->list == &gpio_devices)
			break;

		 
		if (prev->base + prev->ngpio <= gdev->base
				&& gdev->base + gdev->ngpio <= next->base) {
			list_add(&gdev->list, &prev->list);
			return 0;
		}
	}

	return -EBUSY;
}

 
static struct gpio_desc *gpio_name_to_desc(const char * const name)
{
	struct gpio_device *gdev;
	unsigned long flags;

	if (!name)
		return NULL;

	spin_lock_irqsave(&gpio_lock, flags);

	list_for_each_entry(gdev, &gpio_devices, list) {
		struct gpio_desc *desc;

		for_each_gpio_desc(gdev->chip, desc) {
			if (desc->name && !strcmp(desc->name, name)) {
				spin_unlock_irqrestore(&gpio_lock, flags);
				return desc;
			}
		}
	}

	spin_unlock_irqrestore(&gpio_lock, flags);

	return NULL;
}

 
static int gpiochip_set_desc_names(struct gpio_chip *gc)
{
	struct gpio_device *gdev = gc->gpiodev;
	int i;

	 
	for (i = 0; i != gc->ngpio; ++i) {
		struct gpio_desc *gpio;

		gpio = gpio_name_to_desc(gc->names[i]);
		if (gpio)
			dev_warn(&gdev->dev,
				 "Detected name collision for GPIO name '%s'\n",
				 gc->names[i]);
	}

	 
	for (i = 0; i != gc->ngpio; ++i)
		gdev->descs[i].name = gc->names[i];

	return 0;
}

 
static int gpiochip_set_names(struct gpio_chip *chip)
{
	struct gpio_device *gdev = chip->gpiodev;
	struct device *dev = &gdev->dev;
	const char **names;
	int ret, i;
	int count;

	count = device_property_string_array_count(dev, "gpio-line-names");
	if (count < 0)
		return 0;

	 
	if (count <= chip->offset) {
		dev_warn(dev, "gpio-line-names too short (length %d), cannot map names for the gpiochip at offset %u\n",
			 count, chip->offset);
		return 0;
	}

	names = kcalloc(count, sizeof(*names), GFP_KERNEL);
	if (!names)
		return -ENOMEM;

	ret = device_property_read_string_array(dev, "gpio-line-names",
						names, count);
	if (ret < 0) {
		dev_warn(dev, "failed to read GPIO line names\n");
		kfree(names);
		return ret;
	}

	 
	count = (count > chip->offset) ? count - chip->offset : count;
	if (count > chip->ngpio)
		count = chip->ngpio;

	for (i = 0; i < count; i++) {
		 
		if (names[chip->offset + i] && names[chip->offset + i][0])
			gdev->descs[i].name = names[chip->offset + i];
	}

	kfree(names);

	return 0;
}

static unsigned long *gpiochip_allocate_mask(struct gpio_chip *gc)
{
	unsigned long *p;

	p = bitmap_alloc(gc->ngpio, GFP_KERNEL);
	if (!p)
		return NULL;

	 
	bitmap_fill(p, gc->ngpio);

	return p;
}

static void gpiochip_free_mask(unsigned long **p)
{
	bitmap_free(*p);
	*p = NULL;
}

static unsigned int gpiochip_count_reserved_ranges(struct gpio_chip *gc)
{
	struct device *dev = &gc->gpiodev->dev;
	int size;

	 
	size = device_property_count_u32(dev, "gpio-reserved-ranges");
	if (size > 0 && size % 2 == 0)
		return size;

	return 0;
}

static int gpiochip_apply_reserved_ranges(struct gpio_chip *gc)
{
	struct device *dev = &gc->gpiodev->dev;
	unsigned int size;
	u32 *ranges;
	int ret;

	size = gpiochip_count_reserved_ranges(gc);
	if (size == 0)
		return 0;

	ranges = kmalloc_array(size, sizeof(*ranges), GFP_KERNEL);
	if (!ranges)
		return -ENOMEM;

	ret = device_property_read_u32_array(dev, "gpio-reserved-ranges",
					     ranges, size);
	if (ret) {
		kfree(ranges);
		return ret;
	}

	while (size) {
		u32 count = ranges[--size];
		u32 start = ranges[--size];

		if (start >= gc->ngpio || start + count > gc->ngpio)
			continue;

		bitmap_clear(gc->valid_mask, start, count);
	}

	kfree(ranges);
	return 0;
}

static int gpiochip_init_valid_mask(struct gpio_chip *gc)
{
	int ret;

	if (!(gpiochip_count_reserved_ranges(gc) || gc->init_valid_mask))
		return 0;

	gc->valid_mask = gpiochip_allocate_mask(gc);
	if (!gc->valid_mask)
		return -ENOMEM;

	ret = gpiochip_apply_reserved_ranges(gc);
	if (ret)
		return ret;

	if (gc->init_valid_mask)
		return gc->init_valid_mask(gc,
					   gc->valid_mask,
					   gc->ngpio);

	return 0;
}

static void gpiochip_free_valid_mask(struct gpio_chip *gc)
{
	gpiochip_free_mask(&gc->valid_mask);
}

static int gpiochip_add_pin_ranges(struct gpio_chip *gc)
{
	 
	if (device_property_present(&gc->gpiodev->dev, "gpio-ranges"))
		return 0;

	if (gc->add_pin_ranges)
		return gc->add_pin_ranges(gc);

	return 0;
}

bool gpiochip_line_is_valid(const struct gpio_chip *gc,
				unsigned int offset)
{
	 
	if (likely(!gc->valid_mask))
		return true;
	return test_bit(offset, gc->valid_mask);
}
EXPORT_SYMBOL_GPL(gpiochip_line_is_valid);

static void gpiodev_release(struct device *dev)
{
	struct gpio_device *gdev = to_gpio_device(dev);
	unsigned long flags;

	spin_lock_irqsave(&gpio_lock, flags);
	list_del(&gdev->list);
	spin_unlock_irqrestore(&gpio_lock, flags);

	ida_free(&gpio_ida, gdev->id);
	kfree_const(gdev->label);
	kfree(gdev->descs);
	kfree(gdev);
}

#ifdef CONFIG_GPIO_CDEV
#define gcdev_register(gdev, devt)	gpiolib_cdev_register((gdev), (devt))
#define gcdev_unregister(gdev)		gpiolib_cdev_unregister((gdev))
#else
 
#define gcdev_register(gdev, devt)	device_add(&(gdev)->dev)
#define gcdev_unregister(gdev)		device_del(&(gdev)->dev)
#endif

static int gpiochip_setup_dev(struct gpio_device *gdev)
{
	struct fwnode_handle *fwnode = dev_fwnode(&gdev->dev);
	int ret;

	 
	if (fwnode && !fwnode->dev)
		fwnode_dev_initialized(fwnode, false);

	ret = gcdev_register(gdev, gpio_devt);
	if (ret)
		return ret;

	 
	gdev->dev.release = gpiodev_release;

	ret = gpiochip_sysfs_register(gdev);
	if (ret)
		goto err_remove_device;

	dev_dbg(&gdev->dev, "registered GPIOs %d to %d on %s\n", gdev->base,
		gdev->base + gdev->ngpio - 1, gdev->chip->label ? : "generic");

	return 0;

err_remove_device:
	gcdev_unregister(gdev);
	return ret;
}

static void gpiochip_machine_hog(struct gpio_chip *gc, struct gpiod_hog *hog)
{
	struct gpio_desc *desc;
	int rv;

	desc = gpiochip_get_desc(gc, hog->chip_hwnum);
	if (IS_ERR(desc)) {
		chip_err(gc, "%s: unable to get GPIO desc: %ld\n", __func__,
			 PTR_ERR(desc));
		return;
	}

	if (test_bit(FLAG_IS_HOGGED, &desc->flags))
		return;

	rv = gpiod_hog(desc, hog->line_name, hog->lflags, hog->dflags);
	if (rv)
		gpiod_err(desc, "%s: unable to hog GPIO line (%s:%u): %d\n",
			  __func__, gc->label, hog->chip_hwnum, rv);
}

static void machine_gpiochip_add(struct gpio_chip *gc)
{
	struct gpiod_hog *hog;

	mutex_lock(&gpio_machine_hogs_mutex);

	list_for_each_entry(hog, &gpio_machine_hogs, list) {
		if (!strcmp(gc->label, hog->chip_label))
			gpiochip_machine_hog(gc, hog);
	}

	mutex_unlock(&gpio_machine_hogs_mutex);
}

static void gpiochip_setup_devs(void)
{
	struct gpio_device *gdev;
	int ret;

	list_for_each_entry(gdev, &gpio_devices, list) {
		ret = gpiochip_setup_dev(gdev);
		if (ret)
			dev_err(&gdev->dev,
				"Failed to initialize gpio device (%d)\n", ret);
	}
}

static void gpiochip_set_data(struct gpio_chip *gc, void *data)
{
	gc->gpiodev->data = data;
}

 
void *gpiochip_get_data(struct gpio_chip *gc)
{
	return gc->gpiodev->data;
}
EXPORT_SYMBOL_GPL(gpiochip_get_data);

int gpiochip_get_ngpios(struct gpio_chip *gc, struct device *dev)
{
	u32 ngpios = gc->ngpio;
	int ret;

	if (ngpios == 0) {
		ret = device_property_read_u32(dev, "ngpios", &ngpios);
		if (ret == -ENODATA)
			 
			ngpios = 0;
		else if (ret)
			return ret;

		gc->ngpio = ngpios;
	}

	if (gc->ngpio == 0) {
		chip_err(gc, "tried to insert a GPIO chip with zero lines\n");
		return -EINVAL;
	}

	if (gc->ngpio > FASTPATH_NGPIO)
		chip_warn(gc, "line cnt %u is greater than fast path cnt %u\n",
			gc->ngpio, FASTPATH_NGPIO);

	return 0;
}
EXPORT_SYMBOL_GPL(gpiochip_get_ngpios);

int gpiochip_add_data_with_key(struct gpio_chip *gc, void *data,
			       struct lock_class_key *lock_key,
			       struct lock_class_key *request_key)
{
	struct gpio_device *gdev;
	unsigned long flags;
	unsigned int i;
	int base = 0;
	int ret = 0;

	 
	gdev = kzalloc(sizeof(*gdev), GFP_KERNEL);
	if (!gdev)
		return -ENOMEM;
	gdev->dev.bus = &gpio_bus_type;
	gdev->dev.parent = gc->parent;
	gdev->chip = gc;

	gc->gpiodev = gdev;
	gpiochip_set_data(gc, data);

	 
	if (gc->fwnode)
		device_set_node(&gdev->dev, gc->fwnode);
	else if (gc->parent)
		device_set_node(&gdev->dev, dev_fwnode(gc->parent));

	gdev->id = ida_alloc(&gpio_ida, GFP_KERNEL);
	if (gdev->id < 0) {
		ret = gdev->id;
		goto err_free_gdev;
	}

	ret = dev_set_name(&gdev->dev, GPIOCHIP_NAME "%d", gdev->id);
	if (ret)
		goto err_free_ida;

	device_initialize(&gdev->dev);
	if (gc->parent && gc->parent->driver)
		gdev->owner = gc->parent->driver->owner;
	else if (gc->owner)
		 
		gdev->owner = gc->owner;
	else
		gdev->owner = THIS_MODULE;

	ret = gpiochip_get_ngpios(gc, &gdev->dev);
	if (ret)
		goto err_free_dev_name;

	gdev->descs = kcalloc(gc->ngpio, sizeof(*gdev->descs), GFP_KERNEL);
	if (!gdev->descs) {
		ret = -ENOMEM;
		goto err_free_dev_name;
	}

	gdev->label = kstrdup_const(gc->label ?: "unknown", GFP_KERNEL);
	if (!gdev->label) {
		ret = -ENOMEM;
		goto err_free_descs;
	}

	gdev->ngpio = gc->ngpio;

	spin_lock_irqsave(&gpio_lock, flags);

	 
	base = gc->base;
	if (base < 0) {
		base = gpiochip_find_base(gc->ngpio);
		if (base < 0) {
			spin_unlock_irqrestore(&gpio_lock, flags);
			ret = base;
			base = 0;
			goto err_free_label;
		}
		 
		gc->base = base;
	} else {
		dev_warn(&gdev->dev,
			 "Static allocation of GPIO base is deprecated, use dynamic allocation.\n");
	}
	gdev->base = base;

	ret = gpiodev_add_to_list(gdev);
	if (ret) {
		spin_unlock_irqrestore(&gpio_lock, flags);
		chip_err(gc, "GPIO integer space overlap, cannot add chip\n");
		goto err_free_label;
	}

	for (i = 0; i < gc->ngpio; i++)
		gdev->descs[i].gdev = gdev;

	spin_unlock_irqrestore(&gpio_lock, flags);

	BLOCKING_INIT_NOTIFIER_HEAD(&gdev->line_state_notifier);
	BLOCKING_INIT_NOTIFIER_HEAD(&gdev->device_notifier);
	init_rwsem(&gdev->sem);

#ifdef CONFIG_PINCTRL
	INIT_LIST_HEAD(&gdev->pin_ranges);
#endif

	if (gc->names) {
		ret = gpiochip_set_desc_names(gc);
		if (ret)
			goto err_remove_from_list;
	}
	ret = gpiochip_set_names(gc);
	if (ret)
		goto err_remove_from_list;

	ret = gpiochip_init_valid_mask(gc);
	if (ret)
		goto err_remove_from_list;

	ret = of_gpiochip_add(gc);
	if (ret)
		goto err_free_gpiochip_mask;

	for (i = 0; i < gc->ngpio; i++) {
		struct gpio_desc *desc = &gdev->descs[i];

		if (gc->get_direction && gpiochip_line_is_valid(gc, i)) {
			assign_bit(FLAG_IS_OUT,
				   &desc->flags, !gc->get_direction(gc, i));
		} else {
			assign_bit(FLAG_IS_OUT,
				   &desc->flags, !gc->direction_input);
		}
	}

	ret = gpiochip_add_pin_ranges(gc);
	if (ret)
		goto err_remove_of_chip;

	acpi_gpiochip_add(gc);

	machine_gpiochip_add(gc);

	ret = gpiochip_irqchip_init_valid_mask(gc);
	if (ret)
		goto err_remove_acpi_chip;

	ret = gpiochip_irqchip_init_hw(gc);
	if (ret)
		goto err_remove_acpi_chip;

	ret = gpiochip_add_irqchip(gc, lock_key, request_key);
	if (ret)
		goto err_remove_irqchip_mask;

	 
	if (gpiolib_initialized) {
		ret = gpiochip_setup_dev(gdev);
		if (ret)
			goto err_remove_irqchip;
	}
	return 0;

err_remove_irqchip:
	gpiochip_irqchip_remove(gc);
err_remove_irqchip_mask:
	gpiochip_irqchip_free_valid_mask(gc);
err_remove_acpi_chip:
	acpi_gpiochip_remove(gc);
err_remove_of_chip:
	gpiochip_free_hogs(gc);
	of_gpiochip_remove(gc);
err_free_gpiochip_mask:
	gpiochip_remove_pin_ranges(gc);
	gpiochip_free_valid_mask(gc);
	if (gdev->dev.release) {
		 
		gpio_device_put(gdev);
		goto err_print_message;
	}
err_remove_from_list:
	spin_lock_irqsave(&gpio_lock, flags);
	list_del(&gdev->list);
	spin_unlock_irqrestore(&gpio_lock, flags);
err_free_label:
	kfree_const(gdev->label);
err_free_descs:
	kfree(gdev->descs);
err_free_dev_name:
	kfree(dev_name(&gdev->dev));
err_free_ida:
	ida_free(&gpio_ida, gdev->id);
err_free_gdev:
	kfree(gdev);
err_print_message:
	 
	if (ret != -EPROBE_DEFER) {
		pr_err("%s: GPIOs %d..%d (%s) failed to register, %d\n", __func__,
		       base, base + (int)gc->ngpio - 1,
		       gc->label ? : "generic", ret);
	}
	return ret;
}
EXPORT_SYMBOL_GPL(gpiochip_add_data_with_key);

 
void gpiochip_remove(struct gpio_chip *gc)
{
	struct gpio_device *gdev = gc->gpiodev;
	unsigned long	flags;
	unsigned int	i;

	down_write(&gdev->sem);

	 
	gpiochip_sysfs_unregister(gdev);
	gpiochip_free_hogs(gc);
	 
	gdev->chip = NULL;
	gpiochip_irqchip_remove(gc);
	acpi_gpiochip_remove(gc);
	of_gpiochip_remove(gc);
	gpiochip_remove_pin_ranges(gc);
	gpiochip_free_valid_mask(gc);
	 
	gpiochip_set_data(gc, NULL);

	spin_lock_irqsave(&gpio_lock, flags);
	for (i = 0; i < gdev->ngpio; i++) {
		if (gpiochip_is_requested(gc, i))
			break;
	}
	spin_unlock_irqrestore(&gpio_lock, flags);

	if (i != gdev->ngpio)
		dev_crit(&gdev->dev,
			 "REMOVING GPIOCHIP WITH GPIOS STILL REQUESTED\n");

	 
	gcdev_unregister(gdev);
	up_write(&gdev->sem);
	gpio_device_put(gdev);
}
EXPORT_SYMBOL_GPL(gpiochip_remove);

 
struct gpio_chip *gpiochip_find(void *data,
				int (*match)(struct gpio_chip *gc,
					     void *data))
{
	struct gpio_device *gdev;
	struct gpio_chip *gc = NULL;

	gdev = gpio_device_find(data, match);
	if (gdev) {
		gc = gdev->chip;
		gpio_device_put(gdev);
	}

	return gc;
}
EXPORT_SYMBOL_GPL(gpiochip_find);

 
struct gpio_device *gpio_device_find(void *data,
				     int (*match)(struct gpio_chip *gc,
						  void *data))
{
	struct gpio_device *gdev;

	 
	might_sleep();

	guard(spinlock_irqsave)(&gpio_lock);

	list_for_each_entry(gdev, &gpio_devices, list) {
		if (gdev->chip && match(gdev->chip, data))
			return gpio_device_get(gdev);
	}

	return NULL;
}
EXPORT_SYMBOL_GPL(gpio_device_find);

static int gpiochip_match_name(struct gpio_chip *gc, void *data)
{
	const char *name = data;

	return !strcmp(gc->label, name);
}

static struct gpio_chip *find_chip_by_name(const char *name)
{
	return gpiochip_find((void *)name, gpiochip_match_name);
}

 
struct gpio_device *gpio_device_get(struct gpio_device *gdev)
{
	return to_gpio_device(get_device(&gdev->dev));
}
EXPORT_SYMBOL_GPL(gpio_device_get);

 
void gpio_device_put(struct gpio_device *gdev)
{
	put_device(&gdev->dev);
}
EXPORT_SYMBOL_GPL(gpio_device_put);

#ifdef CONFIG_GPIOLIB_IRQCHIP

 

static int gpiochip_irqchip_init_hw(struct gpio_chip *gc)
{
	struct gpio_irq_chip *girq = &gc->irq;

	if (!girq->init_hw)
		return 0;

	return girq->init_hw(gc);
}

static int gpiochip_irqchip_init_valid_mask(struct gpio_chip *gc)
{
	struct gpio_irq_chip *girq = &gc->irq;

	if (!girq->init_valid_mask)
		return 0;

	girq->valid_mask = gpiochip_allocate_mask(gc);
	if (!girq->valid_mask)
		return -ENOMEM;

	girq->init_valid_mask(gc, girq->valid_mask, gc->ngpio);

	return 0;
}

static void gpiochip_irqchip_free_valid_mask(struct gpio_chip *gc)
{
	gpiochip_free_mask(&gc->irq.valid_mask);
}

bool gpiochip_irqchip_irq_valid(const struct gpio_chip *gc,
				unsigned int offset)
{
	if (!gpiochip_line_is_valid(gc, offset))
		return false;
	 
	if (likely(!gc->irq.valid_mask))
		return true;
	return test_bit(offset, gc->irq.valid_mask);
}
EXPORT_SYMBOL_GPL(gpiochip_irqchip_irq_valid);

#ifdef CONFIG_IRQ_DOMAIN_HIERARCHY

 
static void gpiochip_set_hierarchical_irqchip(struct gpio_chip *gc,
					      struct irq_chip *irqchip)
{
	 
	if (is_of_node(gc->irq.fwnode))
		return;

	 
	if (is_fwnode_irqchip(gc->irq.fwnode)) {
		int i;
		int ret;

		for (i = 0; i < gc->ngpio; i++) {
			struct irq_fwspec fwspec;
			unsigned int parent_hwirq;
			unsigned int parent_type;
			struct gpio_irq_chip *girq = &gc->irq;

			 
			ret = girq->child_to_parent_hwirq(gc, i,
							  IRQ_TYPE_EDGE_RISING,
							  &parent_hwirq,
							  &parent_type);
			if (ret) {
				chip_err(gc, "skip set-up on hwirq %d\n",
					 i);
				continue;
			}

			fwspec.fwnode = gc->irq.fwnode;
			 
			fwspec.param[0] = girq->child_offset_to_irq(gc, i);
			 
			fwspec.param[1] = IRQ_TYPE_EDGE_RISING;
			fwspec.param_count = 2;
			ret = irq_domain_alloc_irqs(gc->irq.domain, 1,
						    NUMA_NO_NODE, &fwspec);
			if (ret < 0) {
				chip_err(gc,
					 "can not allocate irq for GPIO line %d parent hwirq %d in hierarchy domain: %d\n",
					 i, parent_hwirq,
					 ret);
			}
		}
	}

	chip_err(gc, "%s unknown fwnode type proceed anyway\n", __func__);

	return;
}

static int gpiochip_hierarchy_irq_domain_translate(struct irq_domain *d,
						   struct irq_fwspec *fwspec,
						   unsigned long *hwirq,
						   unsigned int *type)
{
	 
	if (is_of_node(fwspec->fwnode) && fwspec->param_count == 2) {
		return irq_domain_translate_twocell(d, fwspec, hwirq, type);
	}

	 
	if (is_fwnode_irqchip(fwspec->fwnode)) {
		int ret;

		ret = irq_domain_translate_twocell(d, fwspec, hwirq, type);
		if (ret)
			return ret;
		WARN_ON(*type == IRQ_TYPE_NONE);
		return 0;
	}
	return -EINVAL;
}

static int gpiochip_hierarchy_irq_domain_alloc(struct irq_domain *d,
					       unsigned int irq,
					       unsigned int nr_irqs,
					       void *data)
{
	struct gpio_chip *gc = d->host_data;
	irq_hw_number_t hwirq;
	unsigned int type = IRQ_TYPE_NONE;
	struct irq_fwspec *fwspec = data;
	union gpio_irq_fwspec gpio_parent_fwspec = {};
	unsigned int parent_hwirq;
	unsigned int parent_type;
	struct gpio_irq_chip *girq = &gc->irq;
	int ret;

	 
	WARN_ON(nr_irqs != 1);

	ret = gc->irq.child_irq_domain_ops.translate(d, fwspec, &hwirq, &type);
	if (ret)
		return ret;

	chip_dbg(gc, "allocate IRQ %d, hwirq %lu\n", irq, hwirq);

	ret = girq->child_to_parent_hwirq(gc, hwirq, type,
					  &parent_hwirq, &parent_type);
	if (ret) {
		chip_err(gc, "can't look up hwirq %lu\n", hwirq);
		return ret;
	}
	chip_dbg(gc, "found parent hwirq %u\n", parent_hwirq);

	 
	irq_domain_set_info(d,
			    irq,
			    hwirq,
			    gc->irq.chip,
			    gc,
			    girq->handler,
			    NULL, NULL);
	irq_set_probe(irq);

	 
	ret = girq->populate_parent_alloc_arg(gc, &gpio_parent_fwspec,
					      parent_hwirq, parent_type);
	if (ret)
		return ret;

	chip_dbg(gc, "alloc_irqs_parent for %d parent hwirq %d\n",
		  irq, parent_hwirq);
	irq_set_lockdep_class(irq, gc->irq.lock_key, gc->irq.request_key);
	ret = irq_domain_alloc_irqs_parent(d, irq, 1, &gpio_parent_fwspec);
	 
	if (irq_domain_is_msi(d->parent) && (ret == -EEXIST))
		ret = 0;
	if (ret)
		chip_err(gc,
			 "failed to allocate parent hwirq %d for hwirq %lu\n",
			 parent_hwirq, hwirq);

	return ret;
}

static unsigned int gpiochip_child_offset_to_irq_noop(struct gpio_chip *gc,
						      unsigned int offset)
{
	return offset;
}

static void gpiochip_hierarchy_setup_domain_ops(struct irq_domain_ops *ops)
{
	ops->activate = gpiochip_irq_domain_activate;
	ops->deactivate = gpiochip_irq_domain_deactivate;
	ops->alloc = gpiochip_hierarchy_irq_domain_alloc;

	 
	if (!ops->translate)
		ops->translate = gpiochip_hierarchy_irq_domain_translate;
	if (!ops->free)
		ops->free = irq_domain_free_irqs_common;
}

static struct irq_domain *gpiochip_hierarchy_create_domain(struct gpio_chip *gc)
{
	struct irq_domain *domain;

	if (!gc->irq.child_to_parent_hwirq ||
	    !gc->irq.fwnode) {
		chip_err(gc, "missing irqdomain vital data\n");
		return ERR_PTR(-EINVAL);
	}

	if (!gc->irq.child_offset_to_irq)
		gc->irq.child_offset_to_irq = gpiochip_child_offset_to_irq_noop;

	if (!gc->irq.populate_parent_alloc_arg)
		gc->irq.populate_parent_alloc_arg =
			gpiochip_populate_parent_fwspec_twocell;

	gpiochip_hierarchy_setup_domain_ops(&gc->irq.child_irq_domain_ops);

	domain = irq_domain_create_hierarchy(
		gc->irq.parent_domain,
		0,
		gc->ngpio,
		gc->irq.fwnode,
		&gc->irq.child_irq_domain_ops,
		gc);

	if (!domain)
		return ERR_PTR(-ENOMEM);

	gpiochip_set_hierarchical_irqchip(gc, gc->irq.chip);

	return domain;
}

static bool gpiochip_hierarchy_is_hierarchical(struct gpio_chip *gc)
{
	return !!gc->irq.parent_domain;
}

int gpiochip_populate_parent_fwspec_twocell(struct gpio_chip *gc,
					    union gpio_irq_fwspec *gfwspec,
					    unsigned int parent_hwirq,
					    unsigned int parent_type)
{
	struct irq_fwspec *fwspec = &gfwspec->fwspec;

	fwspec->fwnode = gc->irq.parent_domain->fwnode;
	fwspec->param_count = 2;
	fwspec->param[0] = parent_hwirq;
	fwspec->param[1] = parent_type;

	return 0;
}
EXPORT_SYMBOL_GPL(gpiochip_populate_parent_fwspec_twocell);

int gpiochip_populate_parent_fwspec_fourcell(struct gpio_chip *gc,
					     union gpio_irq_fwspec *gfwspec,
					     unsigned int parent_hwirq,
					     unsigned int parent_type)
{
	struct irq_fwspec *fwspec = &gfwspec->fwspec;

	fwspec->fwnode = gc->irq.parent_domain->fwnode;
	fwspec->param_count = 4;
	fwspec->param[0] = 0;
	fwspec->param[1] = parent_hwirq;
	fwspec->param[2] = 0;
	fwspec->param[3] = parent_type;

	return 0;
}
EXPORT_SYMBOL_GPL(gpiochip_populate_parent_fwspec_fourcell);

#else

static struct irq_domain *gpiochip_hierarchy_create_domain(struct gpio_chip *gc)
{
	return ERR_PTR(-EINVAL);
}

static bool gpiochip_hierarchy_is_hierarchical(struct gpio_chip *gc)
{
	return false;
}

#endif  

 
int gpiochip_irq_map(struct irq_domain *d, unsigned int irq, irq_hw_number_t hwirq)
{
	struct gpio_chip *gc = d->host_data;
	int ret = 0;

	if (!gpiochip_irqchip_irq_valid(gc, hwirq))
		return -ENXIO;

	irq_set_chip_data(irq, gc);
	 
	irq_set_lockdep_class(irq, gc->irq.lock_key, gc->irq.request_key);
	irq_set_chip_and_handler(irq, gc->irq.chip, gc->irq.handler);
	 
	if (gc->irq.threaded)
		irq_set_nested_thread(irq, 1);
	irq_set_noprobe(irq);

	if (gc->irq.num_parents == 1)
		ret = irq_set_parent(irq, gc->irq.parents[0]);
	else if (gc->irq.map)
		ret = irq_set_parent(irq, gc->irq.map[hwirq]);

	if (ret < 0)
		return ret;

	 
	if (gc->irq.default_type != IRQ_TYPE_NONE)
		irq_set_irq_type(irq, gc->irq.default_type);

	return 0;
}
EXPORT_SYMBOL_GPL(gpiochip_irq_map);

void gpiochip_irq_unmap(struct irq_domain *d, unsigned int irq)
{
	struct gpio_chip *gc = d->host_data;

	if (gc->irq.threaded)
		irq_set_nested_thread(irq, 0);
	irq_set_chip_and_handler(irq, NULL, NULL);
	irq_set_chip_data(irq, NULL);
}
EXPORT_SYMBOL_GPL(gpiochip_irq_unmap);

static const struct irq_domain_ops gpiochip_domain_ops = {
	.map	= gpiochip_irq_map,
	.unmap	= gpiochip_irq_unmap,
	 
	.xlate	= irq_domain_xlate_twocell,
};

static struct irq_domain *gpiochip_simple_create_domain(struct gpio_chip *gc)
{
	struct fwnode_handle *fwnode = dev_fwnode(&gc->gpiodev->dev);
	struct irq_domain *domain;

	domain = irq_domain_create_simple(fwnode, gc->ngpio, gc->irq.first,
					  &gpiochip_domain_ops, gc);
	if (!domain)
		return ERR_PTR(-EINVAL);

	return domain;
}

 
 
int gpiochip_irq_domain_activate(struct irq_domain *domain,
				 struct irq_data *data, bool reserve)
{
	struct gpio_chip *gc = domain->host_data;
	unsigned int hwirq = irqd_to_hwirq(data);

	return gpiochip_lock_as_irq(gc, hwirq);
}
EXPORT_SYMBOL_GPL(gpiochip_irq_domain_activate);

 
void gpiochip_irq_domain_deactivate(struct irq_domain *domain,
				    struct irq_data *data)
{
	struct gpio_chip *gc = domain->host_data;
	unsigned int hwirq = irqd_to_hwirq(data);

	return gpiochip_unlock_as_irq(gc, hwirq);
}
EXPORT_SYMBOL_GPL(gpiochip_irq_domain_deactivate);

static int gpiochip_to_irq(struct gpio_chip *gc, unsigned int offset)
{
	struct irq_domain *domain = gc->irq.domain;

#ifdef CONFIG_GPIOLIB_IRQCHIP
	 
	if (!gc->irq.initialized)
		return -EPROBE_DEFER;
#endif

	if (!gpiochip_irqchip_irq_valid(gc, offset))
		return -ENXIO;

#ifdef CONFIG_IRQ_DOMAIN_HIERARCHY
	if (irq_domain_is_hierarchy(domain)) {
		struct irq_fwspec spec;

		spec.fwnode = domain->fwnode;
		spec.param_count = 2;
		spec.param[0] = gc->irq.child_offset_to_irq(gc, offset);
		spec.param[1] = IRQ_TYPE_NONE;

		return irq_create_fwspec_mapping(&spec);
	}
#endif

	return irq_create_mapping(domain, offset);
}

int gpiochip_irq_reqres(struct irq_data *d)
{
	struct gpio_chip *gc = irq_data_get_irq_chip_data(d);
	unsigned int hwirq = irqd_to_hwirq(d);

	return gpiochip_reqres_irq(gc, hwirq);
}
EXPORT_SYMBOL(gpiochip_irq_reqres);

void gpiochip_irq_relres(struct irq_data *d)
{
	struct gpio_chip *gc = irq_data_get_irq_chip_data(d);
	unsigned int hwirq = irqd_to_hwirq(d);

	gpiochip_relres_irq(gc, hwirq);
}
EXPORT_SYMBOL(gpiochip_irq_relres);

static void gpiochip_irq_mask(struct irq_data *d)
{
	struct gpio_chip *gc = irq_data_get_irq_chip_data(d);
	unsigned int hwirq = irqd_to_hwirq(d);

	if (gc->irq.irq_mask)
		gc->irq.irq_mask(d);
	gpiochip_disable_irq(gc, hwirq);
}

static void gpiochip_irq_unmask(struct irq_data *d)
{
	struct gpio_chip *gc = irq_data_get_irq_chip_data(d);
	unsigned int hwirq = irqd_to_hwirq(d);

	gpiochip_enable_irq(gc, hwirq);
	if (gc->irq.irq_unmask)
		gc->irq.irq_unmask(d);
}

static void gpiochip_irq_enable(struct irq_data *d)
{
	struct gpio_chip *gc = irq_data_get_irq_chip_data(d);
	unsigned int hwirq = irqd_to_hwirq(d);

	gpiochip_enable_irq(gc, hwirq);
	gc->irq.irq_enable(d);
}

static void gpiochip_irq_disable(struct irq_data *d)
{
	struct gpio_chip *gc = irq_data_get_irq_chip_data(d);
	unsigned int hwirq = irqd_to_hwirq(d);

	gc->irq.irq_disable(d);
	gpiochip_disable_irq(gc, hwirq);
}

static void gpiochip_set_irq_hooks(struct gpio_chip *gc)
{
	struct irq_chip *irqchip = gc->irq.chip;

	if (irqchip->flags & IRQCHIP_IMMUTABLE)
		return;

	chip_warn(gc, "not an immutable chip, please consider fixing it!\n");

	if (!irqchip->irq_request_resources &&
	    !irqchip->irq_release_resources) {
		irqchip->irq_request_resources = gpiochip_irq_reqres;
		irqchip->irq_release_resources = gpiochip_irq_relres;
	}
	if (WARN_ON(gc->irq.irq_enable))
		return;
	 
	if (irqchip->irq_enable == gpiochip_irq_enable ||
		irqchip->irq_mask == gpiochip_irq_mask) {
		 
		chip_info(gc,
			  "detected irqchip that is shared with multiple gpiochips: please fix the driver.\n");
		return;
	}

	if (irqchip->irq_disable) {
		gc->irq.irq_disable = irqchip->irq_disable;
		irqchip->irq_disable = gpiochip_irq_disable;
	} else {
		gc->irq.irq_mask = irqchip->irq_mask;
		irqchip->irq_mask = gpiochip_irq_mask;
	}

	if (irqchip->irq_enable) {
		gc->irq.irq_enable = irqchip->irq_enable;
		irqchip->irq_enable = gpiochip_irq_enable;
	} else {
		gc->irq.irq_unmask = irqchip->irq_unmask;
		irqchip->irq_unmask = gpiochip_irq_unmask;
	}
}

static int gpiochip_irqchip_add_allocated_domain(struct gpio_chip *gc,
						 struct irq_domain *domain,
						 bool allocated_externally)
{
	if (!domain)
		return -EINVAL;

	if (gc->to_irq)
		chip_warn(gc, "to_irq is redefined in %s and you shouldn't rely on it\n", __func__);

	gc->to_irq = gpiochip_to_irq;
	gc->irq.domain = domain;
	gc->irq.domain_is_allocated_externally = allocated_externally;

	 
	barrier();

	gc->irq.initialized = true;

	return 0;
}

 
static int gpiochip_add_irqchip(struct gpio_chip *gc,
				struct lock_class_key *lock_key,
				struct lock_class_key *request_key)
{
	struct fwnode_handle *fwnode = dev_fwnode(&gc->gpiodev->dev);
	struct irq_chip *irqchip = gc->irq.chip;
	struct irq_domain *domain;
	unsigned int type;
	unsigned int i;
	int ret;

	if (!irqchip)
		return 0;

	if (gc->irq.parent_handler && gc->can_sleep) {
		chip_err(gc, "you cannot have chained interrupts on a chip that may sleep\n");
		return -EINVAL;
	}

	type = gc->irq.default_type;

	 
	if (WARN(fwnode && type != IRQ_TYPE_NONE,
		 "%pfw: Ignoring %u default trigger\n", fwnode, type))
		type = IRQ_TYPE_NONE;

	gc->irq.default_type = type;
	gc->irq.lock_key = lock_key;
	gc->irq.request_key = request_key;

	 
	if (gpiochip_hierarchy_is_hierarchical(gc)) {
		domain = gpiochip_hierarchy_create_domain(gc);
	} else {
		domain = gpiochip_simple_create_domain(gc);
	}
	if (IS_ERR(domain))
		return PTR_ERR(domain);

	if (gc->irq.parent_handler) {
		for (i = 0; i < gc->irq.num_parents; i++) {
			void *data;

			if (gc->irq.per_parent_data)
				data = gc->irq.parent_handler_data_array[i];
			else
				data = gc->irq.parent_handler_data ?: gc;

			 
			irq_set_chained_handler_and_data(gc->irq.parents[i],
							 gc->irq.parent_handler,
							 data);
		}
	}

	gpiochip_set_irq_hooks(gc);

	ret = gpiochip_irqchip_add_allocated_domain(gc, domain, false);
	if (ret)
		return ret;

	acpi_gpiochip_request_interrupts(gc);

	return 0;
}

 
static void gpiochip_irqchip_remove(struct gpio_chip *gc)
{
	struct irq_chip *irqchip = gc->irq.chip;
	unsigned int offset;

	acpi_gpiochip_free_interrupts(gc);

	if (irqchip && gc->irq.parent_handler) {
		struct gpio_irq_chip *irq = &gc->irq;
		unsigned int i;

		for (i = 0; i < irq->num_parents; i++)
			irq_set_chained_handler_and_data(irq->parents[i],
							 NULL, NULL);
	}

	 
	if (!gc->irq.domain_is_allocated_externally && gc->irq.domain) {
		unsigned int irq;

		for (offset = 0; offset < gc->ngpio; offset++) {
			if (!gpiochip_irqchip_irq_valid(gc, offset))
				continue;

			irq = irq_find_mapping(gc->irq.domain, offset);
			irq_dispose_mapping(irq);
		}

		irq_domain_remove(gc->irq.domain);
	}

	if (irqchip && !(irqchip->flags & IRQCHIP_IMMUTABLE)) {
		if (irqchip->irq_request_resources == gpiochip_irq_reqres) {
			irqchip->irq_request_resources = NULL;
			irqchip->irq_release_resources = NULL;
		}
		if (irqchip->irq_enable == gpiochip_irq_enable) {
			irqchip->irq_enable = gc->irq.irq_enable;
			irqchip->irq_disable = gc->irq.irq_disable;
		}
	}
	gc->irq.irq_enable = NULL;
	gc->irq.irq_disable = NULL;
	gc->irq.chip = NULL;

	gpiochip_irqchip_free_valid_mask(gc);
}

 
int gpiochip_irqchip_add_domain(struct gpio_chip *gc,
				struct irq_domain *domain)
{
	return gpiochip_irqchip_add_allocated_domain(gc, domain, true);
}
EXPORT_SYMBOL_GPL(gpiochip_irqchip_add_domain);

#else  

static inline int gpiochip_add_irqchip(struct gpio_chip *gc,
				       struct lock_class_key *lock_key,
				       struct lock_class_key *request_key)
{
	return 0;
}
static void gpiochip_irqchip_remove(struct gpio_chip *gc) {}

static inline int gpiochip_irqchip_init_hw(struct gpio_chip *gc)
{
	return 0;
}

static inline int gpiochip_irqchip_init_valid_mask(struct gpio_chip *gc)
{
	return 0;
}
static inline void gpiochip_irqchip_free_valid_mask(struct gpio_chip *gc)
{ }

#endif  

 
int gpiochip_generic_request(struct gpio_chip *gc, unsigned int offset)
{
#ifdef CONFIG_PINCTRL
	if (list_empty(&gc->gpiodev->pin_ranges))
		return 0;
#endif

	return pinctrl_gpio_request(gc->gpiodev->base + offset);
}
EXPORT_SYMBOL_GPL(gpiochip_generic_request);

 
void gpiochip_generic_free(struct gpio_chip *gc, unsigned int offset)
{
#ifdef CONFIG_PINCTRL
	if (list_empty(&gc->gpiodev->pin_ranges))
		return;
#endif

	pinctrl_gpio_free(gc->gpiodev->base + offset);
}
EXPORT_SYMBOL_GPL(gpiochip_generic_free);

 
int gpiochip_generic_config(struct gpio_chip *gc, unsigned int offset,
			    unsigned long config)
{
	return pinctrl_gpio_set_config(gc->gpiodev->base + offset, config);
}
EXPORT_SYMBOL_GPL(gpiochip_generic_config);

#ifdef CONFIG_PINCTRL

 
int gpiochip_add_pingroup_range(struct gpio_chip *gc,
			struct pinctrl_dev *pctldev,
			unsigned int gpio_offset, const char *pin_group)
{
	struct gpio_pin_range *pin_range;
	struct gpio_device *gdev = gc->gpiodev;
	int ret;

	pin_range = kzalloc(sizeof(*pin_range), GFP_KERNEL);
	if (!pin_range) {
		chip_err(gc, "failed to allocate pin ranges\n");
		return -ENOMEM;
	}

	 
	pin_range->range.id = gpio_offset;
	pin_range->range.gc = gc;
	pin_range->range.name = gc->label;
	pin_range->range.base = gdev->base + gpio_offset;
	pin_range->pctldev = pctldev;

	ret = pinctrl_get_group_pins(pctldev, pin_group,
					&pin_range->range.pins,
					&pin_range->range.npins);
	if (ret < 0) {
		kfree(pin_range);
		return ret;
	}

	pinctrl_add_gpio_range(pctldev, &pin_range->range);

	chip_dbg(gc, "created GPIO range %d->%d ==> %s PINGRP %s\n",
		 gpio_offset, gpio_offset + pin_range->range.npins - 1,
		 pinctrl_dev_get_devname(pctldev), pin_group);

	list_add_tail(&pin_range->node, &gdev->pin_ranges);

	return 0;
}
EXPORT_SYMBOL_GPL(gpiochip_add_pingroup_range);

 
int gpiochip_add_pin_range(struct gpio_chip *gc, const char *pinctl_name,
			   unsigned int gpio_offset, unsigned int pin_offset,
			   unsigned int npins)
{
	struct gpio_pin_range *pin_range;
	struct gpio_device *gdev = gc->gpiodev;
	int ret;

	pin_range = kzalloc(sizeof(*pin_range), GFP_KERNEL);
	if (!pin_range) {
		chip_err(gc, "failed to allocate pin ranges\n");
		return -ENOMEM;
	}

	 
	pin_range->range.id = gpio_offset;
	pin_range->range.gc = gc;
	pin_range->range.name = gc->label;
	pin_range->range.base = gdev->base + gpio_offset;
	pin_range->range.pin_base = pin_offset;
	pin_range->range.npins = npins;
	pin_range->pctldev = pinctrl_find_and_add_gpio_range(pinctl_name,
			&pin_range->range);
	if (IS_ERR(pin_range->pctldev)) {
		ret = PTR_ERR(pin_range->pctldev);
		chip_err(gc, "could not create pin range\n");
		kfree(pin_range);
		return ret;
	}
	chip_dbg(gc, "created GPIO range %d->%d ==> %s PIN %d->%d\n",
		 gpio_offset, gpio_offset + npins - 1,
		 pinctl_name,
		 pin_offset, pin_offset + npins - 1);

	list_add_tail(&pin_range->node, &gdev->pin_ranges);

	return 0;
}
EXPORT_SYMBOL_GPL(gpiochip_add_pin_range);

 
void gpiochip_remove_pin_ranges(struct gpio_chip *gc)
{
	struct gpio_pin_range *pin_range, *tmp;
	struct gpio_device *gdev = gc->gpiodev;

	list_for_each_entry_safe(pin_range, tmp, &gdev->pin_ranges, node) {
		list_del(&pin_range->node);
		pinctrl_remove_gpio_range(pin_range->pctldev,
				&pin_range->range);
		kfree(pin_range);
	}
}
EXPORT_SYMBOL_GPL(gpiochip_remove_pin_ranges);

#endif  

 
static int gpiod_request_commit(struct gpio_desc *desc, const char *label)
{
	struct gpio_chip	*gc = desc->gdev->chip;
	int			ret;
	unsigned long		flags;
	unsigned		offset;

	if (label) {
		label = kstrdup_const(label, GFP_KERNEL);
		if (!label)
			return -ENOMEM;
	}

	spin_lock_irqsave(&gpio_lock, flags);

	 

	if (test_and_set_bit(FLAG_REQUESTED, &desc->flags) == 0) {
		desc_set_label(desc, label ? : "?");
	} else {
		ret = -EBUSY;
		goto out_free_unlock;
	}

	if (gc->request) {
		 
		spin_unlock_irqrestore(&gpio_lock, flags);
		offset = gpio_chip_hwgpio(desc);
		if (gpiochip_line_is_valid(gc, offset))
			ret = gc->request(gc, offset);
		else
			ret = -EINVAL;
		spin_lock_irqsave(&gpio_lock, flags);

		if (ret) {
			desc_set_label(desc, NULL);
			clear_bit(FLAG_REQUESTED, &desc->flags);
			goto out_free_unlock;
		}
	}
	if (gc->get_direction) {
		 
		spin_unlock_irqrestore(&gpio_lock, flags);
		gpiod_get_direction(desc);
		spin_lock_irqsave(&gpio_lock, flags);
	}
	spin_unlock_irqrestore(&gpio_lock, flags);
	return 0;

out_free_unlock:
	spin_unlock_irqrestore(&gpio_lock, flags);
	kfree_const(label);
	return ret;
}

 
static int validate_desc(const struct gpio_desc *desc, const char *func)
{
	if (!desc)
		return 0;
	if (IS_ERR(desc)) {
		pr_warn("%s: invalid GPIO (errorpointer)\n", func);
		return PTR_ERR(desc);
	}
	if (!desc->gdev) {
		pr_warn("%s: invalid GPIO (no device)\n", func);
		return -EINVAL;
	}
	if (!desc->gdev->chip) {
		dev_warn(&desc->gdev->dev,
			 "%s: backing chip is gone\n", func);
		return 0;
	}
	return 1;
}

#define VALIDATE_DESC(desc) do { \
	int __valid = validate_desc(desc, __func__); \
	if (__valid <= 0) \
		return __valid; \
	} while (0)

#define VALIDATE_DESC_VOID(desc) do { \
	int __valid = validate_desc(desc, __func__); \
	if (__valid <= 0) \
		return; \
	} while (0)

int gpiod_request(struct gpio_desc *desc, const char *label)
{
	int ret = -EPROBE_DEFER;

	VALIDATE_DESC(desc);

	if (try_module_get(desc->gdev->owner)) {
		ret = gpiod_request_commit(desc, label);
		if (ret)
			module_put(desc->gdev->owner);
		else
			gpio_device_get(desc->gdev);
	}

	if (ret)
		gpiod_dbg(desc, "%s: status %d\n", __func__, ret);

	return ret;
}

static bool gpiod_free_commit(struct gpio_desc *desc)
{
	bool			ret = false;
	unsigned long		flags;
	struct gpio_chip	*gc;

	might_sleep();

	spin_lock_irqsave(&gpio_lock, flags);

	gc = desc->gdev->chip;
	if (gc && test_bit(FLAG_REQUESTED, &desc->flags)) {
		if (gc->free) {
			spin_unlock_irqrestore(&gpio_lock, flags);
			might_sleep_if(gc->can_sleep);
			gc->free(gc, gpio_chip_hwgpio(desc));
			spin_lock_irqsave(&gpio_lock, flags);
		}
		kfree_const(desc->label);
		desc_set_label(desc, NULL);
		clear_bit(FLAG_ACTIVE_LOW, &desc->flags);
		clear_bit(FLAG_REQUESTED, &desc->flags);
		clear_bit(FLAG_OPEN_DRAIN, &desc->flags);
		clear_bit(FLAG_OPEN_SOURCE, &desc->flags);
		clear_bit(FLAG_PULL_UP, &desc->flags);
		clear_bit(FLAG_PULL_DOWN, &desc->flags);
		clear_bit(FLAG_BIAS_DISABLE, &desc->flags);
		clear_bit(FLAG_EDGE_RISING, &desc->flags);
		clear_bit(FLAG_EDGE_FALLING, &desc->flags);
		clear_bit(FLAG_IS_HOGGED, &desc->flags);
#ifdef CONFIG_OF_DYNAMIC
		desc->hog = NULL;
#endif
#ifdef CONFIG_GPIO_CDEV
		WRITE_ONCE(desc->debounce_period_us, 0);
#endif
		ret = true;
	}

	spin_unlock_irqrestore(&gpio_lock, flags);
	gpiod_line_state_notify(desc, GPIOLINE_CHANGED_RELEASED);

	return ret;
}

void gpiod_free(struct gpio_desc *desc)
{
	 
	if (!desc)
		return;

	if (!gpiod_free_commit(desc))
		WARN_ON(extra_checks);

	module_put(desc->gdev->owner);
	gpio_device_put(desc->gdev);
}

 
const char *gpiochip_is_requested(struct gpio_chip *gc, unsigned int offset)
{
	struct gpio_desc *desc;

	desc = gpiochip_get_desc(gc, offset);
	if (IS_ERR(desc))
		return NULL;

	if (test_bit(FLAG_REQUESTED, &desc->flags) == 0)
		return NULL;
	return desc->label;
}
EXPORT_SYMBOL_GPL(gpiochip_is_requested);

 
struct gpio_desc *gpiochip_request_own_desc(struct gpio_chip *gc,
					    unsigned int hwnum,
					    const char *label,
					    enum gpio_lookup_flags lflags,
					    enum gpiod_flags dflags)
{
	struct gpio_desc *desc = gpiochip_get_desc(gc, hwnum);
	int ret;

	if (IS_ERR(desc)) {
		chip_err(gc, "failed to get GPIO descriptor\n");
		return desc;
	}

	ret = gpiod_request_commit(desc, label);
	if (ret < 0)
		return ERR_PTR(ret);

	ret = gpiod_configure_flags(desc, label, lflags, dflags);
	if (ret) {
		chip_err(gc, "setup of own GPIO %s failed\n", label);
		gpiod_free_commit(desc);
		return ERR_PTR(ret);
	}

	return desc;
}
EXPORT_SYMBOL_GPL(gpiochip_request_own_desc);

 
void gpiochip_free_own_desc(struct gpio_desc *desc)
{
	if (desc)
		gpiod_free_commit(desc);
}
EXPORT_SYMBOL_GPL(gpiochip_free_own_desc);

 

static int gpio_do_set_config(struct gpio_chip *gc, unsigned int offset,
			      unsigned long config)
{
	if (!gc->set_config)
		return -ENOTSUPP;

	return gc->set_config(gc, offset, config);
}

static int gpio_set_config_with_argument(struct gpio_desc *desc,
					 enum pin_config_param mode,
					 u32 argument)
{
	struct gpio_chip *gc = desc->gdev->chip;
	unsigned long config;

	config = pinconf_to_config_packed(mode, argument);
	return gpio_do_set_config(gc, gpio_chip_hwgpio(desc), config);
}

static int gpio_set_config_with_argument_optional(struct gpio_desc *desc,
						  enum pin_config_param mode,
						  u32 argument)
{
	struct device *dev = &desc->gdev->dev;
	int gpio = gpio_chip_hwgpio(desc);
	int ret;

	ret = gpio_set_config_with_argument(desc, mode, argument);
	if (ret != -ENOTSUPP)
		return ret;

	switch (mode) {
	case PIN_CONFIG_PERSIST_STATE:
		dev_dbg(dev, "Persistence not supported for GPIO %d\n", gpio);
		break;
	default:
		break;
	}

	return 0;
}

static int gpio_set_config(struct gpio_desc *desc, enum pin_config_param mode)
{
	return gpio_set_config_with_argument(desc, mode, 0);
}

static int gpio_set_bias(struct gpio_desc *desc)
{
	enum pin_config_param bias;
	unsigned int arg;

	if (test_bit(FLAG_BIAS_DISABLE, &desc->flags))
		bias = PIN_CONFIG_BIAS_DISABLE;
	else if (test_bit(FLAG_PULL_UP, &desc->flags))
		bias = PIN_CONFIG_BIAS_PULL_UP;
	else if (test_bit(FLAG_PULL_DOWN, &desc->flags))
		bias = PIN_CONFIG_BIAS_PULL_DOWN;
	else
		return 0;

	switch (bias) {
	case PIN_CONFIG_BIAS_PULL_DOWN:
	case PIN_CONFIG_BIAS_PULL_UP:
		arg = 1;
		break;

	default:
		arg = 0;
		break;
	}

	return gpio_set_config_with_argument_optional(desc, bias, arg);
}

 
int gpio_set_debounce_timeout(struct gpio_desc *desc, unsigned int debounce)
{
	return gpio_set_config_with_argument_optional(desc,
						      PIN_CONFIG_INPUT_DEBOUNCE,
						      debounce);
}

 
int gpiod_direction_input(struct gpio_desc *desc)
{
	struct gpio_chip	*gc;
	int			ret = 0;

	VALIDATE_DESC(desc);
	gc = desc->gdev->chip;

	 
	if (!gc->get && gc->direction_input) {
		gpiod_warn(desc,
			   "%s: missing get() but have direction_input()\n",
			   __func__);
		return -EIO;
	}

	 
	if (gc->direction_input) {
		ret = gc->direction_input(gc, gpio_chip_hwgpio(desc));
	} else if (gc->get_direction &&
		  (gc->get_direction(gc, gpio_chip_hwgpio(desc)) != 1)) {
		gpiod_warn(desc,
			   "%s: missing direction_input() operation and line is output\n",
			   __func__);
		return -EIO;
	}
	if (ret == 0) {
		clear_bit(FLAG_IS_OUT, &desc->flags);
		ret = gpio_set_bias(desc);
	}

	trace_gpio_direction(desc_to_gpio(desc), 1, ret);

	return ret;
}
EXPORT_SYMBOL_GPL(gpiod_direction_input);

static int gpiod_direction_output_raw_commit(struct gpio_desc *desc, int value)
{
	struct gpio_chip *gc = desc->gdev->chip;
	int val = !!value;
	int ret = 0;

	 
	if (!gc->set && !gc->direction_output) {
		gpiod_warn(desc,
			   "%s: missing set() and direction_output() operations\n",
			   __func__);
		return -EIO;
	}

	if (gc->direction_output) {
		ret = gc->direction_output(gc, gpio_chip_hwgpio(desc), val);
	} else {
		 
		if (gc->get_direction &&
		    gc->get_direction(gc, gpio_chip_hwgpio(desc))) {
			gpiod_warn(desc,
				"%s: missing direction_output() operation\n",
				__func__);
			return -EIO;
		}
		 
		gc->set(gc, gpio_chip_hwgpio(desc), val);
	}

	if (!ret)
		set_bit(FLAG_IS_OUT, &desc->flags);
	trace_gpio_value(desc_to_gpio(desc), 0, val);
	trace_gpio_direction(desc_to_gpio(desc), 0, ret);
	return ret;
}

 
int gpiod_direction_output_raw(struct gpio_desc *desc, int value)
{
	VALIDATE_DESC(desc);
	return gpiod_direction_output_raw_commit(desc, value);
}
EXPORT_SYMBOL_GPL(gpiod_direction_output_raw);

 
int gpiod_direction_output(struct gpio_desc *desc, int value)
{
	int ret;

	VALIDATE_DESC(desc);
	if (test_bit(FLAG_ACTIVE_LOW, &desc->flags))
		value = !value;
	else
		value = !!value;

	 
	if (test_bit(FLAG_USED_AS_IRQ, &desc->flags) &&
	    test_bit(FLAG_IRQ_IS_ENABLED, &desc->flags)) {
		gpiod_err(desc,
			  "%s: tried to set a GPIO tied to an IRQ as output\n",
			  __func__);
		return -EIO;
	}

	if (test_bit(FLAG_OPEN_DRAIN, &desc->flags)) {
		 
		ret = gpio_set_config(desc, PIN_CONFIG_DRIVE_OPEN_DRAIN);
		if (!ret)
			goto set_output_value;
		 
		if (value) {
			ret = gpiod_direction_input(desc);
			goto set_output_flag;
		}
	} else if (test_bit(FLAG_OPEN_SOURCE, &desc->flags)) {
		ret = gpio_set_config(desc, PIN_CONFIG_DRIVE_OPEN_SOURCE);
		if (!ret)
			goto set_output_value;
		 
		if (!value) {
			ret = gpiod_direction_input(desc);
			goto set_output_flag;
		}
	} else {
		gpio_set_config(desc, PIN_CONFIG_DRIVE_PUSH_PULL);
	}

set_output_value:
	ret = gpio_set_bias(desc);
	if (ret)
		return ret;
	return gpiod_direction_output_raw_commit(desc, value);

set_output_flag:
	 
	if (ret == 0)
		set_bit(FLAG_IS_OUT, &desc->flags);
	return ret;
}
EXPORT_SYMBOL_GPL(gpiod_direction_output);

 
int gpiod_enable_hw_timestamp_ns(struct gpio_desc *desc, unsigned long flags)
{
	int ret = 0;
	struct gpio_chip *gc;

	VALIDATE_DESC(desc);

	gc = desc->gdev->chip;
	if (!gc->en_hw_timestamp) {
		gpiod_warn(desc, "%s: hw ts not supported\n", __func__);
		return -ENOTSUPP;
	}

	ret = gc->en_hw_timestamp(gc, gpio_chip_hwgpio(desc), flags);
	if (ret)
		gpiod_warn(desc, "%s: hw ts request failed\n", __func__);

	return ret;
}
EXPORT_SYMBOL_GPL(gpiod_enable_hw_timestamp_ns);

 
int gpiod_disable_hw_timestamp_ns(struct gpio_desc *desc, unsigned long flags)
{
	int ret = 0;
	struct gpio_chip *gc;

	VALIDATE_DESC(desc);

	gc = desc->gdev->chip;
	if (!gc->dis_hw_timestamp) {
		gpiod_warn(desc, "%s: hw ts not supported\n", __func__);
		return -ENOTSUPP;
	}

	ret = gc->dis_hw_timestamp(gc, gpio_chip_hwgpio(desc), flags);
	if (ret)
		gpiod_warn(desc, "%s: hw ts release failed\n", __func__);

	return ret;
}
EXPORT_SYMBOL_GPL(gpiod_disable_hw_timestamp_ns);

 
int gpiod_set_config(struct gpio_desc *desc, unsigned long config)
{
	struct gpio_chip *gc;

	VALIDATE_DESC(desc);
	gc = desc->gdev->chip;

	return gpio_do_set_config(gc, gpio_chip_hwgpio(desc), config);
}
EXPORT_SYMBOL_GPL(gpiod_set_config);

 
int gpiod_set_debounce(struct gpio_desc *desc, unsigned int debounce)
{
	unsigned long config;

	config = pinconf_to_config_packed(PIN_CONFIG_INPUT_DEBOUNCE, debounce);
	return gpiod_set_config(desc, config);
}
EXPORT_SYMBOL_GPL(gpiod_set_debounce);

 
int gpiod_set_transitory(struct gpio_desc *desc, bool transitory)
{
	VALIDATE_DESC(desc);
	 
	assign_bit(FLAG_TRANSITORY, &desc->flags, transitory);

	 
	return gpio_set_config_with_argument_optional(desc,
						      PIN_CONFIG_PERSIST_STATE,
						      !transitory);
}
EXPORT_SYMBOL_GPL(gpiod_set_transitory);

 
int gpiod_is_active_low(const struct gpio_desc *desc)
{
	VALIDATE_DESC(desc);
	return test_bit(FLAG_ACTIVE_LOW, &desc->flags);
}
EXPORT_SYMBOL_GPL(gpiod_is_active_low);

 
void gpiod_toggle_active_low(struct gpio_desc *desc)
{
	VALIDATE_DESC_VOID(desc);
	change_bit(FLAG_ACTIVE_LOW, &desc->flags);
}
EXPORT_SYMBOL_GPL(gpiod_toggle_active_low);

static int gpio_chip_get_value(struct gpio_chip *gc, const struct gpio_desc *desc)
{
	return gc->get ? gc->get(gc, gpio_chip_hwgpio(desc)) : -EIO;
}

 

static int gpiod_get_raw_value_commit(const struct gpio_desc *desc)
{
	struct gpio_chip	*gc;
	int value;

	gc = desc->gdev->chip;
	value = gpio_chip_get_value(gc, desc);
	value = value < 0 ? value : !!value;
	trace_gpio_value(desc_to_gpio(desc), 1, value);
	return value;
}

static int gpio_chip_get_multiple(struct gpio_chip *gc,
				  unsigned long *mask, unsigned long *bits)
{
	if (gc->get_multiple)
		return gc->get_multiple(gc, mask, bits);
	if (gc->get) {
		int i, value;

		for_each_set_bit(i, mask, gc->ngpio) {
			value = gc->get(gc, i);
			if (value < 0)
				return value;
			__assign_bit(i, bits, value);
		}
		return 0;
	}
	return -EIO;
}

int gpiod_get_array_value_complex(bool raw, bool can_sleep,
				  unsigned int array_size,
				  struct gpio_desc **desc_array,
				  struct gpio_array *array_info,
				  unsigned long *value_bitmap)
{
	int ret, i = 0;

	 
	if (array_info && array_info->desc == desc_array &&
	    array_size <= array_info->size &&
	    (void *)array_info == desc_array + array_info->size) {
		if (!can_sleep)
			WARN_ON(array_info->chip->can_sleep);

		ret = gpio_chip_get_multiple(array_info->chip,
					     array_info->get_mask,
					     value_bitmap);
		if (ret)
			return ret;

		if (!raw && !bitmap_empty(array_info->invert_mask, array_size))
			bitmap_xor(value_bitmap, value_bitmap,
				   array_info->invert_mask, array_size);

		i = find_first_zero_bit(array_info->get_mask, array_size);
		if (i == array_size)
			return 0;
	} else {
		array_info = NULL;
	}

	while (i < array_size) {
		struct gpio_chip *gc = desc_array[i]->gdev->chip;
		DECLARE_BITMAP(fastpath_mask, FASTPATH_NGPIO);
		DECLARE_BITMAP(fastpath_bits, FASTPATH_NGPIO);
		unsigned long *mask, *bits;
		int first, j;

		if (likely(gc->ngpio <= FASTPATH_NGPIO)) {
			mask = fastpath_mask;
			bits = fastpath_bits;
		} else {
			gfp_t flags = can_sleep ? GFP_KERNEL : GFP_ATOMIC;

			mask = bitmap_alloc(gc->ngpio, flags);
			if (!mask)
				return -ENOMEM;

			bits = bitmap_alloc(gc->ngpio, flags);
			if (!bits) {
				bitmap_free(mask);
				return -ENOMEM;
			}
		}

		bitmap_zero(mask, gc->ngpio);

		if (!can_sleep)
			WARN_ON(gc->can_sleep);

		 
		first = i;
		do {
			const struct gpio_desc *desc = desc_array[i];
			int hwgpio = gpio_chip_hwgpio(desc);

			__set_bit(hwgpio, mask);
			i++;

			if (array_info)
				i = find_next_zero_bit(array_info->get_mask,
						       array_size, i);
		} while ((i < array_size) &&
			 (desc_array[i]->gdev->chip == gc));

		ret = gpio_chip_get_multiple(gc, mask, bits);
		if (ret) {
			if (mask != fastpath_mask)
				bitmap_free(mask);
			if (bits != fastpath_bits)
				bitmap_free(bits);
			return ret;
		}

		for (j = first; j < i; ) {
			const struct gpio_desc *desc = desc_array[j];
			int hwgpio = gpio_chip_hwgpio(desc);
			int value = test_bit(hwgpio, bits);

			if (!raw && test_bit(FLAG_ACTIVE_LOW, &desc->flags))
				value = !value;
			__assign_bit(j, value_bitmap, value);
			trace_gpio_value(desc_to_gpio(desc), 1, value);
			j++;

			if (array_info)
				j = find_next_zero_bit(array_info->get_mask, i,
						       j);
		}

		if (mask != fastpath_mask)
			bitmap_free(mask);
		if (bits != fastpath_bits)
			bitmap_free(bits);
	}
	return 0;
}

 
int gpiod_get_raw_value(const struct gpio_desc *desc)
{
	VALIDATE_DESC(desc);
	 
	WARN_ON(desc->gdev->chip->can_sleep);
	return gpiod_get_raw_value_commit(desc);
}
EXPORT_SYMBOL_GPL(gpiod_get_raw_value);

 
int gpiod_get_value(const struct gpio_desc *desc)
{
	int value;

	VALIDATE_DESC(desc);
	 
	WARN_ON(desc->gdev->chip->can_sleep);

	value = gpiod_get_raw_value_commit(desc);
	if (value < 0)
		return value;

	if (test_bit(FLAG_ACTIVE_LOW, &desc->flags))
		value = !value;

	return value;
}
EXPORT_SYMBOL_GPL(gpiod_get_value);

 
int gpiod_get_raw_array_value(unsigned int array_size,
			      struct gpio_desc **desc_array,
			      struct gpio_array *array_info,
			      unsigned long *value_bitmap)
{
	if (!desc_array)
		return -EINVAL;
	return gpiod_get_array_value_complex(true, false, array_size,
					     desc_array, array_info,
					     value_bitmap);
}
EXPORT_SYMBOL_GPL(gpiod_get_raw_array_value);

 
int gpiod_get_array_value(unsigned int array_size,
			  struct gpio_desc **desc_array,
			  struct gpio_array *array_info,
			  unsigned long *value_bitmap)
{
	if (!desc_array)
		return -EINVAL;
	return gpiod_get_array_value_complex(false, false, array_size,
					     desc_array, array_info,
					     value_bitmap);
}
EXPORT_SYMBOL_GPL(gpiod_get_array_value);

 
static void gpio_set_open_drain_value_commit(struct gpio_desc *desc, bool value)
{
	int ret = 0;
	struct gpio_chip *gc = desc->gdev->chip;
	int offset = gpio_chip_hwgpio(desc);

	if (value) {
		ret = gc->direction_input(gc, offset);
	} else {
		ret = gc->direction_output(gc, offset, 0);
		if (!ret)
			set_bit(FLAG_IS_OUT, &desc->flags);
	}
	trace_gpio_direction(desc_to_gpio(desc), value, ret);
	if (ret < 0)
		gpiod_err(desc,
			  "%s: Error in set_value for open drain err %d\n",
			  __func__, ret);
}

 
static void gpio_set_open_source_value_commit(struct gpio_desc *desc, bool value)
{
	int ret = 0;
	struct gpio_chip *gc = desc->gdev->chip;
	int offset = gpio_chip_hwgpio(desc);

	if (value) {
		ret = gc->direction_output(gc, offset, 1);
		if (!ret)
			set_bit(FLAG_IS_OUT, &desc->flags);
	} else {
		ret = gc->direction_input(gc, offset);
	}
	trace_gpio_direction(desc_to_gpio(desc), !value, ret);
	if (ret < 0)
		gpiod_err(desc,
			  "%s: Error in set_value for open source err %d\n",
			  __func__, ret);
}

static void gpiod_set_raw_value_commit(struct gpio_desc *desc, bool value)
{
	struct gpio_chip	*gc;

	gc = desc->gdev->chip;
	trace_gpio_value(desc_to_gpio(desc), 0, value);
	gc->set(gc, gpio_chip_hwgpio(desc), value);
}

 
static void gpio_chip_set_multiple(struct gpio_chip *gc,
				   unsigned long *mask, unsigned long *bits)
{
	if (gc->set_multiple) {
		gc->set_multiple(gc, mask, bits);
	} else {
		unsigned int i;

		 
		for_each_set_bit(i, mask, gc->ngpio)
			gc->set(gc, i, test_bit(i, bits));
	}
}

int gpiod_set_array_value_complex(bool raw, bool can_sleep,
				  unsigned int array_size,
				  struct gpio_desc **desc_array,
				  struct gpio_array *array_info,
				  unsigned long *value_bitmap)
{
	int i = 0;

	 
	if (array_info && array_info->desc == desc_array &&
	    array_size <= array_info->size &&
	    (void *)array_info == desc_array + array_info->size) {
		if (!can_sleep)
			WARN_ON(array_info->chip->can_sleep);

		if (!raw && !bitmap_empty(array_info->invert_mask, array_size))
			bitmap_xor(value_bitmap, value_bitmap,
				   array_info->invert_mask, array_size);

		gpio_chip_set_multiple(array_info->chip, array_info->set_mask,
				       value_bitmap);

		i = find_first_zero_bit(array_info->set_mask, array_size);
		if (i == array_size)
			return 0;
	} else {
		array_info = NULL;
	}

	while (i < array_size) {
		struct gpio_chip *gc = desc_array[i]->gdev->chip;
		DECLARE_BITMAP(fastpath_mask, FASTPATH_NGPIO);
		DECLARE_BITMAP(fastpath_bits, FASTPATH_NGPIO);
		unsigned long *mask, *bits;
		int count = 0;

		if (likely(gc->ngpio <= FASTPATH_NGPIO)) {
			mask = fastpath_mask;
			bits = fastpath_bits;
		} else {
			gfp_t flags = can_sleep ? GFP_KERNEL : GFP_ATOMIC;

			mask = bitmap_alloc(gc->ngpio, flags);
			if (!mask)
				return -ENOMEM;

			bits = bitmap_alloc(gc->ngpio, flags);
			if (!bits) {
				bitmap_free(mask);
				return -ENOMEM;
			}
		}

		bitmap_zero(mask, gc->ngpio);

		if (!can_sleep)
			WARN_ON(gc->can_sleep);

		do {
			struct gpio_desc *desc = desc_array[i];
			int hwgpio = gpio_chip_hwgpio(desc);
			int value = test_bit(i, value_bitmap);

			 
			if (!raw && !(array_info &&
			    test_bit(i, array_info->invert_mask)) &&
			    test_bit(FLAG_ACTIVE_LOW, &desc->flags))
				value = !value;
			trace_gpio_value(desc_to_gpio(desc), 0, value);
			 
			if (test_bit(FLAG_OPEN_DRAIN, &desc->flags) && !raw) {
				gpio_set_open_drain_value_commit(desc, value);
			} else if (test_bit(FLAG_OPEN_SOURCE, &desc->flags) && !raw) {
				gpio_set_open_source_value_commit(desc, value);
			} else {
				__set_bit(hwgpio, mask);
				__assign_bit(hwgpio, bits, value);
				count++;
			}
			i++;

			if (array_info)
				i = find_next_zero_bit(array_info->set_mask,
						       array_size, i);
		} while ((i < array_size) &&
			 (desc_array[i]->gdev->chip == gc));
		 
		if (count != 0)
			gpio_chip_set_multiple(gc, mask, bits);

		if (mask != fastpath_mask)
			bitmap_free(mask);
		if (bits != fastpath_bits)
			bitmap_free(bits);
	}
	return 0;
}

 
void gpiod_set_raw_value(struct gpio_desc *desc, int value)
{
	VALIDATE_DESC_VOID(desc);
	 
	WARN_ON(desc->gdev->chip->can_sleep);
	gpiod_set_raw_value_commit(desc, value);
}
EXPORT_SYMBOL_GPL(gpiod_set_raw_value);

 
static void gpiod_set_value_nocheck(struct gpio_desc *desc, int value)
{
	if (test_bit(FLAG_ACTIVE_LOW, &desc->flags))
		value = !value;
	if (test_bit(FLAG_OPEN_DRAIN, &desc->flags))
		gpio_set_open_drain_value_commit(desc, value);
	else if (test_bit(FLAG_OPEN_SOURCE, &desc->flags))
		gpio_set_open_source_value_commit(desc, value);
	else
		gpiod_set_raw_value_commit(desc, value);
}

 
void gpiod_set_value(struct gpio_desc *desc, int value)
{
	VALIDATE_DESC_VOID(desc);
	 
	WARN_ON(desc->gdev->chip->can_sleep);
	gpiod_set_value_nocheck(desc, value);
}
EXPORT_SYMBOL_GPL(gpiod_set_value);

 
int gpiod_set_raw_array_value(unsigned int array_size,
			      struct gpio_desc **desc_array,
			      struct gpio_array *array_info,
			      unsigned long *value_bitmap)
{
	if (!desc_array)
		return -EINVAL;
	return gpiod_set_array_value_complex(true, false, array_size,
					desc_array, array_info, value_bitmap);
}
EXPORT_SYMBOL_GPL(gpiod_set_raw_array_value);

 
int gpiod_set_array_value(unsigned int array_size,
			  struct gpio_desc **desc_array,
			  struct gpio_array *array_info,
			  unsigned long *value_bitmap)
{
	if (!desc_array)
		return -EINVAL;
	return gpiod_set_array_value_complex(false, false, array_size,
					     desc_array, array_info,
					     value_bitmap);
}
EXPORT_SYMBOL_GPL(gpiod_set_array_value);

 
int gpiod_cansleep(const struct gpio_desc *desc)
{
	VALIDATE_DESC(desc);
	return desc->gdev->chip->can_sleep;
}
EXPORT_SYMBOL_GPL(gpiod_cansleep);

 
int gpiod_set_consumer_name(struct gpio_desc *desc, const char *name)
{
	VALIDATE_DESC(desc);
	if (name) {
		name = kstrdup_const(name, GFP_KERNEL);
		if (!name)
			return -ENOMEM;
	}

	kfree_const(desc->label);
	desc_set_label(desc, name);

	return 0;
}
EXPORT_SYMBOL_GPL(gpiod_set_consumer_name);

 
int gpiod_to_irq(const struct gpio_desc *desc)
{
	struct gpio_chip *gc;
	int offset;

	 
	if (!desc || IS_ERR(desc) || !desc->gdev || !desc->gdev->chip)
		return -EINVAL;

	gc = desc->gdev->chip;
	offset = gpio_chip_hwgpio(desc);
	if (gc->to_irq) {
		int retirq = gc->to_irq(gc, offset);

		 
		if (!retirq)
			return -ENXIO;

		return retirq;
	}
#ifdef CONFIG_GPIOLIB_IRQCHIP
	if (gc->irq.chip) {
		 
		return -EPROBE_DEFER;
	}
#endif
	return -ENXIO;
}
EXPORT_SYMBOL_GPL(gpiod_to_irq);

 
int gpiochip_lock_as_irq(struct gpio_chip *gc, unsigned int offset)
{
	struct gpio_desc *desc;

	desc = gpiochip_get_desc(gc, offset);
	if (IS_ERR(desc))
		return PTR_ERR(desc);

	 
	if (!gc->can_sleep && gc->get_direction) {
		int dir = gpiod_get_direction(desc);

		if (dir < 0) {
			chip_err(gc, "%s: cannot get GPIO direction\n",
				 __func__);
			return dir;
		}
	}

	 
	if (test_bit(FLAG_IS_OUT, &desc->flags) &&
	    !test_bit(FLAG_OPEN_DRAIN, &desc->flags)) {
		chip_err(gc,
			 "%s: tried to flag a GPIO set as output for IRQ\n",
			 __func__);
		return -EIO;
	}

	set_bit(FLAG_USED_AS_IRQ, &desc->flags);
	set_bit(FLAG_IRQ_IS_ENABLED, &desc->flags);

	 
	if (!desc->label)
		desc_set_label(desc, "interrupt");

	return 0;
}
EXPORT_SYMBOL_GPL(gpiochip_lock_as_irq);

 
void gpiochip_unlock_as_irq(struct gpio_chip *gc, unsigned int offset)
{
	struct gpio_desc *desc;

	desc = gpiochip_get_desc(gc, offset);
	if (IS_ERR(desc))
		return;

	clear_bit(FLAG_USED_AS_IRQ, &desc->flags);
	clear_bit(FLAG_IRQ_IS_ENABLED, &desc->flags);

	 
	if (desc->label && !strcmp(desc->label, "interrupt"))
		desc_set_label(desc, NULL);
}
EXPORT_SYMBOL_GPL(gpiochip_unlock_as_irq);

void gpiochip_disable_irq(struct gpio_chip *gc, unsigned int offset)
{
	struct gpio_desc *desc = gpiochip_get_desc(gc, offset);

	if (!IS_ERR(desc) &&
	    !WARN_ON(!test_bit(FLAG_USED_AS_IRQ, &desc->flags)))
		clear_bit(FLAG_IRQ_IS_ENABLED, &desc->flags);
}
EXPORT_SYMBOL_GPL(gpiochip_disable_irq);

void gpiochip_enable_irq(struct gpio_chip *gc, unsigned int offset)
{
	struct gpio_desc *desc = gpiochip_get_desc(gc, offset);

	if (!IS_ERR(desc) &&
	    !WARN_ON(!test_bit(FLAG_USED_AS_IRQ, &desc->flags))) {
		 
		WARN_ON(test_bit(FLAG_IS_OUT, &desc->flags) &&
			!test_bit(FLAG_OPEN_DRAIN, &desc->flags));
		set_bit(FLAG_IRQ_IS_ENABLED, &desc->flags);
	}
}
EXPORT_SYMBOL_GPL(gpiochip_enable_irq);

bool gpiochip_line_is_irq(struct gpio_chip *gc, unsigned int offset)
{
	if (offset >= gc->ngpio)
		return false;

	return test_bit(FLAG_USED_AS_IRQ, &gc->gpiodev->descs[offset].flags);
}
EXPORT_SYMBOL_GPL(gpiochip_line_is_irq);

int gpiochip_reqres_irq(struct gpio_chip *gc, unsigned int offset)
{
	int ret;

	if (!try_module_get(gc->gpiodev->owner))
		return -ENODEV;

	ret = gpiochip_lock_as_irq(gc, offset);
	if (ret) {
		chip_err(gc, "unable to lock HW IRQ %u for IRQ\n", offset);
		module_put(gc->gpiodev->owner);
		return ret;
	}
	return 0;
}
EXPORT_SYMBOL_GPL(gpiochip_reqres_irq);

void gpiochip_relres_irq(struct gpio_chip *gc, unsigned int offset)
{
	gpiochip_unlock_as_irq(gc, offset);
	module_put(gc->gpiodev->owner);
}
EXPORT_SYMBOL_GPL(gpiochip_relres_irq);

bool gpiochip_line_is_open_drain(struct gpio_chip *gc, unsigned int offset)
{
	if (offset >= gc->ngpio)
		return false;

	return test_bit(FLAG_OPEN_DRAIN, &gc->gpiodev->descs[offset].flags);
}
EXPORT_SYMBOL_GPL(gpiochip_line_is_open_drain);

bool gpiochip_line_is_open_source(struct gpio_chip *gc, unsigned int offset)
{
	if (offset >= gc->ngpio)
		return false;

	return test_bit(FLAG_OPEN_SOURCE, &gc->gpiodev->descs[offset].flags);
}
EXPORT_SYMBOL_GPL(gpiochip_line_is_open_source);

bool gpiochip_line_is_persistent(struct gpio_chip *gc, unsigned int offset)
{
	if (offset >= gc->ngpio)
		return false;

	return !test_bit(FLAG_TRANSITORY, &gc->gpiodev->descs[offset].flags);
}
EXPORT_SYMBOL_GPL(gpiochip_line_is_persistent);

 
int gpiod_get_raw_value_cansleep(const struct gpio_desc *desc)
{
	might_sleep_if(extra_checks);
	VALIDATE_DESC(desc);
	return gpiod_get_raw_value_commit(desc);
}
EXPORT_SYMBOL_GPL(gpiod_get_raw_value_cansleep);

 
int gpiod_get_value_cansleep(const struct gpio_desc *desc)
{
	int value;

	might_sleep_if(extra_checks);
	VALIDATE_DESC(desc);
	value = gpiod_get_raw_value_commit(desc);
	if (value < 0)
		return value;

	if (test_bit(FLAG_ACTIVE_LOW, &desc->flags))
		value = !value;

	return value;
}
EXPORT_SYMBOL_GPL(gpiod_get_value_cansleep);

 
int gpiod_get_raw_array_value_cansleep(unsigned int array_size,
				       struct gpio_desc **desc_array,
				       struct gpio_array *array_info,
				       unsigned long *value_bitmap)
{
	might_sleep_if(extra_checks);
	if (!desc_array)
		return -EINVAL;
	return gpiod_get_array_value_complex(true, true, array_size,
					     desc_array, array_info,
					     value_bitmap);
}
EXPORT_SYMBOL_GPL(gpiod_get_raw_array_value_cansleep);

 
int gpiod_get_array_value_cansleep(unsigned int array_size,
				   struct gpio_desc **desc_array,
				   struct gpio_array *array_info,
				   unsigned long *value_bitmap)
{
	might_sleep_if(extra_checks);
	if (!desc_array)
		return -EINVAL;
	return gpiod_get_array_value_complex(false, true, array_size,
					     desc_array, array_info,
					     value_bitmap);
}
EXPORT_SYMBOL_GPL(gpiod_get_array_value_cansleep);

 
void gpiod_set_raw_value_cansleep(struct gpio_desc *desc, int value)
{
	might_sleep_if(extra_checks);
	VALIDATE_DESC_VOID(desc);
	gpiod_set_raw_value_commit(desc, value);
}
EXPORT_SYMBOL_GPL(gpiod_set_raw_value_cansleep);

 
void gpiod_set_value_cansleep(struct gpio_desc *desc, int value)
{
	might_sleep_if(extra_checks);
	VALIDATE_DESC_VOID(desc);
	gpiod_set_value_nocheck(desc, value);
}
EXPORT_SYMBOL_GPL(gpiod_set_value_cansleep);

 
int gpiod_set_raw_array_value_cansleep(unsigned int array_size,
				       struct gpio_desc **desc_array,
				       struct gpio_array *array_info,
				       unsigned long *value_bitmap)
{
	might_sleep_if(extra_checks);
	if (!desc_array)
		return -EINVAL;
	return gpiod_set_array_value_complex(true, true, array_size, desc_array,
				      array_info, value_bitmap);
}
EXPORT_SYMBOL_GPL(gpiod_set_raw_array_value_cansleep);

 
void gpiod_add_lookup_tables(struct gpiod_lookup_table **tables, size_t n)
{
	unsigned int i;

	mutex_lock(&gpio_lookup_lock);

	for (i = 0; i < n; i++)
		list_add_tail(&tables[i]->list, &gpio_lookup_list);

	mutex_unlock(&gpio_lookup_lock);
}

 
int gpiod_set_array_value_cansleep(unsigned int array_size,
				   struct gpio_desc **desc_array,
				   struct gpio_array *array_info,
				   unsigned long *value_bitmap)
{
	might_sleep_if(extra_checks);
	if (!desc_array)
		return -EINVAL;
	return gpiod_set_array_value_complex(false, true, array_size,
					     desc_array, array_info,
					     value_bitmap);
}
EXPORT_SYMBOL_GPL(gpiod_set_array_value_cansleep);

void gpiod_line_state_notify(struct gpio_desc *desc, unsigned long action)
{
	blocking_notifier_call_chain(&desc->gdev->line_state_notifier,
				     action, desc);
}

 
void gpiod_add_lookup_table(struct gpiod_lookup_table *table)
{
	gpiod_add_lookup_tables(&table, 1);
}
EXPORT_SYMBOL_GPL(gpiod_add_lookup_table);

 
void gpiod_remove_lookup_table(struct gpiod_lookup_table *table)
{
	 
	if (!table)
		return;

	mutex_lock(&gpio_lookup_lock);

	list_del(&table->list);

	mutex_unlock(&gpio_lookup_lock);
}
EXPORT_SYMBOL_GPL(gpiod_remove_lookup_table);

 
void gpiod_add_hogs(struct gpiod_hog *hogs)
{
	struct gpio_chip *gc;
	struct gpiod_hog *hog;

	mutex_lock(&gpio_machine_hogs_mutex);

	for (hog = &hogs[0]; hog->chip_label; hog++) {
		list_add_tail(&hog->list, &gpio_machine_hogs);

		 
		gc = find_chip_by_name(hog->chip_label);
		if (gc)
			gpiochip_machine_hog(gc, hog);
	}

	mutex_unlock(&gpio_machine_hogs_mutex);
}
EXPORT_SYMBOL_GPL(gpiod_add_hogs);

void gpiod_remove_hogs(struct gpiod_hog *hogs)
{
	struct gpiod_hog *hog;

	mutex_lock(&gpio_machine_hogs_mutex);
	for (hog = &hogs[0]; hog->chip_label; hog++)
		list_del(&hog->list);
	mutex_unlock(&gpio_machine_hogs_mutex);
}
EXPORT_SYMBOL_GPL(gpiod_remove_hogs);

static struct gpiod_lookup_table *gpiod_find_lookup_table(struct device *dev)
{
	const char *dev_id = dev ? dev_name(dev) : NULL;
	struct gpiod_lookup_table *table;

	mutex_lock(&gpio_lookup_lock);

	list_for_each_entry(table, &gpio_lookup_list, list) {
		if (table->dev_id && dev_id) {
			 
			if (!strcmp(table->dev_id, dev_id))
				goto found;
		} else {
			 
			if (dev_id == table->dev_id)
				goto found;
		}
	}
	table = NULL;

found:
	mutex_unlock(&gpio_lookup_lock);
	return table;
}

static struct gpio_desc *gpiod_find(struct device *dev, const char *con_id,
				    unsigned int idx, unsigned long *flags)
{
	struct gpio_desc *desc = ERR_PTR(-ENOENT);
	struct gpiod_lookup_table *table;
	struct gpiod_lookup *p;

	table = gpiod_find_lookup_table(dev);
	if (!table)
		return desc;

	for (p = &table->table[0]; p->key; p++) {
		struct gpio_chip *gc;

		 
		if (p->idx != idx)
			continue;

		 
		if (p->con_id && (!con_id || strcmp(p->con_id, con_id)))
			continue;

		if (p->chip_hwnum == U16_MAX) {
			desc = gpio_name_to_desc(p->key);
			if (desc) {
				*flags = p->flags;
				return desc;
			}

			dev_warn(dev, "cannot find GPIO line %s, deferring\n",
				 p->key);
			return ERR_PTR(-EPROBE_DEFER);
		}

		gc = find_chip_by_name(p->key);

		if (!gc) {
			 
			dev_warn(dev, "cannot find GPIO chip %s, deferring\n",
				 p->key);
			return ERR_PTR(-EPROBE_DEFER);
		}

		if (gc->ngpio <= p->chip_hwnum) {
			dev_err(dev,
				"requested GPIO %u (%u) is out of range [0..%u] for chip %s\n",
				idx, p->chip_hwnum, gc->ngpio - 1,
				gc->label);
			return ERR_PTR(-EINVAL);
		}

		desc = gpiochip_get_desc(gc, p->chip_hwnum);
		*flags = p->flags;

		return desc;
	}

	return desc;
}

static int platform_gpio_count(struct device *dev, const char *con_id)
{
	struct gpiod_lookup_table *table;
	struct gpiod_lookup *p;
	unsigned int count = 0;

	table = gpiod_find_lookup_table(dev);
	if (!table)
		return -ENOENT;

	for (p = &table->table[0]; p->key; p++) {
		if ((con_id && p->con_id && !strcmp(con_id, p->con_id)) ||
		    (!con_id && !p->con_id))
			count++;
	}
	if (!count)
		return -ENOENT;

	return count;
}

static struct gpio_desc *gpiod_find_by_fwnode(struct fwnode_handle *fwnode,
					      struct device *consumer,
					      const char *con_id,
					      unsigned int idx,
					      enum gpiod_flags *flags,
					      unsigned long *lookupflags)
{
	struct gpio_desc *desc = ERR_PTR(-ENOENT);

	if (is_of_node(fwnode)) {
		dev_dbg(consumer, "using DT '%pfw' for '%s' GPIO lookup\n",
			fwnode, con_id);
		desc = of_find_gpio(to_of_node(fwnode), con_id, idx, lookupflags);
	} else if (is_acpi_node(fwnode)) {
		dev_dbg(consumer, "using ACPI '%pfw' for '%s' GPIO lookup\n",
			fwnode, con_id);
		desc = acpi_find_gpio(fwnode, con_id, idx, flags, lookupflags);
	} else if (is_software_node(fwnode)) {
		dev_dbg(consumer, "using swnode '%pfw' for '%s' GPIO lookup\n",
			fwnode, con_id);
		desc = swnode_find_gpio(fwnode, con_id, idx, lookupflags);
	}

	return desc;
}

static struct gpio_desc *gpiod_find_and_request(struct device *consumer,
						struct fwnode_handle *fwnode,
						const char *con_id,
						unsigned int idx,
						enum gpiod_flags flags,
						const char *label,
						bool platform_lookup_allowed)
{
	unsigned long lookupflags = GPIO_LOOKUP_FLAGS_DEFAULT;
	struct gpio_desc *desc;
	int ret;

	desc = gpiod_find_by_fwnode(fwnode, consumer, con_id, idx, &flags, &lookupflags);
	if (gpiod_not_found(desc) && platform_lookup_allowed) {
		 
		dev_dbg(consumer, "using lookup tables for GPIO lookup\n");
		desc = gpiod_find(consumer, con_id, idx, &lookupflags);
	}

	if (IS_ERR(desc)) {
		dev_dbg(consumer, "No GPIO consumer %s found\n", con_id);
		return desc;
	}

	 
	ret = gpiod_request(desc, label);
	if (ret) {
		if (!(ret == -EBUSY && flags & GPIOD_FLAGS_BIT_NONEXCLUSIVE))
			return ERR_PTR(ret);

		 
		dev_info(consumer,
			 "nonexclusive access to GPIO for %s\n", con_id);
		return desc;
	}

	ret = gpiod_configure_flags(desc, con_id, lookupflags, flags);
	if (ret < 0) {
		dev_dbg(consumer, "setup of GPIO %s failed\n", con_id);
		gpiod_put(desc);
		return ERR_PTR(ret);
	}

	gpiod_line_state_notify(desc, GPIOLINE_CHANGED_REQUESTED);

	return desc;
}

 
struct gpio_desc *fwnode_gpiod_get_index(struct fwnode_handle *fwnode,
					 const char *con_id,
					 int index,
					 enum gpiod_flags flags,
					 const char *label)
{
	return gpiod_find_and_request(NULL, fwnode, con_id, index, flags, label, false);
}
EXPORT_SYMBOL_GPL(fwnode_gpiod_get_index);

 
int gpiod_count(struct device *dev, const char *con_id)
{
	const struct fwnode_handle *fwnode = dev ? dev_fwnode(dev) : NULL;
	int count = -ENOENT;

	if (is_of_node(fwnode))
		count = of_gpio_get_count(dev, con_id);
	else if (is_acpi_node(fwnode))
		count = acpi_gpio_count(dev, con_id);
	else if (is_software_node(fwnode))
		count = swnode_gpio_count(fwnode, con_id);

	if (count < 0)
		count = platform_gpio_count(dev, con_id);

	return count;
}
EXPORT_SYMBOL_GPL(gpiod_count);

 
struct gpio_desc *__must_check gpiod_get(struct device *dev, const char *con_id,
					 enum gpiod_flags flags)
{
	return gpiod_get_index(dev, con_id, 0, flags);
}
EXPORT_SYMBOL_GPL(gpiod_get);

 
struct gpio_desc *__must_check gpiod_get_optional(struct device *dev,
						  const char *con_id,
						  enum gpiod_flags flags)
{
	return gpiod_get_index_optional(dev, con_id, 0, flags);
}
EXPORT_SYMBOL_GPL(gpiod_get_optional);


 
int gpiod_configure_flags(struct gpio_desc *desc, const char *con_id,
		unsigned long lflags, enum gpiod_flags dflags)
{
	int ret;

	if (lflags & GPIO_ACTIVE_LOW)
		set_bit(FLAG_ACTIVE_LOW, &desc->flags);

	if (lflags & GPIO_OPEN_DRAIN)
		set_bit(FLAG_OPEN_DRAIN, &desc->flags);
	else if (dflags & GPIOD_FLAGS_BIT_OPEN_DRAIN) {
		 
		set_bit(FLAG_OPEN_DRAIN, &desc->flags);
		gpiod_warn(desc,
			   "enforced open drain please flag it properly in DT/ACPI DSDT/board file\n");
	}

	if (lflags & GPIO_OPEN_SOURCE)
		set_bit(FLAG_OPEN_SOURCE, &desc->flags);

	if (((lflags & GPIO_PULL_UP) && (lflags & GPIO_PULL_DOWN)) ||
	    ((lflags & GPIO_PULL_UP) && (lflags & GPIO_PULL_DISABLE)) ||
	    ((lflags & GPIO_PULL_DOWN) && (lflags & GPIO_PULL_DISABLE))) {
		gpiod_err(desc,
			  "multiple pull-up, pull-down or pull-disable enabled, invalid configuration\n");
		return -EINVAL;
	}

	if (lflags & GPIO_PULL_UP)
		set_bit(FLAG_PULL_UP, &desc->flags);
	else if (lflags & GPIO_PULL_DOWN)
		set_bit(FLAG_PULL_DOWN, &desc->flags);
	else if (lflags & GPIO_PULL_DISABLE)
		set_bit(FLAG_BIAS_DISABLE, &desc->flags);

	ret = gpiod_set_transitory(desc, (lflags & GPIO_TRANSITORY));
	if (ret < 0)
		return ret;

	 
	if (!(dflags & GPIOD_FLAGS_BIT_DIR_SET)) {
		gpiod_dbg(desc, "no flags found for %s\n", con_id);
		return 0;
	}

	 
	if (dflags & GPIOD_FLAGS_BIT_DIR_OUT)
		ret = gpiod_direction_output(desc,
				!!(dflags & GPIOD_FLAGS_BIT_DIR_VAL));
	else
		ret = gpiod_direction_input(desc);

	return ret;
}

 
struct gpio_desc *__must_check gpiod_get_index(struct device *dev,
					       const char *con_id,
					       unsigned int idx,
					       enum gpiod_flags flags)
{
	struct fwnode_handle *fwnode = dev ? dev_fwnode(dev) : NULL;
	const char *devname = dev ? dev_name(dev) : "?";
	const char *label = con_id ?: devname;

	return gpiod_find_and_request(dev, fwnode, con_id, idx, flags, label, true);
}
EXPORT_SYMBOL_GPL(gpiod_get_index);

 
struct gpio_desc *__must_check gpiod_get_index_optional(struct device *dev,
							const char *con_id,
							unsigned int index,
							enum gpiod_flags flags)
{
	struct gpio_desc *desc;

	desc = gpiod_get_index(dev, con_id, index, flags);
	if (gpiod_not_found(desc))
		return NULL;

	return desc;
}
EXPORT_SYMBOL_GPL(gpiod_get_index_optional);

 
int gpiod_hog(struct gpio_desc *desc, const char *name,
	      unsigned long lflags, enum gpiod_flags dflags)
{
	struct gpio_chip *gc;
	struct gpio_desc *local_desc;
	int hwnum;
	int ret;

	gc = gpiod_to_chip(desc);
	hwnum = gpio_chip_hwgpio(desc);

	local_desc = gpiochip_request_own_desc(gc, hwnum, name,
					       lflags, dflags);
	if (IS_ERR(local_desc)) {
		ret = PTR_ERR(local_desc);
		pr_err("requesting hog GPIO %s (chip %s, offset %d) failed, %d\n",
		       name, gc->label, hwnum, ret);
		return ret;
	}

	 
	set_bit(FLAG_IS_HOGGED, &desc->flags);

	gpiod_dbg(desc, "hogged as %s%s\n",
		(dflags & GPIOD_FLAGS_BIT_DIR_OUT) ? "output" : "input",
		(dflags & GPIOD_FLAGS_BIT_DIR_OUT) ?
		  (dflags & GPIOD_FLAGS_BIT_DIR_VAL) ? "/high" : "/low" : "");

	return 0;
}

 
static void gpiochip_free_hogs(struct gpio_chip *gc)
{
	struct gpio_desc *desc;

	for_each_gpio_desc_with_flag(gc, desc, FLAG_IS_HOGGED)
		gpiochip_free_own_desc(desc);
}

 
struct gpio_descs *__must_check gpiod_get_array(struct device *dev,
						const char *con_id,
						enum gpiod_flags flags)
{
	struct gpio_desc *desc;
	struct gpio_descs *descs;
	struct gpio_array *array_info = NULL;
	struct gpio_chip *gc;
	int count, bitmap_size;
	size_t descs_size;

	count = gpiod_count(dev, con_id);
	if (count < 0)
		return ERR_PTR(count);

	descs_size = struct_size(descs, desc, count);
	descs = kzalloc(descs_size, GFP_KERNEL);
	if (!descs)
		return ERR_PTR(-ENOMEM);

	for (descs->ndescs = 0; descs->ndescs < count; descs->ndescs++) {
		desc = gpiod_get_index(dev, con_id, descs->ndescs, flags);
		if (IS_ERR(desc)) {
			gpiod_put_array(descs);
			return ERR_CAST(desc);
		}

		descs->desc[descs->ndescs] = desc;

		gc = gpiod_to_chip(desc);
		 
		if (descs->ndescs == 0 && gpio_chip_hwgpio(desc) == 0) {
			struct gpio_descs *array;

			bitmap_size = BITS_TO_LONGS(gc->ngpio > count ?
						    gc->ngpio : count);

			array = krealloc(descs, descs_size +
					 struct_size(array_info, invert_mask, 3 * bitmap_size),
					 GFP_KERNEL | __GFP_ZERO);
			if (!array) {
				gpiod_put_array(descs);
				return ERR_PTR(-ENOMEM);
			}

			descs = array;

			array_info = (void *)descs + descs_size;
			array_info->get_mask = array_info->invert_mask +
						  bitmap_size;
			array_info->set_mask = array_info->get_mask +
						  bitmap_size;

			array_info->desc = descs->desc;
			array_info->size = count;
			array_info->chip = gc;
			bitmap_set(array_info->get_mask, descs->ndescs,
				   count - descs->ndescs);
			bitmap_set(array_info->set_mask, descs->ndescs,
				   count - descs->ndescs);
			descs->info = array_info;
		}

		 
		if (!array_info)
			continue;

		 
		if (array_info->chip != gc) {
			__clear_bit(descs->ndescs, array_info->get_mask);
			__clear_bit(descs->ndescs, array_info->set_mask);
		}
		 
		else if (gpio_chip_hwgpio(desc) != descs->ndescs) {
			 
			if (bitmap_full(array_info->get_mask, descs->ndescs)) {
				array_info = NULL;
			} else {
				__clear_bit(descs->ndescs,
					    array_info->get_mask);
				__clear_bit(descs->ndescs,
					    array_info->set_mask);
			}
		} else {
			 
			if (gpiochip_line_is_open_drain(gc, descs->ndescs) ||
			    gpiochip_line_is_open_source(gc, descs->ndescs))
				__clear_bit(descs->ndescs,
					    array_info->set_mask);
			 
			if (gpiod_is_active_low(desc))
				__set_bit(descs->ndescs,
					  array_info->invert_mask);
		}
	}
	if (array_info)
		dev_dbg(dev,
			"GPIO array info: chip=%s, size=%d, get_mask=%lx, set_mask=%lx, invert_mask=%lx\n",
			array_info->chip->label, array_info->size,
			*array_info->get_mask, *array_info->set_mask,
			*array_info->invert_mask);
	return descs;
}
EXPORT_SYMBOL_GPL(gpiod_get_array);

 
struct gpio_descs *__must_check gpiod_get_array_optional(struct device *dev,
							const char *con_id,
							enum gpiod_flags flags)
{
	struct gpio_descs *descs;

	descs = gpiod_get_array(dev, con_id, flags);
	if (gpiod_not_found(descs))
		return NULL;

	return descs;
}
EXPORT_SYMBOL_GPL(gpiod_get_array_optional);

 
void gpiod_put(struct gpio_desc *desc)
{
	if (desc)
		gpiod_free(desc);
}
EXPORT_SYMBOL_GPL(gpiod_put);

 
void gpiod_put_array(struct gpio_descs *descs)
{
	unsigned int i;

	for (i = 0; i < descs->ndescs; i++)
		gpiod_put(descs->desc[i]);

	kfree(descs);
}
EXPORT_SYMBOL_GPL(gpiod_put_array);

static int gpio_stub_drv_probe(struct device *dev)
{
	 
	return 0;
}

static struct device_driver gpio_stub_drv = {
	.name = "gpio_stub_drv",
	.bus = &gpio_bus_type,
	.probe = gpio_stub_drv_probe,
};

static int __init gpiolib_dev_init(void)
{
	int ret;

	 
	ret = bus_register(&gpio_bus_type);
	if (ret < 0) {
		pr_err("gpiolib: could not register GPIO bus type\n");
		return ret;
	}

	ret = driver_register(&gpio_stub_drv);
	if (ret < 0) {
		pr_err("gpiolib: could not register GPIO stub driver\n");
		bus_unregister(&gpio_bus_type);
		return ret;
	}

	ret = alloc_chrdev_region(&gpio_devt, 0, GPIO_DEV_MAX, GPIOCHIP_NAME);
	if (ret < 0) {
		pr_err("gpiolib: failed to allocate char dev region\n");
		driver_unregister(&gpio_stub_drv);
		bus_unregister(&gpio_bus_type);
		return ret;
	}

	gpiolib_initialized = true;
	gpiochip_setup_devs();

#if IS_ENABLED(CONFIG_OF_DYNAMIC) && IS_ENABLED(CONFIG_OF_GPIO)
	WARN_ON(of_reconfig_notifier_register(&gpio_of_notifier));
#endif  

	return ret;
}
core_initcall(gpiolib_dev_init);

#ifdef CONFIG_DEBUG_FS

static void gpiolib_dbg_show(struct seq_file *s, struct gpio_device *gdev)
{
	struct gpio_chip	*gc = gdev->chip;
	struct gpio_desc	*desc;
	unsigned		gpio = gdev->base;
	int			value;
	bool			is_out;
	bool			is_irq;
	bool			active_low;

	for_each_gpio_desc(gc, desc) {
		if (test_bit(FLAG_REQUESTED, &desc->flags)) {
			gpiod_get_direction(desc);
			is_out = test_bit(FLAG_IS_OUT, &desc->flags);
			value = gpio_chip_get_value(gc, desc);
			is_irq = test_bit(FLAG_USED_AS_IRQ, &desc->flags);
			active_low = test_bit(FLAG_ACTIVE_LOW, &desc->flags);
			seq_printf(s, " gpio-%-3d (%-20.20s|%-20.20s) %s %s %s%s\n",
				   gpio, desc->name ?: "", desc->label,
				   is_out ? "out" : "in ",
				   value >= 0 ? (value ? "hi" : "lo") : "?  ",
				   is_irq ? "IRQ " : "",
				   active_low ? "ACTIVE LOW" : "");
		} else if (desc->name) {
			seq_printf(s, " gpio-%-3d (%-20.20s)\n", gpio, desc->name);
		}

		gpio++;
	}
}

static void *gpiolib_seq_start(struct seq_file *s, loff_t *pos)
{
	unsigned long flags;
	struct gpio_device *gdev = NULL;
	loff_t index = *pos;

	s->private = "";

	spin_lock_irqsave(&gpio_lock, flags);
	list_for_each_entry(gdev, &gpio_devices, list)
		if (index-- == 0) {
			spin_unlock_irqrestore(&gpio_lock, flags);
			return gdev;
		}
	spin_unlock_irqrestore(&gpio_lock, flags);

	return NULL;
}

static void *gpiolib_seq_next(struct seq_file *s, void *v, loff_t *pos)
{
	unsigned long flags;
	struct gpio_device *gdev = v;
	void *ret = NULL;

	spin_lock_irqsave(&gpio_lock, flags);
	if (list_is_last(&gdev->list, &gpio_devices))
		ret = NULL;
	else
		ret = list_first_entry(&gdev->list, struct gpio_device, list);
	spin_unlock_irqrestore(&gpio_lock, flags);

	s->private = "\n";
	++*pos;

	return ret;
}

static void gpiolib_seq_stop(struct seq_file *s, void *v)
{
}

static int gpiolib_seq_show(struct seq_file *s, void *v)
{
	struct gpio_device *gdev = v;
	struct gpio_chip *gc = gdev->chip;
	struct device *parent;

	if (!gc) {
		seq_printf(s, "%s%s: (dangling chip)", (char *)s->private,
			   dev_name(&gdev->dev));
		return 0;
	}

	seq_printf(s, "%s%s: GPIOs %d-%d", (char *)s->private,
		   dev_name(&gdev->dev),
		   gdev->base, gdev->base + gdev->ngpio - 1);
	parent = gc->parent;
	if (parent)
		seq_printf(s, ", parent: %s/%s",
			   parent->bus ? parent->bus->name : "no-bus",
			   dev_name(parent));
	if (gc->label)
		seq_printf(s, ", %s", gc->label);
	if (gc->can_sleep)
		seq_printf(s, ", can sleep");
	seq_printf(s, ":\n");

	if (gc->dbg_show)
		gc->dbg_show(s, gc);
	else
		gpiolib_dbg_show(s, gdev);

	return 0;
}

static const struct seq_operations gpiolib_sops = {
	.start = gpiolib_seq_start,
	.next = gpiolib_seq_next,
	.stop = gpiolib_seq_stop,
	.show = gpiolib_seq_show,
};
DEFINE_SEQ_ATTRIBUTE(gpiolib);

static int __init gpiolib_debugfs_init(void)
{
	 
	debugfs_create_file("gpio", 0444, NULL, NULL, &gpiolib_fops);
	return 0;
}
subsys_initcall(gpiolib_debugfs_init);

#endif	 
