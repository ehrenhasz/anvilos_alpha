

#include <linux/bitops.h>
#include <linux/device.h>
#include <linux/idr.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/kdev_t.h>
#include <linux/kstrtox.h>
#include <linux/list.h>
#include <linux/mutex.h>
#include <linux/printk.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/string.h>
#include <linux/sysfs.h>
#include <linux/types.h>

#include <linux/gpio/consumer.h>
#include <linux/gpio/driver.h>

#include "gpiolib.h"
#include "gpiolib-sysfs.h"

struct kernfs_node;

#define GPIO_IRQF_TRIGGER_NONE		0
#define GPIO_IRQF_TRIGGER_FALLING	BIT(0)
#define GPIO_IRQF_TRIGGER_RISING	BIT(1)
#define GPIO_IRQF_TRIGGER_BOTH		(GPIO_IRQF_TRIGGER_FALLING | \
					 GPIO_IRQF_TRIGGER_RISING)

struct gpiod_data {
	struct gpio_desc *desc;

	struct mutex mutex;
	struct kernfs_node *value_kn;
	int irq;
	unsigned char irq_flags;

	bool direction_can_change;
};

 
static DEFINE_MUTEX(sysfs_lock);

 

static ssize_t direction_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct gpiod_data *data = dev_get_drvdata(dev);
	struct gpio_desc *desc = data->desc;
	int value;

	mutex_lock(&data->mutex);

	gpiod_get_direction(desc);
	value = !!test_bit(FLAG_IS_OUT, &desc->flags);

	mutex_unlock(&data->mutex);

	return sysfs_emit(buf, "%s\n", value ? "out" : "in");
}

static ssize_t direction_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	struct gpiod_data *data = dev_get_drvdata(dev);
	struct gpio_desc *desc = data->desc;
	ssize_t			status;

	mutex_lock(&data->mutex);

	if (sysfs_streq(buf, "high"))
		status = gpiod_direction_output_raw(desc, 1);
	else if (sysfs_streq(buf, "out") || sysfs_streq(buf, "low"))
		status = gpiod_direction_output_raw(desc, 0);
	else if (sysfs_streq(buf, "in"))
		status = gpiod_direction_input(desc);
	else
		status = -EINVAL;

	mutex_unlock(&data->mutex);

	return status ? : size;
}
static DEVICE_ATTR_RW(direction);

static ssize_t value_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct gpiod_data *data = dev_get_drvdata(dev);
	struct gpio_desc *desc = data->desc;
	ssize_t			status;

	mutex_lock(&data->mutex);

	status = gpiod_get_value_cansleep(desc);

	mutex_unlock(&data->mutex);

	if (status < 0)
		return status;

	return sysfs_emit(buf, "%zd\n", status);
}

static ssize_t value_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	struct gpiod_data *data = dev_get_drvdata(dev);
	struct gpio_desc *desc = data->desc;
	ssize_t status;
	long value;

	status = kstrtol(buf, 0, &value);

	mutex_lock(&data->mutex);

	if (!test_bit(FLAG_IS_OUT, &desc->flags)) {
		status = -EPERM;
	} else if (status == 0) {
		gpiod_set_value_cansleep(desc, value);
		status = size;
	}

	mutex_unlock(&data->mutex);

	return status;
}
static DEVICE_ATTR_PREALLOC(value, S_IWUSR | S_IRUGO, value_show, value_store);

