
 
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/errno.h>
#include <linux/mutex.h>
#include <linux/rwsem.h>
#include <linux/uaccess.h>
#include <linux/usb.h>

#define DRIVER_VERSION "USBLCD Driver Version 1.05"

#define USBLCD_MINOR		144

#define IOCTL_GET_HARD_VERSION	1
#define IOCTL_GET_DRV_VERSION	2


static const struct usb_device_id id_table[] = {
	{ .idVendor = 0x10D2, .match_flags = USB_DEVICE_ID_MATCH_VENDOR, },
	{ },
};
MODULE_DEVICE_TABLE(usb, id_table);

struct usb_lcd {
	struct usb_device	*udev;			 
	struct usb_interface	*interface;		 
	unsigned char		*bulk_in_buffer;	 
	size_t			bulk_in_size;		 
	__u8			bulk_in_endpointAddr;	 
	__u8			bulk_out_endpointAddr;	 
	struct kref		kref;
	struct semaphore	limit_sem;		 
	struct usb_anchor	submitted;		 
	struct rw_semaphore	io_rwsem;
	unsigned long		disconnected:1;
};
#define to_lcd_dev(d) container_of(d, struct usb_lcd, kref)

#define USB_LCD_CONCURRENT_WRITES	5

static struct usb_driver lcd_driver;


static void lcd_delete(struct kref *kref)
{
	struct usb_lcd *dev = to_lcd_dev(kref);

	usb_put_dev(dev->udev);
	kfree(dev->bulk_in_buffer);
	kfree(dev);
}


static int lcd_open(struct inode *inode, struct file *file)
{
	struct usb_lcd *dev;
	struct usb_interface *interface;
	int subminor, r;

	subminor = iminor(inode);

	interface = usb_find_interface(&lcd_driver, subminor);
	if (!interface) {
		pr_err("USBLCD: %s - error, can't find device for minor %d\n",
		       __func__, subminor);
		return -ENODEV;
	}

	dev = usb_get_intfdata(interface);

	 
	kref_get(&dev->kref);

	 
	r = usb_autopm_get_interface(interface);
	if (r < 0) {
		kref_put(&dev->kref, lcd_delete);
		return r;
	}

	 
	file->private_data = dev;

	return 0;
}

static int lcd_release(struct inode *inode, struct file *file)
{
	struct usb_lcd *dev;

	dev = file->private_data;
	if (dev == NULL)
		return -ENODEV;

	 
	usb_autopm_put_interface(dev->interface);
	kref_put(&dev->kref, lcd_delete);
	return 0;
}

static ssize_t lcd_read(struct file *file, char __user * buffer,
			size_t count, loff_t *ppos)
{
	struct usb_lcd *dev;
	int retval = 0;
	int bytes_read;

	dev = file->private_data;

	down_read(&dev->io_rwsem);

	if (dev->disconnected) {
		retval = -ENODEV;
		goto out_up_io;
	}

	 
	retval = usb_bulk_msg(dev->udev,
			      usb_rcvbulkpipe(dev->udev,
					      dev->bulk_in_endpointAddr),
			      dev->bulk_in_buffer,
			      min(dev->bulk_in_size, count),
			      &bytes_read, 10000);

	 
	if (!retval) {
		if (copy_to_user(buffer, dev->bulk_in_buffer, bytes_read))
			retval = -EFAULT;
		else
			retval = bytes_read;
	}

out_up_io:
	up_read(&dev->io_rwsem);

	return retval;
}

