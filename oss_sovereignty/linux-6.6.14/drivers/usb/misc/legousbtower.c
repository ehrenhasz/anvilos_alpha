
 

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/completion.h>
#include <linux/mutex.h>
#include <linux/uaccess.h>
#include <linux/usb.h>
#include <linux/poll.h>


#define DRIVER_AUTHOR "Juergen Stuber <starblue@sourceforge.net>"
#define DRIVER_DESC "LEGO USB Tower Driver"


 

 
static int read_buffer_size = 480;
module_param(read_buffer_size, int, 0);
MODULE_PARM_DESC(read_buffer_size, "Read buffer size");

 
static int write_buffer_size = 480;
module_param(write_buffer_size, int, 0);
MODULE_PARM_DESC(write_buffer_size, "Write buffer size");

 
static int packet_timeout = 50;
module_param(packet_timeout, int, 0);
MODULE_PARM_DESC(packet_timeout, "Packet timeout in ms");

 
static int read_timeout = 200;
module_param(read_timeout, int, 0);
MODULE_PARM_DESC(read_timeout, "Read timeout in ms");

 
static int interrupt_in_interval = 2;
module_param(interrupt_in_interval, int, 0);
MODULE_PARM_DESC(interrupt_in_interval, "Interrupt in interval in ms");

static int interrupt_out_interval = 8;
module_param(interrupt_out_interval, int, 0);
MODULE_PARM_DESC(interrupt_out_interval, "Interrupt out interval in ms");

 
#define LEGO_USB_TOWER_VENDOR_ID	0x0694
#define LEGO_USB_TOWER_PRODUCT_ID	0x0001

 
#define LEGO_USB_TOWER_REQUEST_RESET		0x04
#define LEGO_USB_TOWER_REQUEST_GET_VERSION	0xFD

struct tower_reset_reply {
	__le16 size;
	__u8 err_code;
	__u8 spare;
};

struct tower_get_version_reply {
	__le16 size;
	__u8 err_code;
	__u8 spare;
	__u8 major;
	__u8 minor;
	__le16 build_no;
};


 
static const struct usb_device_id tower_table[] = {
	{ USB_DEVICE(LEGO_USB_TOWER_VENDOR_ID, LEGO_USB_TOWER_PRODUCT_ID) },
	{ }					 
};

MODULE_DEVICE_TABLE(usb, tower_table);

#define LEGO_USB_TOWER_MINOR_BASE	160


 
struct lego_usb_tower {
	struct mutex		lock;		 
	struct usb_device	*udev;		 
	unsigned char		minor;		 

	int			open_count;	 
	unsigned long		disconnected:1;

	char			*read_buffer;
	size_t			read_buffer_length;  
	size_t			read_packet_length;  
	spinlock_t		read_buffer_lock;
	int			packet_timeout_jiffies;
	unsigned long		read_last_arrival;

	wait_queue_head_t	read_wait;
	wait_queue_head_t	write_wait;

	char			*interrupt_in_buffer;
	struct usb_endpoint_descriptor *interrupt_in_endpoint;
	struct urb		*interrupt_in_urb;
	int			interrupt_in_interval;
	int			interrupt_in_done;

	char			*interrupt_out_buffer;
	struct usb_endpoint_descriptor *interrupt_out_endpoint;
	struct urb		*interrupt_out_urb;
	int			interrupt_out_interval;
	int			interrupt_out_busy;

};


 
static ssize_t tower_read(struct file *file, char __user *buffer, size_t count, loff_t *ppos);
static ssize_t tower_write(struct file *file, const char __user *buffer, size_t count, loff_t *ppos);
static inline void tower_delete(struct lego_usb_tower *dev);
static int tower_open(struct inode *inode, struct file *file);
static int tower_release(struct inode *inode, struct file *file);
static __poll_t tower_poll(struct file *file, poll_table *wait);
static loff_t tower_llseek(struct file *file, loff_t off, int whence);

static void tower_check_for_read_packet(struct lego_usb_tower *dev);
static void tower_interrupt_in_callback(struct urb *urb);
static void tower_interrupt_out_callback(struct urb *urb);

static int  tower_probe(struct usb_interface *interface, const struct usb_device_id *id);
static void tower_disconnect(struct usb_interface *interface);


 
static const struct file_operations tower_fops = {
	.owner =	THIS_MODULE,
	.read  =	tower_read,
	.write =	tower_write,
	.open =		tower_open,
	.release =	tower_release,
	.poll =		tower_poll,
	.llseek =	tower_llseek,
};