static irqreturn_t gpio_sysfs_irq(int irq, void *priv)
{
	struct gpiod_data *data = priv;

	sysfs_notify_dirent(data->value_kn);

	return IRQ_HANDLED;
}

 
static int gpio_sysfs_request_irq(struct device *dev, unsigned char flags)
{
	struct gpiod_data	*data = dev_get_drvdata(dev);
	struct gpio_desc	*desc = data->desc;
	unsigned long		irq_flags;
	int			ret;

	data->irq = gpiod_to_irq(desc);
	if (data->irq < 0)
		return -EIO;

	data->value_kn = sysfs_get_dirent(dev->kobj.sd, "value");
	if (!data->value_kn)
		return -ENODEV;

	irq_flags = IRQF_SHARED;
	if (flags & GPIO_IRQF_TRIGGER_FALLING)
		irq_flags |= test_bit(FLAG_ACTIVE_LOW, &desc->flags) ?
			IRQF_TRIGGER_RISING : IRQF_TRIGGER_FALLING;
	if (flags & GPIO_IRQF_TRIGGER_RISING)
		irq_flags |= test_bit(FLAG_ACTIVE_LOW, &desc->flags) ?
			IRQF_TRIGGER_FALLING : IRQF_TRIGGER_RISING;

	 
	ret = gpiochip_lock_as_irq(desc->gdev->chip, gpio_chip_hwgpio(desc));
	if (ret < 0)
		goto err_put_kn;

	ret = request_any_context_irq(data->irq, gpio_sysfs_irq, irq_flags,
				"gpiolib", data);
	if (ret < 0)
		goto err_unlock;

	data->irq_flags = flags;

	return 0;

err_unlock:
	gpiochip_unlock_as_irq(desc->gdev->chip, gpio_chip_hwgpio(desc));
err_put_kn:
	sysfs_put(data->value_kn);

	return ret;
}

 
static void gpio_sysfs_free_irq(struct device *dev)
{
	struct gpiod_data *data = dev_get_drvdata(dev);
	struct gpio_desc *desc = data->desc;

	data->irq_flags = 0;
	free_irq(data->irq, data);
	gpiochip_unlock_as_irq(desc->gdev->chip, gpio_chip_hwgpio(desc));
	sysfs_put(data->value_kn);
}

static const char * const trigger_names[] = {
	[GPIO_IRQF_TRIGGER_NONE]	= "none",
	[GPIO_IRQF_TRIGGER_FALLING]	= "falling",
	[GPIO_IRQF_TRIGGER_RISING]	= "rising",
	[GPIO_IRQF_TRIGGER_BOTH]	= "both",
};

static ssize_t edge_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct gpiod_data *data = dev_get_drvdata(dev);
	int flags;

	mutex_lock(&data->mutex);

	flags = data->irq_flags;

	mutex_unlock(&data->mutex);

	if (flags >= ARRAY_SIZE(trigger_names))
		return 0;

	return sysfs_emit(buf, "%s\n", trigger_names[flags]);
}

static ssize_t edge_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	struct gpiod_data *data = dev_get_drvdata(dev);
	ssize_t	status = size;
	int flags;

	flags = sysfs_match_string(trigger_names, buf);
	if (flags < 0)
		return flags;

	mutex_lock(&data->mutex);

	if (flags == data->irq_flags) {
		status = size;
		goto out_unlock;
	}

	if (data->irq_flags)
		gpio_sysfs_free_irq(dev);

	if (flags) {
		status = gpio_sysfs_request_irq(dev, flags);
		if (!status)
			status = size;
	}

out_unlock:
	mutex_unlock(&data->mutex);

	return status;
}
static DEVICE_ATTR_RW(edge);

 
static int gpio_sysfs_set_active_low(struct device *dev, int value)
{
	struct gpiod_data	*data = dev_get_drvdata(dev);
	struct gpio_desc	*desc = data->desc;
	int			status = 0;
	unsigned int		flags = data->irq_flags;

	if (!!test_bit(FLAG_ACTIVE_LOW, &desc->flags) == !!value)
		return 0;

	assign_bit(FLAG_ACTIVE_LOW, &desc->flags, value);

	 
	if (flags == GPIO_IRQF_TRIGGER_FALLING ||
					flags == GPIO_IRQF_TRIGGER_RISING) {
		gpio_sysfs_free_irq(dev);
		status = gpio_sysfs_request_irq(dev, flags);
	}

	return status;
}