static long lcd_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	struct usb_lcd *dev;
	u16 bcdDevice;
	char buf[30];

	dev = file->private_data;
	if (dev == NULL)
		return -ENODEV;

	switch (cmd) {
	case IOCTL_GET_HARD_VERSION:
		bcdDevice = le16_to_cpu((dev->udev)->descriptor.bcdDevice);
		sprintf(buf, "%1d%1d.%1d%1d",
			(bcdDevice & 0xF000)>>12,
			(bcdDevice & 0xF00)>>8,
			(bcdDevice & 0xF0)>>4,
			(bcdDevice & 0xF));
		if (copy_to_user((void __user *)arg, buf, strlen(buf)) != 0)
			return -EFAULT;
		break;
	case IOCTL_GET_DRV_VERSION:
		sprintf(buf, DRIVER_VERSION);
		if (copy_to_user((void __user *)arg, buf, strlen(buf)) != 0)
			return -EFAULT;
		break;
	default:
		return -ENOTTY;
	}

	return 0;
}

static void lcd_write_bulk_callback(struct urb *urb)
{
	struct usb_lcd *dev;
	int status = urb->status;

	dev = urb->context;

	 
	if (status &&
	    !(status == -ENOENT ||
	      status == -ECONNRESET ||
	      status == -ESHUTDOWN)) {
		dev_dbg(&dev->interface->dev,
			"nonzero write bulk status received: %d\n", status);
	}

	 
	usb_free_coherent(urb->dev, urb->transfer_buffer_length,
			  urb->transfer_buffer, urb->transfer_dma);
	up(&dev->limit_sem);
}

static ssize_t lcd_write(struct file *file, const char __user * user_buffer,
			 size_t count, loff_t *ppos)
{
	struct usb_lcd *dev;
	int retval = 0, r;
	struct urb *urb = NULL;
	char *buf = NULL;

	dev = file->private_data;

	 
	if (count == 0)
		goto exit;

	r = down_interruptible(&dev->limit_sem);
	if (r < 0)
		return -EINTR;

	down_read(&dev->io_rwsem);

	if (dev->disconnected) {
		retval = -ENODEV;
		goto err_up_io;
	}

	 
	urb = usb_alloc_urb(0, GFP_KERNEL);
	if (!urb) {
		retval = -ENOMEM;
		goto err_up_io;
	}

	buf = usb_alloc_coherent(dev->udev, count, GFP_KERNEL,
				 &urb->transfer_dma);
	if (!buf) {
		retval = -ENOMEM;
		goto error;
	}

	if (copy_from_user(buf, user_buffer, count)) {
		retval = -EFAULT;
		goto error;
	}

	 
	usb_fill_bulk_urb(urb, dev->udev,
			  usb_sndbulkpipe(dev->udev,
			  dev->bulk_out_endpointAddr),
			  buf, count, lcd_write_bulk_callback, dev);
	urb->transfer_flags |= URB_NO_TRANSFER_DMA_MAP;

	usb_anchor_urb(urb, &dev->submitted);

	 
	retval = usb_submit_urb(urb, GFP_KERNEL);
	if (retval) {
		dev_err(&dev->udev->dev,
			"%s - failed submitting write urb, error %d\n",
			__func__, retval);
		goto error_unanchor;
	}

	 
	usb_free_urb(urb);

	up_read(&dev->io_rwsem);
exit:
	return count;
error_unanchor:
	usb_unanchor_urb(urb);
error:
	usb_free_coherent(dev->udev, count, buf, urb->transfer_dma);
	usb_free_urb(urb);
err_up_io:
	up_read(&dev->io_rwsem);
	up(&dev->limit_sem);
	return retval;
}

static const struct file_operations lcd_fops = {
	.owner =        THIS_MODULE,
	.read =         lcd_read,
	.write =        lcd_write,
	.open =         lcd_open,
	.unlocked_ioctl = lcd_ioctl,
	.release =      lcd_release,
	.llseek =	 noop_llseek,
};

 
static struct usb_class_driver lcd_class = {
	.name =         "lcd%d",
	.fops =         &lcd_fops,
	.minor_base =   USBLCD_MINOR,
};

