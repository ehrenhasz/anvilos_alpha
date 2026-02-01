
 

#include <linux/module.h>
#include <linux/errno.h>
#include <linux/rwsem.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/usb.h>

#include "usb.h"

#define MAX_USB_MINORS	256
static const struct file_operations *usb_minors[MAX_USB_MINORS];
static DECLARE_RWSEM(minor_rwsem);

static int usb_open(struct inode *inode, struct file *file)
{
	int err = -ENODEV;
	const struct file_operations *new_fops;

	down_read(&minor_rwsem);
	new_fops = fops_get(usb_minors[iminor(inode)]);

	if (!new_fops)
		goto done;

	replace_fops(file, new_fops);
	 
	if (file->f_op->open)
		err = file->f_op->open(inode, file);
 done:
	up_read(&minor_rwsem);
	return err;
}

static const struct file_operations usb_fops = {
	.owner =	THIS_MODULE,
	.open =		usb_open,
	.llseek =	noop_llseek,
};

static char *usb_devnode(const struct device *dev, umode_t *mode)
{
	struct usb_class_driver *drv;

	drv = dev_get_drvdata(dev);
	if (!drv || !drv->devnode)
		return NULL;
	return drv->devnode(dev, mode);
}

const struct class usbmisc_class = {
	.name		= "usbmisc",
	.devnode	= usb_devnode,
};

int usb_major_init(void)
{
	int error;

	error = register_chrdev(USB_MAJOR, "usb", &usb_fops);
	if (error)
		printk(KERN_ERR "Unable to get major %d for usb devices\n",
		       USB_MAJOR);

	return error;
}

void usb_major_cleanup(void)
{
	unregister_chrdev(USB_MAJOR, "usb");
}

 
int usb_register_dev(struct usb_interface *intf,
		     struct usb_class_driver *class_driver)
{
	int retval = 0;
	int minor_base = class_driver->minor_base;
	int minor;
	char name[20];

#ifdef CONFIG_USB_DYNAMIC_MINORS
	 
	minor_base = 0;
#endif

	if (class_driver->fops == NULL)
		return -EINVAL;
	if (intf->minor >= 0)
		return -EADDRINUSE;

	dev_dbg(&intf->dev, "looking for a minor, starting at %d\n", minor_base);

	down_write(&minor_rwsem);
	for (minor = minor_base; minor < MAX_USB_MINORS; ++minor) {
		if (usb_minors[minor])
			continue;

		usb_minors[minor] = class_driver->fops;
		intf->minor = minor;
		break;
	}
	if (intf->minor < 0) {
		up_write(&minor_rwsem);
		return -EXFULL;
	}

	 
	snprintf(name, sizeof(name), class_driver->name, minor - minor_base);
	intf->usb_dev = device_create(&usbmisc_class, &intf->dev,
				      MKDEV(USB_MAJOR, minor), class_driver,
				      "%s", kbasename(name));
	if (IS_ERR(intf->usb_dev)) {
		usb_minors[minor] = NULL;
		intf->minor = -1;
		retval = PTR_ERR(intf->usb_dev);
	}
	up_write(&minor_rwsem);
	return retval;
}
EXPORT_SYMBOL_GPL(usb_register_dev);

 
void usb_deregister_dev(struct usb_interface *intf,
			struct usb_class_driver *class_driver)
{
	if (intf->minor == -1)
		return;

	dev_dbg(&intf->dev, "removing %d minor\n", intf->minor);
	device_destroy(&usbmisc_class, MKDEV(USB_MAJOR, intf->minor));

	down_write(&minor_rwsem);
	usb_minors[intf->minor] = NULL;
	up_write(&minor_rwsem);

	intf->usb_dev = NULL;
	intf->minor = -1;
}
EXPORT_SYMBOL_GPL(usb_deregister_dev);