static ssize_t active_low_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct gpiod_data *data = dev_get_drvdata(dev);
	struct gpio_desc *desc = data->desc;
	int value;

	mutex_lock(&data->mutex);

	value = !!test_bit(FLAG_ACTIVE_LOW, &desc->flags);

	mutex_unlock(&data->mutex);

	return sysfs_emit(buf, "%d\n", value);
}

static ssize_t active_low_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	struct gpiod_data	*data = dev_get_drvdata(dev);
	ssize_t			status;
	long			value;

	status = kstrtol(buf, 0, &value);
	if (status)
		return status;

	mutex_lock(&data->mutex);

	status = gpio_sysfs_set_active_low(dev, value);

	mutex_unlock(&data->mutex);

	return status ? : size;
}
static DEVICE_ATTR_RW(active_low);

static umode_t gpio_is_visible(struct kobject *kobj, struct attribute *attr,
			       int n)
{
	struct device *dev = kobj_to_dev(kobj);
	struct gpiod_data *data = dev_get_drvdata(dev);
	struct gpio_desc *desc = data->desc;
	umode_t mode = attr->mode;
	bool show_direction = data->direction_can_change;

	if (attr == &dev_attr_direction.attr) {
		if (!show_direction)
			mode = 0;
	} else if (attr == &dev_attr_edge.attr) {
		if (gpiod_to_irq(desc) < 0)
			mode = 0;
		if (!show_direction && test_bit(FLAG_IS_OUT, &desc->flags))
			mode = 0;
	}

	return mode;
}

static struct attribute *gpio_attrs[] = {
	&dev_attr_direction.attr,
	&dev_attr_edge.attr,
	&dev_attr_value.attr,
	&dev_attr_active_low.attr,
	NULL,
};

static const struct attribute_group gpio_group = {
	.attrs = gpio_attrs,
	.is_visible = gpio_is_visible,
};

static const struct attribute_group *gpio_groups[] = {
	&gpio_group,
	NULL
};

 

static ssize_t base_show(struct device *dev,
			       struct device_attribute *attr, char *buf)
{
	const struct gpio_chip	*chip = dev_get_drvdata(dev);

	return sysfs_emit(buf, "%d\n", chip->base);
}
static DEVICE_ATTR_RO(base);

static ssize_t label_show(struct device *dev,
			       struct device_attribute *attr, char *buf)
{
	const struct gpio_chip	*chip = dev_get_drvdata(dev);

	return sysfs_emit(buf, "%s\n", chip->label ?: "");
}
static DEVICE_ATTR_RO(label);

static ssize_t ngpio_show(struct device *dev,
			       struct device_attribute *attr, char *buf)
{
	const struct gpio_chip	*chip = dev_get_drvdata(dev);

	return sysfs_emit(buf, "%u\n", chip->ngpio);
}
static DEVICE_ATTR_RO(ngpio);

static struct attribute *gpiochip_attrs[] = {
	&dev_attr_base.attr,
	&dev_attr_label.attr,
	&dev_attr_ngpio.attr,
	NULL,
};
ATTRIBUTE_GROUPS(gpiochip);

 
static ssize_t export_store(const struct class *class,
				const struct class_attribute *attr,
				const char *buf, size_t len)
{
	long			gpio;
	struct gpio_desc	*desc;
	int			status;
	struct gpio_chip	*gc;
	int			offset;

	status = kstrtol(buf, 0, &gpio);
	if (status < 0)
		goto done;

	desc = gpio_to_desc(gpio);
	 
	if (!desc) {
		pr_warn("%s: invalid GPIO %ld\n", __func__, gpio);
		return -EINVAL;
	}
	gc = desc->gdev->chip;
	offset = gpio_chip_hwgpio(desc);
	if (!gpiochip_line_is_valid(gc, offset)) {
		pr_warn("%s: GPIO %ld masked\n", __func__, gpio);
		return -EINVAL;
	}

	 

	status = gpiod_request_user(desc, "sysfs");
	if (status)
		goto done;

	status = gpiod_set_transitory(desc, false);
	if (status) {
		gpiod_free(desc);
		goto done;
	}

	status = gpiod_export(desc, true);
	if (status < 0)
		gpiod_free(desc);
	else
		set_bit(FLAG_SYSFS, &desc->flags);

done:
	if (status)
		pr_debug("%s: status %d\n", __func__, status);
	return status ? : len;
}
static CLASS_ATTR_WO(export);

