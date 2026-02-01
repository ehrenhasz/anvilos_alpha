
 

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/module.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/lcd.h>
#include <linux/notifier.h>
#include <linux/ctype.h>
#include <linux/err.h>
#include <linux/fb.h>
#include <linux/slab.h>

#if defined(CONFIG_FB) || (defined(CONFIG_FB_MODULE) && \
			   defined(CONFIG_LCD_CLASS_DEVICE_MODULE))
 
static int fb_notifier_callback(struct notifier_block *self,
				 unsigned long event, void *data)
{
	struct lcd_device *ld;
	struct fb_event *evdata = data;

	ld = container_of(self, struct lcd_device, fb_notif);
	if (!ld->ops)
		return 0;

	mutex_lock(&ld->ops_lock);
	if (!ld->ops->check_fb || ld->ops->check_fb(ld, evdata->info)) {
		if (event == FB_EVENT_BLANK) {
			if (ld->ops->set_power)
				ld->ops->set_power(ld, *(int *)evdata->data);
		} else {
			if (ld->ops->set_mode)
				ld->ops->set_mode(ld, evdata->data);
		}
	}
	mutex_unlock(&ld->ops_lock);
	return 0;
}

static int lcd_register_fb(struct lcd_device *ld)
{
	memset(&ld->fb_notif, 0, sizeof(ld->fb_notif));
	ld->fb_notif.notifier_call = fb_notifier_callback;
	return fb_register_client(&ld->fb_notif);
}

static void lcd_unregister_fb(struct lcd_device *ld)
{
	fb_unregister_client(&ld->fb_notif);
}
#else
static int lcd_register_fb(struct lcd_device *ld)
{
	return 0;
}

static inline void lcd_unregister_fb(struct lcd_device *ld)
{
}
#endif  

static ssize_t lcd_power_show(struct device *dev, struct device_attribute *attr,
		char *buf)
{
	int rc;
	struct lcd_device *ld = to_lcd_device(dev);

	mutex_lock(&ld->ops_lock);
	if (ld->ops && ld->ops->get_power)
		rc = sprintf(buf, "%d\n", ld->ops->get_power(ld));
	else
		rc = -ENXIO;
	mutex_unlock(&ld->ops_lock);

	return rc;
}

static ssize_t lcd_power_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int rc;
	struct lcd_device *ld = to_lcd_device(dev);
	unsigned long power;

	rc = kstrtoul(buf, 0, &power);
	if (rc)
		return rc;

	rc = -ENXIO;

	mutex_lock(&ld->ops_lock);
	if (ld->ops && ld->ops->set_power) {
		pr_debug("set power to %lu\n", power);
		ld->ops->set_power(ld, power);
		rc = count;
	}
	mutex_unlock(&ld->ops_lock);

	return rc;
}
static DEVICE_ATTR_RW(lcd_power);

static ssize_t contrast_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int rc = -ENXIO;
	struct lcd_device *ld = to_lcd_device(dev);

	mutex_lock(&ld->ops_lock);
	if (ld->ops && ld->ops->get_contrast)
		rc = sprintf(buf, "%d\n", ld->ops->get_contrast(ld));
	mutex_unlock(&ld->ops_lock);

	return rc;
}

static ssize_t contrast_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int rc;
	struct lcd_device *ld = to_lcd_device(dev);
	unsigned long contrast;

	rc = kstrtoul(buf, 0, &contrast);
	if (rc)
		return rc;

	rc = -ENXIO;

	mutex_lock(&ld->ops_lock);
	if (ld->ops && ld->ops->set_contrast) {
		pr_debug("set contrast to %lu\n", contrast);
		ld->ops->set_contrast(ld, contrast);
		rc = count;
	}
	mutex_unlock(&ld->ops_lock);

	return rc;
}
static DEVICE_ATTR_RW(contrast);

static ssize_t max_contrast_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct lcd_device *ld = to_lcd_device(dev);

	return sprintf(buf, "%d\n", ld->props.max_contrast);
}
static DEVICE_ATTR_RO(max_contrast);