static char *legousbtower_devnode(const struct device *dev, umode_t *mode)
{
	return kasprintf(GFP_KERNEL, "usb/%s", dev_name(dev));
}

 
static struct usb_class_driver tower_class = {
	.name =		"legousbtower%d",
	.devnode = 	legousbtower_devnode,
	.fops =		&tower_fops,
	.minor_base =	LEGO_USB_TOWER_MINOR_BASE,
};


 
static struct usb_driver tower_driver = {
	.name =		"legousbtower",
	.probe =	tower_probe,
	.disconnect =	tower_disconnect,
	.id_table =	tower_table,
};


 
static inline void lego_usb_tower_debug_data(struct device *dev,
					     const char *function, int size,
					     const unsigned char *data)
{
	dev_dbg(dev, "%s - length = %d, data = %*ph\n",
		function, size, size, data);
}


 
static inline void tower_delete(struct lego_usb_tower *dev)
{
	 
	usb_free_urb(dev->interrupt_in_urb);
	usb_free_urb(dev->interrupt_out_urb);
	kfree(dev->read_buffer);
	kfree(dev->interrupt_in_buffer);
	kfree(dev->interrupt_out_buffer);
	usb_put_dev(dev->udev);
	kfree(dev);
}


 
static int tower_open(struct inode *inode, struct file *file)
{
	struct lego_usb_tower *dev = NULL;
	int subminor;
	int retval = 0;
	struct usb_interface *interface;
	struct tower_reset_reply reset_reply;
	int result;

	nonseekable_open(inode, file);
	subminor = iminor(inode);

	interface = usb_find_interface(&tower_driver, subminor);
	if (!interface) {
		pr_err("error, can't find device for minor %d\n", subminor);
		retval = -ENODEV;
		goto exit;
	}

	dev = usb_get_intfdata(interface);
	if (!dev) {
		retval = -ENODEV;
		goto exit;
	}

	 
	if (mutex_lock_interruptible(&dev->lock)) {
	        retval = -ERESTARTSYS;
		goto exit;
	}


	 
	if (dev->open_count) {
		retval = -EBUSY;
		goto unlock_exit;
	}

	 
	result = usb_control_msg_recv(dev->udev, 0,
				      LEGO_USB_TOWER_REQUEST_RESET,
				      USB_TYPE_VENDOR | USB_DIR_IN | USB_RECIP_DEVICE,
				      0, 0,
				      &reset_reply, sizeof(reset_reply), 1000,
				      GFP_KERNEL);
	if (result < 0) {
		dev_err(&dev->udev->dev,
			"LEGO USB Tower reset control request failed\n");
		retval = result;
		goto unlock_exit;
	}

	 
	dev->read_buffer_length = 0;
	dev->read_packet_length = 0;
	usb_fill_int_urb(dev->interrupt_in_urb,
			 dev->udev,
			 usb_rcvintpipe(dev->udev, dev->interrupt_in_endpoint->bEndpointAddress),
			 dev->interrupt_in_buffer,
			 usb_endpoint_maxp(dev->interrupt_in_endpoint),
			 tower_interrupt_in_callback,
			 dev,
			 dev->interrupt_in_interval);

	dev->interrupt_in_done = 0;
	mb();

	retval = usb_submit_urb(dev->interrupt_in_urb, GFP_KERNEL);
	if (retval) {
		dev_err(&dev->udev->dev,
			"Couldn't submit interrupt_in_urb %d\n", retval);
		goto unlock_exit;
	}

	 
	file->private_data = dev;

	dev->open_count = 1;

unlock_exit:
	mutex_unlock(&dev->lock);

exit:
	return retval;
}

 
static int tower_release(struct inode *inode, struct file *file)
{
	struct lego_usb_tower *dev;
	int retval = 0;

	dev = file->private_data;
	if (dev == NULL) {
		retval = -ENODEV;
		goto exit;
	}

	mutex_lock(&dev->lock);

	if (dev->disconnected) {
		 

		 
		mutex_unlock(&dev->lock);
		tower_delete(dev);
		goto exit;
	}

	 
	if (dev->interrupt_out_busy) {
		wait_event_interruptible_timeout(dev->write_wait, !dev->interrupt_out_busy,
						 2 * HZ);
	}

	 
	usb_kill_urb(dev->interrupt_in_urb);
	usb_kill_urb(dev->interrupt_out_urb);

	dev->open_count = 0;

	mutex_unlock(&dev->lock);
exit:
	return retval;
}

 
static void tower_check_for_read_packet(struct lego_usb_tower *dev)
{
	spin_lock_irq(&dev->read_buffer_lock);
	if (!packet_timeout
	    || time_after(jiffies, dev->read_last_arrival + dev->packet_timeout_jiffies)
	    || dev->read_buffer_length == read_buffer_size) {
		dev->read_packet_length = dev->read_buffer_length;
	}
	dev->interrupt_in_done = 0;
	spin_unlock_irq(&dev->read_buffer_lock);
}


 
static __poll_t tower_poll(struct file *file, poll_table *wait)
{
	struct lego_usb_tower *dev;
	__poll_t mask = 0;

	dev = file->private_data;

	if (dev->disconnected)
		return EPOLLERR | EPOLLHUP;

	poll_wait(file, &dev->read_wait, wait);
	poll_wait(file, &dev->write_wait, wait);

	tower_check_for_read_packet(dev);
	if (dev->read_packet_length > 0)
		mask |= EPOLLIN | EPOLLRDNORM;
	if (!dev->interrupt_out_busy)
		mask |= EPOLLOUT | EPOLLWRNORM;

	return mask;
}


 
static loff_t tower_llseek(struct file *file, loff_t off, int whence)
{
	return -ESPIPE;		 
}


 
static ssize_t tower_read(struct file *file, char __user *buffer, size_t count, loff_t *ppos)
{
	struct lego_usb_tower *dev;
	size_t bytes_to_read;
	int i;
	int retval = 0;
	unsigned long timeout = 0;

	dev = file->private_data;

	 
	if (mutex_lock_interruptible(&dev->lock)) {
		retval = -ERESTARTSYS;
		goto exit;
	}

	 
	if (dev->disconnected) {
		retval = -ENODEV;
		goto unlock_exit;
	}

	 
	if (count == 0) {
		dev_dbg(&dev->udev->dev, "read request of 0 bytes\n");
		goto unlock_exit;
	}

	if (read_timeout)
		timeout = jiffies + msecs_to_jiffies(read_timeout);

	 
	tower_check_for_read_packet(dev);
	while (dev->read_packet_length == 0) {
		if (file->f_flags & O_NONBLOCK) {
			retval = -EAGAIN;
			goto unlock_exit;
		}
		retval = wait_event_interruptible_timeout(dev->read_wait, dev->interrupt_in_done, dev->packet_timeout_jiffies);
		if (retval < 0)
			goto unlock_exit;

		 
		if (read_timeout
		    && (dev->read_buffer_length || dev->interrupt_out_busy)) {
			timeout = jiffies + msecs_to_jiffies(read_timeout);
		}
		 
		if (read_timeout && time_after(jiffies, timeout)) {
			retval = -ETIMEDOUT;
			goto unlock_exit;
		}
		tower_check_for_read_packet(dev);
	}

	 
	bytes_to_read = min(count, dev->read_packet_length);

	if (copy_to_user(buffer, dev->read_buffer, bytes_to_read)) {
		retval = -EFAULT;
		goto unlock_exit;
	}

	spin_lock_irq(&dev->read_buffer_lock);
	dev->read_buffer_length -= bytes_to_read;
	dev->read_packet_length -= bytes_to_read;
	for (i = 0; i < dev->read_buffer_length; i++)
		dev->read_buffer[i] = dev->read_buffer[i+bytes_to_read];
	spin_unlock_irq(&dev->read_buffer_lock);

	retval = bytes_to_read;

unlock_exit:
	 
	mutex_unlock(&dev->lock);

exit:
	return retval;
}


 
static ssize_t tower_write(struct file *file, const char __user *buffer, size_t count, loff_t *ppos)
{
	struct lego_usb_tower *dev;
	size_t bytes_to_write;
	int retval = 0;

	dev = file->private_data;

	 
	if (mutex_lock_interruptible(&dev->lock)) {
		retval = -ERESTARTSYS;
		goto exit;
	}

	 
	if (dev->disconnected) {
		retval = -ENODEV;
		goto unlock_exit;
	}

	 
	if (count == 0) {
		dev_dbg(&dev->udev->dev, "write request of 0 bytes\n");
		goto unlock_exit;
	}

	 
	while (dev->interrupt_out_busy) {
		if (file->f_flags & O_NONBLOCK) {
			retval = -EAGAIN;
			goto unlock_exit;
		}
		retval = wait_event_interruptible(dev->write_wait,
						  !dev->interrupt_out_busy);
		if (retval)
			goto unlock_exit;
	}

	 
	bytes_to_write = min_t(int, count, write_buffer_size);
	dev_dbg(&dev->udev->dev, "%s: count = %zd, bytes_to_write = %zd\n",
		__func__, count, bytes_to_write);

	if (copy_from_user(dev->interrupt_out_buffer, buffer, bytes_to_write)) {
		retval = -EFAULT;
		goto unlock_exit;
	}

	 
	usb_fill_int_urb(dev->interrupt_out_urb,
			 dev->udev,
			 usb_sndintpipe(dev->udev, dev->interrupt_out_endpoint->bEndpointAddress),
			 dev->interrupt_out_buffer,
			 bytes_to_write,
			 tower_interrupt_out_callback,
			 dev,
			 dev->interrupt_out_interval);

	dev->interrupt_out_busy = 1;
	wmb();

	retval = usb_submit_urb(dev->interrupt_out_urb, GFP_KERNEL);
	if (retval) {
		dev->interrupt_out_busy = 0;
		dev_err(&dev->udev->dev,
			"Couldn't submit interrupt_out_urb %d\n", retval);
		goto unlock_exit;
	}
	retval = bytes_to_write;

unlock_exit:
	 
	mutex_unlock(&dev->lock);

exit:
	return retval;
}


 
static void tower_interrupt_in_callback(struct urb *urb)
{
	struct lego_usb_tower *dev = urb->context;
	int status = urb->status;
	int retval;
	unsigned long flags;

	lego_usb_tower_debug_data(&dev->udev->dev, __func__,
				  urb->actual_length, urb->transfer_buffer);

	if (status) {
		if (status == -ENOENT ||
		    status == -ECONNRESET ||
		    status == -ESHUTDOWN) {
			goto exit;
		} else {
			dev_dbg(&dev->udev->dev,
				"%s: nonzero status received: %d\n", __func__,
				status);
			goto resubmit;  
		}
	}

	if (urb->actual_length > 0) {
		spin_lock_irqsave(&dev->read_buffer_lock, flags);
		if (dev->read_buffer_length + urb->actual_length < read_buffer_size) {
			memcpy(dev->read_buffer + dev->read_buffer_length,
			       dev->interrupt_in_buffer,
			       urb->actual_length);
			dev->read_buffer_length += urb->actual_length;
			dev->read_last_arrival = jiffies;
			dev_dbg(&dev->udev->dev, "%s: received %d bytes\n",
				__func__, urb->actual_length);
		} else {
			pr_warn("read_buffer overflow, %d bytes dropped\n",
				urb->actual_length);
		}
		spin_unlock_irqrestore(&dev->read_buffer_lock, flags);
	}

resubmit:
	retval = usb_submit_urb(dev->interrupt_in_urb, GFP_ATOMIC);
	if (retval) {
		dev_err(&dev->udev->dev, "%s: usb_submit_urb failed (%d)\n",
			__func__, retval);
	}
exit:
	dev->interrupt_in_done = 1;
	wake_up_interruptible(&dev->read_wait);
}


 
static void tower_interrupt_out_callback(struct urb *urb)
{
	struct lego_usb_tower *dev = urb->context;
	int status = urb->status;

	lego_usb_tower_debug_data(&dev->udev->dev, __func__,
				  urb->actual_length, urb->transfer_buffer);

	 
	if (status && !(status == -ENOENT ||
			status == -ECONNRESET ||
			status == -ESHUTDOWN)) {
		dev_dbg(&dev->udev->dev,
			"%s: nonzero write bulk status received: %d\n", __func__,
			status);
	}

	dev->interrupt_out_busy = 0;
	wake_up_interruptible(&dev->write_wait);
}


 
static int tower_probe(struct usb_interface *interface, const struct usb_device_id *id)
{
	struct device *idev = &interface->dev;
	struct usb_device *udev = interface_to_usbdev(interface);
	struct lego_usb_tower *dev;
	struct tower_get_version_reply get_version_reply;
	int retval = -ENOMEM;
	int result;

	 
	dev = kzalloc(sizeof(*dev), GFP_KERNEL);
	if (!dev)
		goto exit;

	mutex_init(&dev->lock);
	dev->udev = usb_get_dev(udev);
	spin_lock_init(&dev->read_buffer_lock);
	dev->packet_timeout_jiffies = msecs_to_jiffies(packet_timeout);
	dev->read_last_arrival = jiffies;
	init_waitqueue_head(&dev->read_wait);
	init_waitqueue_head(&dev->write_wait);

	result = usb_find_common_endpoints_reverse(interface->cur_altsetting,
			NULL, NULL,
			&dev->interrupt_in_endpoint,
			&dev->interrupt_out_endpoint);
	if (result) {
		dev_err(idev, "interrupt endpoints not found\n");
		retval = result;
		goto error;
	}

	dev->read_buffer = kmalloc(read_buffer_size, GFP_KERNEL);
	if (!dev->read_buffer)
		goto error;
	dev->interrupt_in_buffer = kmalloc(usb_endpoint_maxp(dev->interrupt_in_endpoint), GFP_KERNEL);
	if (!dev->interrupt_in_buffer)
		goto error;
	dev->interrupt_in_urb = usb_alloc_urb(0, GFP_KERNEL);
	if (!dev->interrupt_in_urb)
		goto error;
	dev->interrupt_out_buffer = kmalloc(write_buffer_size, GFP_KERNEL);
	if (!dev->interrupt_out_buffer)
		goto error;
	dev->interrupt_out_urb = usb_alloc_urb(0, GFP_KERNEL);
	if (!dev->interrupt_out_urb)
		goto error;
	dev->interrupt_in_interval = interrupt_in_interval ? interrupt_in_interval : dev->interrupt_in_endpoint->bInterval;
	dev->interrupt_out_interval = interrupt_out_interval ? interrupt_out_interval : dev->interrupt_out_endpoint->bInterval;

	 
	result = usb_control_msg_recv(udev, 0,
				      LEGO_USB_TOWER_REQUEST_GET_VERSION,
				      USB_TYPE_VENDOR | USB_DIR_IN | USB_RECIP_DEVICE,
				      0,
				      0,
				      &get_version_reply,
				      sizeof(get_version_reply),
				      1000, GFP_KERNEL);
	if (result) {
		dev_err(idev, "get version request failed: %d\n", result);
		retval = result;
		goto error;
	}
	dev_info(&interface->dev,
		 "LEGO USB Tower firmware version is %d.%d build %d\n",
		 get_version_reply.major,
		 get_version_reply.minor,
		 le16_to_cpu(get_version_reply.build_no));

	 
	usb_set_intfdata(interface, dev);

	retval = usb_register_dev(interface, &tower_class);
	if (retval) {
		 
		dev_err(idev, "Not able to get a minor for this device.\n");
		goto error;
	}
	dev->minor = interface->minor;

	 
	dev_info(&interface->dev, "LEGO USB Tower #%d now attached to major "
		 "%d minor %d\n", (dev->minor - LEGO_USB_TOWER_MINOR_BASE),
		 USB_MAJOR, dev->minor);

exit:
	return retval;

error:
	tower_delete(dev);
	return retval;
}


 
static void tower_disconnect(struct usb_interface *interface)
{
	struct lego_usb_tower *dev;
	int minor;

	dev = usb_get_intfdata(interface);

	minor = dev->minor;

	 
	usb_deregister_dev(interface, &tower_class);

	 
	usb_poison_urb(dev->interrupt_in_urb);
	usb_poison_urb(dev->interrupt_out_urb);

	mutex_lock(&dev->lock);

	 
	if (!dev->open_count) {
		mutex_unlock(&dev->lock);
		tower_delete(dev);
	} else {
		dev->disconnected = 1;
		 
		wake_up_interruptible_all(&dev->read_wait);
		wake_up_interruptible_all(&dev->write_wait);
		mutex_unlock(&dev->lock);
	}

	dev_info(&interface->dev, "LEGO USB Tower #%d now disconnected\n",
		 (minor - LEGO_USB_TOWER_MINOR_BASE));
}

module_usb_driver(tower_driver);

MODULE_AUTHOR(DRIVER_AUTHOR);
MODULE_DESCRIPTION(DRIVER_DESC);
MODULE_LICENSE("GPL");