static ssize_t unexport_store(const struct class *class,
				const struct class_attribute *attr,
				const char *buf, size_t len)
{
	long			gpio;
	struct gpio_desc	*desc;
	int			status;

	status = kstrtol(buf, 0, &gpio);
	if (status < 0)
		goto done;

	desc = gpio_to_desc(gpio);
	 
	if (!desc) {
		pr_warn("%s: invalid GPIO %ld\n", __func__, gpio);
		return -EINVAL;
	}

	status = -EINVAL;

	 
	if (test_and_clear_bit(FLAG_SYSFS, &desc->flags)) {
		gpiod_unexport(desc);
		gpiod_free(desc);
		status = 0;
	}
done:
	if (status)
		pr_debug("%s: status %d\n", __func__, status);
	return status ? : len;
}
static CLASS_ATTR_WO(unexport);

static struct attribute *gpio_class_attrs[] = {
	&class_attr_export.attr,
	&class_attr_unexport.attr,
	NULL,
};
ATTRIBUTE_GROUPS(gpio_class);

static struct class gpio_class = {
	.name =		"gpio",
	.class_groups = gpio_class_groups,
};


 
int gpiod_export(struct gpio_desc *desc, bool direction_may_change)
{
	struct gpio_chip	*chip;
	struct gpio_device	*gdev;
	struct gpiod_data	*data;
	unsigned long		flags;
	int			status;
	const char		*ioname = NULL;
	struct device		*dev;
	int			offset;

	 
	if (!class_is_registered(&gpio_class)) {
		pr_debug("%s: called too early!\n", __func__);
		return -ENOENT;
	}

	if (!desc) {
		pr_debug("%s: invalid gpio descriptor\n", __func__);
		return -EINVAL;
	}

	gdev = desc->gdev;
	chip = gdev->chip;

	mutex_lock(&sysfs_lock);

	 
	if (!chip || !gdev->mockdev) {
		status = -ENODEV;
		goto err_unlock;
	}

	spin_lock_irqsave(&gpio_lock, flags);
	if (!test_bit(FLAG_REQUESTED, &desc->flags) ||
	     test_bit(FLAG_EXPORT, &desc->flags)) {
		spin_unlock_irqrestore(&gpio_lock, flags);
		gpiod_dbg(desc, "%s: unavailable (requested=%d, exported=%d)\n",
				__func__,
				test_bit(FLAG_REQUESTED, &desc->flags),
				test_bit(FLAG_EXPORT, &desc->flags));
		status = -EPERM;
		goto err_unlock;
	}
	spin_unlock_irqrestore(&gpio_lock, flags);

	data = kzalloc(sizeof(*data), GFP_KERNEL);
	if (!data) {
		status = -ENOMEM;
		goto err_unlock;
	}

	data->desc = desc;
	mutex_init(&data->mutex);
	if (chip->direction_input && chip->direction_output)
		data->direction_can_change = direction_may_change;
	else
		data->direction_can_change = false;

	offset = gpio_chip_hwgpio(desc);
	if (chip->names && chip->names[offset])
		ioname = chip->names[offset];

	dev = device_create_with_groups(&gpio_class, &gdev->dev,
					MKDEV(0, 0), data, gpio_groups,
					ioname ? ioname : "gpio%u",
					desc_to_gpio(desc));
	if (IS_ERR(dev)) {
		status = PTR_ERR(dev);
		goto err_free_data;
	}

	set_bit(FLAG_EXPORT, &desc->flags);
	mutex_unlock(&sysfs_lock);
	return 0;

err_free_data:
	kfree(data);
err_unlock:
	mutex_unlock(&sysfs_lock);
	gpiod_dbg(desc, "%s: status %d\n", __func__, status);
	return status;
}
EXPORT_SYMBOL_GPL(gpiod_export);