static int lcd_probe(struct usb_interface *interface,
		     const struct usb_device_id *id)
{
	struct usb_lcd *dev = NULL;
	struct usb_endpoint_descriptor *bulk_in, *bulk_out;
	int i;
	int retval;

	 
	dev = kzalloc(sizeof(*dev), GFP_KERNEL);
	if (!dev)
		return -ENOMEM;

	kref_init(&dev->kref);
	sema_init(&dev->limit_sem, USB_LCD_CONCURRENT_WRITES);
	init_rwsem(&dev->io_rwsem);
	init_usb_anchor(&dev->submitted);

	dev->udev = usb_get_dev(interface_to_usbdev(interface));
	dev->interface = interface;

	if (le16_to_cpu(dev->udev->descriptor.idProduct) != 0x0001) {
		dev_warn(&interface->dev, "USBLCD model not supported.\n");
		retval = -ENODEV;
		goto error;
	}

	 
	 
	retval = usb_find_common_endpoints(interface->cur_altsetting,
			&bulk_in, &bulk_out, NULL, NULL);
	if (retval) {
		dev_err(&interface->dev,
			"Could not find both bulk-in and bulk-out endpoints\n");
		goto error;
	}

	dev->bulk_in_size = usb_endpoint_maxp(bulk_in);
	dev->bulk_in_endpointAddr = bulk_in->bEndpointAddress;
	dev->bulk_in_buffer = kmalloc(dev->bulk_in_size, GFP_KERNEL);
	if (!dev->bulk_in_buffer) {
		retval = -ENOMEM;
		goto error;
	}

	dev->bulk_out_endpointAddr = bulk_out->bEndpointAddress;

	 
	usb_set_intfdata(interface, dev);

	 
	retval = usb_register_dev(interface, &lcd_class);
	if (retval) {
		 
		dev_err(&interface->dev,
			"Not able to get a minor for this device.\n");
		goto error;
	}

	i = le16_to_cpu(dev->udev->descriptor.bcdDevice);

	dev_info(&interface->dev, "USBLCD Version %1d%1d.%1d%1d found "
		 "at address %d\n", (i & 0xF000)>>12, (i & 0xF00)>>8,
		 (i & 0xF0)>>4, (i & 0xF), dev->udev->devnum);

	 
	dev_info(&interface->dev, "USB LCD device now attached to USBLCD-%d\n",
		 interface->minor);
	return 0;

error:
	kref_put(&dev->kref, lcd_delete);
	return retval;
}

static void lcd_draw_down(struct usb_lcd *dev)
{
	int time;

	time = usb_wait_anchor_empty_timeout(&dev->submitted, 1000);
	if (!time)
		usb_kill_anchored_urbs(&dev->submitted);
}

static int lcd_suspend(struct usb_interface *intf, pm_message_t message)
{
	struct usb_lcd *dev = usb_get_intfdata(intf);

	if (!dev)
		return 0;
	lcd_draw_down(dev);
	return 0;
}

static int lcd_resume(struct usb_interface *intf)
{
	return 0;
}

static void lcd_disconnect(struct usb_interface *interface)
{
	struct usb_lcd *dev = usb_get_intfdata(interface);
	int minor = interface->minor;

	 
	usb_deregister_dev(interface, &lcd_class);

	down_write(&dev->io_rwsem);
	dev->disconnected = 1;
	up_write(&dev->io_rwsem);

	usb_kill_anchored_urbs(&dev->submitted);

	 
	kref_put(&dev->kref, lcd_delete);

	dev_info(&interface->dev, "USB LCD #%d now disconnected\n", minor);
}

static struct usb_driver lcd_driver = {
	.name =		"usblcd",
	.probe =	lcd_probe,
	.disconnect =	lcd_disconnect,
	.suspend =	lcd_suspend,
	.resume =	lcd_resume,
	.id_table =	id_table,
	.supports_autosuspend = 1,
};

module_usb_driver(lcd_driver);

MODULE_AUTHOR("Georges Toth <g.toth@e-biz.lu>");
MODULE_DESCRIPTION(DRIVER_VERSION);
MODULE_LICENSE("GPL");
