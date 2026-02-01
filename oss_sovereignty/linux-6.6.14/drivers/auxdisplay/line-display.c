
 

#include <generated/utsrelease.h>

#include <linux/device.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/sysfs.h>
#include <linux/timer.h>

#include "line-display.h"

#define DEFAULT_SCROLL_RATE	(HZ / 2)

 
static void linedisp_scroll(struct timer_list *t)
{
	struct linedisp *linedisp = from_timer(linedisp, t, timer);
	unsigned int i, ch = linedisp->scroll_pos;
	unsigned int num_chars = linedisp->num_chars;

	 
	for (i = 0; i < num_chars;) {
		 
		for (; i < num_chars && ch < linedisp->message_len; i++, ch++)
			linedisp->buf[i] = linedisp->message[ch];

		 
		ch = 0;
	}

	 
	linedisp->update(linedisp);

	 
	linedisp->scroll_pos++;
	linedisp->scroll_pos %= linedisp->message_len;

	 
	if (linedisp->message_len > num_chars && linedisp->scroll_rate)
		mod_timer(&linedisp->timer, jiffies + linedisp->scroll_rate);
}

 
static int linedisp_display(struct linedisp *linedisp, const char *msg,
			    ssize_t count)
{
	char *new_msg;

	 
	del_timer_sync(&linedisp->timer);

	if (count == -1)
		count = strlen(msg);

	 
	if (msg[count - 1] == '\n')
		count--;

	if (!count) {
		 
		kfree(linedisp->message);
		linedisp->message = NULL;
		linedisp->message_len = 0;
		memset(linedisp->buf, ' ', linedisp->num_chars);
		linedisp->update(linedisp);
		return 0;
	}

	new_msg = kmemdup_nul(msg, count, GFP_KERNEL);
	if (!new_msg)
		return -ENOMEM;

	kfree(linedisp->message);

	linedisp->message = new_msg;
	linedisp->message_len = count;
	linedisp->scroll_pos = 0;

	 
	linedisp_scroll(&linedisp->timer);

	return 0;
}

 
static ssize_t message_show(struct device *dev, struct device_attribute *attr,
			    char *buf)
{
	struct linedisp *linedisp = container_of(dev, struct linedisp, dev);

	return sysfs_emit(buf, "%s\n", linedisp->message);
}

 
static ssize_t message_store(struct device *dev, struct device_attribute *attr,
			     const char *buf, size_t count)
{
	struct linedisp *linedisp = container_of(dev, struct linedisp, dev);
	int err;

	err = linedisp_display(linedisp, buf, count);
	return err ?: count;
}

static DEVICE_ATTR_RW(message);

static ssize_t scroll_step_ms_show(struct device *dev,
				   struct device_attribute *attr, char *buf)
{
	struct linedisp *linedisp = container_of(dev, struct linedisp, dev);

	return sysfs_emit(buf, "%u\n", jiffies_to_msecs(linedisp->scroll_rate));
}

static ssize_t scroll_step_ms_store(struct device *dev,
				    struct device_attribute *attr,
				    const char *buf, size_t count)
{
	struct linedisp *linedisp = container_of(dev, struct linedisp, dev);
	unsigned int ms;

	if (kstrtouint(buf, 10, &ms) != 0)
		return -EINVAL;

	linedisp->scroll_rate = msecs_to_jiffies(ms);
	if (linedisp->message && linedisp->message_len > linedisp->num_chars) {
		del_timer_sync(&linedisp->timer);
		if (linedisp->scroll_rate)
			linedisp_scroll(&linedisp->timer);
	}

	return count;
}

static DEVICE_ATTR_RW(scroll_step_ms);

static struct attribute *linedisp_attrs[] = {
	&dev_attr_message.attr,
	&dev_attr_scroll_step_ms.attr,
	NULL,
};
ATTRIBUTE_GROUPS(linedisp);

static const struct device_type linedisp_type = {
	.groups	= linedisp_groups,
};

 
int linedisp_register(struct linedisp *linedisp, struct device *parent,
		      unsigned int num_chars, char *buf,
		      void (*update)(struct linedisp *linedisp))
{
	static atomic_t linedisp_id = ATOMIC_INIT(-1);
	int err;

	memset(linedisp, 0, sizeof(*linedisp));
	linedisp->dev.parent = parent;
	linedisp->dev.type = &linedisp_type;
	linedisp->update = update;
	linedisp->buf = buf;
	linedisp->num_chars = num_chars;
	linedisp->scroll_rate = DEFAULT_SCROLL_RATE;

	device_initialize(&linedisp->dev);
	dev_set_name(&linedisp->dev, "linedisp.%lu",
		     (unsigned long)atomic_inc_return(&linedisp_id));

	 
	timer_setup(&linedisp->timer, linedisp_scroll, 0);

	err = device_add(&linedisp->dev);
	if (err)
		goto out_del_timer;

	 
	err = linedisp_display(linedisp, "Linux " UTS_RELEASE "       ", -1);
	if (err)
		goto out_del_dev;

	return 0;

out_del_dev:
	device_del(&linedisp->dev);
out_del_timer:
	del_timer_sync(&linedisp->timer);
	put_device(&linedisp->dev);
	return err;
}
EXPORT_SYMBOL_GPL(linedisp_register);

 
void linedisp_unregister(struct linedisp *linedisp)
{
	device_del(&linedisp->dev);
	del_timer_sync(&linedisp->timer);
	kfree(linedisp->message);
	put_device(&linedisp->dev);
}
EXPORT_SYMBOL_GPL(linedisp_unregister);

MODULE_LICENSE("GPL");