static struct class *lcd_class;

static void lcd_device_release(struct device *dev)
{
	struct lcd_device *ld = to_lcd_device(dev);
	kfree(ld);
}

static struct attribute *lcd_device_attrs[] = {
	&dev_attr_lcd_power.attr,
	&dev_attr_contrast.attr,
	&dev_attr_max_contrast.attr,
	NULL,
};
ATTRIBUTE_GROUPS(lcd_device);

 
struct lcd_device *lcd_device_register(const char *name, struct device *parent,
		void *devdata, struct lcd_ops *ops)
{
	struct lcd_device *new_ld;
	int rc;

	pr_debug("lcd_device_register: name=%s\n", name);

	new_ld = kzalloc(sizeof(struct lcd_device), GFP_KERNEL);
	if (!new_ld)
		return ERR_PTR(-ENOMEM);

	mutex_init(&new_ld->ops_lock);
	mutex_init(&new_ld->update_lock);

	new_ld->dev.class = lcd_class;
	new_ld->dev.parent = parent;
	new_ld->dev.release = lcd_device_release;
	dev_set_name(&new_ld->dev, "%s", name);
	dev_set_drvdata(&new_ld->dev, devdata);

	new_ld->ops = ops;

	rc = device_register(&new_ld->dev);
	if (rc) {
		put_device(&new_ld->dev);
		return ERR_PTR(rc);
	}

	rc = lcd_register_fb(new_ld);
	if (rc) {
		device_unregister(&new_ld->dev);
		return ERR_PTR(rc);
	}

	return new_ld;
}
EXPORT_SYMBOL(lcd_device_register);

 
void lcd_device_unregister(struct lcd_device *ld)
{
	if (!ld)
		return;

	mutex_lock(&ld->ops_lock);
	ld->ops = NULL;
	mutex_unlock(&ld->ops_lock);
	lcd_unregister_fb(ld);

	device_unregister(&ld->dev);
}
EXPORT_SYMBOL(lcd_device_unregister);

static void devm_lcd_device_release(struct device *dev, void *res)
{
	struct lcd_device *lcd = *(struct lcd_device **)res;

	lcd_device_unregister(lcd);
}

static int devm_lcd_device_match(struct device *dev, void *res, void *data)
{
	struct lcd_device **r = res;

	return *r == data;
}

 
struct lcd_device *devm_lcd_device_register(struct device *dev,
		const char *name, struct device *parent,
		void *devdata, struct lcd_ops *ops)
{
	struct lcd_device **ptr, *lcd;

	ptr = devres_alloc(devm_lcd_device_release, sizeof(*ptr), GFP_KERNEL);
	if (!ptr)
		return ERR_PTR(-ENOMEM);

	lcd = lcd_device_register(name, parent, devdata, ops);
	if (!IS_ERR(lcd)) {
		*ptr = lcd;
		devres_add(dev, ptr);
	} else {
		devres_free(ptr);
	}

	return lcd;
}
EXPORT_SYMBOL(devm_lcd_device_register);

 
void devm_lcd_device_unregister(struct device *dev, struct lcd_device *ld)
{
	int rc;

	rc = devres_release(dev, devm_lcd_device_release,
				devm_lcd_device_match, ld);
	WARN_ON(rc);
}
EXPORT_SYMBOL(devm_lcd_device_unregister);


static void __exit lcd_class_exit(void)
{
	class_destroy(lcd_class);
}

static int __init lcd_class_init(void)
{
	lcd_class = class_create("lcd");
	if (IS_ERR(lcd_class)) {
		pr_warn("Unable to create backlight class; errno = %ld\n",
			PTR_ERR(lcd_class));
		return PTR_ERR(lcd_class);
	}

	lcd_class->dev_groups = lcd_device_groups;
	return 0;
}

 
postcore_initcall(lcd_class_init);
module_exit(lcd_class_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Jamey Hicks <jamey.hicks@hp.com>, Andrew Zabolotny <zap@homelink.ru>");
MODULE_DESCRIPTION("LCD Lowlevel Control Abstraction");