static int match_export(struct device *dev, const void *desc)
{
	struct gpiod_data *data = dev_get_drvdata(dev);

	return data->desc == desc;
}

 
int gpiod_export_link(struct device *dev, const char *name,
		      struct gpio_desc *desc)
{
	struct device *cdev;
	int ret;

	if (!desc) {
		pr_warn("%s: invalid GPIO\n", __func__);
		return -EINVAL;
	}

	cdev = class_find_device(&gpio_class, NULL, desc, match_export);
	if (!cdev)
		return -ENODEV;

	ret = sysfs_create_link(&dev->kobj, &cdev->kobj, name);
	put_device(cdev);

	return ret;
}
EXPORT_SYMBOL_GPL(gpiod_export_link);

 
void gpiod_unexport(struct gpio_desc *desc)
{
	struct gpiod_data *data;
	struct device *dev;

	if (!desc) {
		pr_warn("%s: invalid GPIO\n", __func__);
		return;
	}

	mutex_lock(&sysfs_lock);

	if (!test_bit(FLAG_EXPORT, &desc->flags))
		goto err_unlock;

	dev = class_find_device(&gpio_class, NULL, desc, match_export);
	if (!dev)
		goto err_unlock;

	data = dev_get_drvdata(dev);

	clear_bit(FLAG_EXPORT, &desc->flags);

	device_unregister(dev);

	 
	if (data->irq_flags)
		gpio_sysfs_free_irq(dev);

	mutex_unlock(&sysfs_lock);

	put_device(dev);
	kfree(data);

	return;

err_unlock:
	mutex_unlock(&sysfs_lock);
}
EXPORT_SYMBOL_GPL(gpiod_unexport);

int gpiochip_sysfs_register(struct gpio_device *gdev)
{
	struct device	*dev;
	struct device	*parent;
	struct gpio_chip *chip = gdev->chip;

	 
	if (!class_is_registered(&gpio_class))
		return 0;

	 
	if (chip->parent)
		parent = chip->parent;
	else
		parent = &gdev->dev;

	 
	dev = device_create_with_groups(&gpio_class, parent, MKDEV(0, 0), chip,
					gpiochip_groups, GPIOCHIP_NAME "%d",
					chip->base);
	if (IS_ERR(dev))
		return PTR_ERR(dev);

	mutex_lock(&sysfs_lock);
	gdev->mockdev = dev;
	mutex_unlock(&sysfs_lock);

	return 0;
}

void gpiochip_sysfs_unregister(struct gpio_device *gdev)
{
	struct gpio_desc *desc;
	struct gpio_chip *chip = gdev->chip;

	if (!gdev->mockdev)
		return;

	device_unregister(gdev->mockdev);

	 
	mutex_lock(&sysfs_lock);
	gdev->mockdev = NULL;
	mutex_unlock(&sysfs_lock);

	 
	for_each_gpio_desc_with_flag(chip, desc, FLAG_SYSFS) {
		gpiod_unexport(desc);
		gpiod_free(desc);
	}
}

static int __init gpiolib_sysfs_init(void)
{
	int		status;
	unsigned long	flags;
	struct gpio_device *gdev;

	status = class_register(&gpio_class);
	if (status < 0)
		return status;

	 
	spin_lock_irqsave(&gpio_lock, flags);
	list_for_each_entry(gdev, &gpio_devices, list) {
		if (gdev->mockdev)
			continue;

		 
		spin_unlock_irqrestore(&gpio_lock, flags);
		status = gpiochip_sysfs_register(gdev);
		spin_lock_irqsave(&gpio_lock, flags);
	}
	spin_unlock_irqrestore(&gpio_lock, flags);

	return status;
}
postcore_initcall(gpiolib_sysfs_init);
