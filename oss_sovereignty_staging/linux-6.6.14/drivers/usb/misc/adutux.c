
 

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/kernel.h>
#include <linux/sched/signal.h>
#include <linux/errno.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/usb.h>
#include <linux/mutex.h>
#include <linux/uaccess.h>

#define DRIVER_AUTHOR "John Homppi"
#define DRIVER_DESC "adutux (see www.ontrak.net)"

 
#define ADU_VENDOR_ID 0x0a07
#define ADU_PRODUCT_ID 0x0064

 
static const struct usb_device_id device_table[] = {
	{ USB_DEVICE(ADU_VENDOR_ID, ADU_PRODUCT_ID) },		 
	{ USB_DEVICE(ADU_VENDOR_ID, ADU_PRODUCT_ID+20) },	 
	{ USB_DEVICE(ADU_VENDOR_ID, ADU_PRODUCT_ID+30) },	 
	{ USB_DEVICE(ADU_VENDOR_ID, ADU_PRODUCT_ID+100) },	 
	{ USB_DEVICE(ADU_VENDOR_ID, ADU_PRODUCT_ID+108) },	 
	{ USB_DEVICE(ADU_VENDOR_ID, ADU_PRODUCT_ID+118) },	 
	{ }  
};

MODULE_DEVICE_TABLE(usb, device_table);

#ifdef CONFIG_USB_DYNAMIC_MINORS
#define ADU_MINOR_BASE	0
#else
#define ADU_MINOR_BASE	67
#endif

 
#define MAX_DEVICES	16

#define COMMAND_TIMEOUT	(2*HZ)

 

 
struct adu_device {
	struct mutex		mtx;
	struct usb_device *udev;  
	struct usb_interface *interface;
	unsigned int		minor;  
	char			serial_number[8];

	int			open_count;  
	unsigned long		disconnected:1;

	char		*read_buffer_primary;
	int			read_buffer_length;
	char		*read_buffer_secondary;
	int			secondary_head;
	int			secondary_tail;
	spinlock_t		buflock;

	wait_queue_head_t	read_wait;
	wait_queue_head_t	write_wait;

	char		*interrupt_in_buffer;
	struct usb_endpoint_descriptor *interrupt_in_endpoint;
	struct urb	*interrupt_in_urb;
	int			read_urb_finished;

	char		*interrupt_out_buffer;
	struct usb_endpoint_descriptor *interrupt_out_endpoint;
	struct urb	*interrupt_out_urb;
	int			out_urb_finished;
};

static DEFINE_MUTEX(adutux_mutex);

static struct usb_driver adu_driver;

static inline void adu_debug_data(struct device *dev, const char *function,
				  int size, const unsigned char *data)
{
	dev_dbg(dev, "%s - length = %d, data = %*ph\n",
		function, size, size, data);
}

 
static void adu_abort_transfers(struct adu_device *dev)
{
	unsigned long flags;

	if (dev->disconnected)
		return;

	 

	 
	spin_lock_irqsave(&dev->buflock, flags);
	if (!dev->read_urb_finished) {
		spin_unlock_irqrestore(&dev->buflock, flags);
		usb_kill_urb(dev->interrupt_in_urb);
	} else
		spin_unlock_irqrestore(&dev->buflock, flags);

	spin_lock_irqsave(&dev->buflock, flags);
	if (!dev->out_urb_finished) {
		spin_unlock_irqrestore(&dev->buflock, flags);
		wait_event_timeout(dev->write_wait, dev->out_urb_finished,
			COMMAND_TIMEOUT);
		usb_kill_urb(dev->interrupt_out_urb);
	} else
		spin_unlock_irqrestore(&dev->buflock, flags);
}

static void adu_delete(struct adu_device *dev)
{
	 
	usb_free_urb(dev->interrupt_in_urb);
	usb_free_urb(dev->interrupt_out_urb);
	kfree(dev->read_buffer_primary);
	kfree(dev->read_buffer_secondary);
	kfree(dev->interrupt_in_buffer);
	kfree(dev->interrupt_out_buffer);
	usb_put_dev(dev->udev);
	kfree(dev);
}

static void adu_interrupt_in_callback(struct urb *urb)
{
	struct adu_device *dev = urb->context;
	int status = urb->status;
	unsigned long flags;

	adu_debug_data(&dev->udev->dev, __func__,
		       urb->actual_length, urb->transfer_buffer);

	spin_lock_irqsave(&dev->buflock, flags);

	if (status != 0) {
		if ((status != -ENOENT) && (status != -ECONNRESET) &&
			(status != -ESHUTDOWN)) {
			dev_dbg(&dev->udev->dev,
				"%s : nonzero status received: %d\n",
				__func__, status);
		}
		goto exit;
	}

	if (urb->actual_length > 0 && dev->interrupt_in_buffer[0] != 0x00) {
		if (dev->read_buffer_length <
		    (4 * usb_endpoint_maxp(dev->interrupt_in_endpoint)) -
		     (urb->actual_length)) {
			memcpy (dev->read_buffer_primary +
				dev->read_buffer_length,
				dev->interrupt_in_buffer, urb->actual_length);

			dev->read_buffer_length += urb->actual_length;
			dev_dbg(&dev->udev->dev, "%s reading  %d\n", __func__,
				urb->actual_length);
		} else {
			dev_dbg(&dev->udev->dev, "%s : read_buffer overflow\n",
				__func__);
		}
	}

exit:
	dev->read_urb_finished = 1;
	spin_unlock_irqrestore(&dev->buflock, flags);
	 
	wake_up_interruptible(&dev->read_wait);
}

static void adu_interrupt_out_callback(struct urb *urb)
{
	struct adu_device *dev = urb->context;
	int status = urb->status;
	unsigned long flags;

	adu_debug_data(&dev->udev->dev, __func__,
		       urb->actual_length, urb->transfer_buffer);

	if (status != 0) {
		if ((status != -ENOENT) &&
		    (status != -ESHUTDOWN) &&
		    (status != -ECONNRESET)) {
			dev_dbg(&dev->udev->dev,
				"%s :nonzero status received: %d\n", __func__,
				status);
		}
		return;
	}

	spin_lock_irqsave(&dev->buflock, flags);
	dev->out_urb_finished = 1;
	wake_up(&dev->write_wait);
	spin_unlock_irqrestore(&dev->buflock, flags);
}

static int adu_open(struct inode *inode, struct file *file)
{
	struct adu_device *dev = NULL;
	struct usb_interface *interface;
	int subminor;
	int retval;

	subminor = iminor(inode);

	retval = mutex_lock_interruptible(&adutux_mutex);
	if (retval)
		goto exit_no_lock;

	interface = usb_find_interface(&adu_driver, subminor);
	if (!interface) {
		pr_err("%s - error, can't find device for minor %d\n",
		       __func__, subminor);
		retval = -ENODEV;
		goto exit_no_device;
	}

	dev = usb_get_intfdata(interface);
	if (!dev) {
		retval = -ENODEV;
		goto exit_no_device;
	}

	 
	if (dev->open_count) {
		retval = -EBUSY;
		goto exit_no_device;
	}

	++dev->open_count;
	dev_dbg(&dev->udev->dev, "%s: open count %d\n", __func__,
		dev->open_count);

	 
	file->private_data = dev;

	 
	dev->read_buffer_length = 0;

	 
	usb_fill_int_urb(dev->interrupt_in_urb, dev->udev,
			 usb_rcvintpipe(dev->udev,
					dev->interrupt_in_endpoint->bEndpointAddress),
			 dev->interrupt_in_buffer,
			 usb_endpoint_maxp(dev->interrupt_in_endpoint),
			 adu_interrupt_in_callback, dev,
			 dev->interrupt_in_endpoint->bInterval);
	dev->read_urb_finished = 0;
	if (usb_submit_urb(dev->interrupt_in_urb, GFP_KERNEL))
		dev->read_urb_finished = 1;
	 
	 

	 
	dev->out_urb_finished = 1;

	retval = 0;

exit_no_device:
	mutex_unlock(&adutux_mutex);
exit_no_lock:
	return retval;
}

static void adu_release_internal(struct adu_device *dev)
{
	 
	--dev->open_count;
	dev_dbg(&dev->udev->dev, "%s : open count %d\n", __func__,
		dev->open_count);
	if (dev->open_count <= 0) {
		adu_abort_transfers(dev);
		dev->open_count = 0;
	}
}

static int adu_release(struct inode *inode, struct file *file)
{
	struct adu_device *dev;
	int retval = 0;

	if (file == NULL) {
		retval = -ENODEV;
		goto exit;
	}

	dev = file->private_data;
	if (dev == NULL) {
		retval = -ENODEV;
		goto exit;
	}

	mutex_lock(&adutux_mutex);  

	if (dev->open_count <= 0) {
		dev_dbg(&dev->udev->dev, "%s : device not opened\n", __func__);
		retval = -ENODEV;
		goto unlock;
	}

	adu_release_internal(dev);
	if (dev->disconnected) {
		 
		if (!dev->open_count)	 
			adu_delete(dev);
	}
unlock:
	mutex_unlock(&adutux_mutex);
exit:
	return retval;
}

static ssize_t adu_read(struct file *file, __user char *buffer, size_t count,
			loff_t *ppos)
{
	struct adu_device *dev;
	size_t bytes_read = 0;
	size_t bytes_to_read = count;
	int retval = 0;
	int timeout = 0;
	int should_submit = 0;
	unsigned long flags;
	DECLARE_WAITQUEUE(wait, current);

	dev = file->private_data;
	if (mutex_lock_interruptible(&dev->mtx))
		return -ERESTARTSYS;

	 
	if (dev->disconnected) {
		retval = -ENODEV;
		pr_err("No device or device unplugged %d\n", retval);
		goto exit;
	}

	 
	if (count == 0) {
		dev_dbg(&dev->udev->dev, "%s : read request of 0 bytes\n",
			__func__);
		goto exit;
	}

	timeout = COMMAND_TIMEOUT;
	dev_dbg(&dev->udev->dev, "%s : about to start looping\n", __func__);
	while (bytes_to_read) {
		size_t data_in_secondary = dev->secondary_tail - dev->secondary_head;
		dev_dbg(&dev->udev->dev,
			"%s : while, data_in_secondary=%zu, status=%d\n",
			__func__, data_in_secondary,
			dev->interrupt_in_urb->status);

		if (data_in_secondary) {
			 
			size_t amount = min(bytes_to_read, data_in_secondary);
			if (copy_to_user(buffer, dev->read_buffer_secondary+dev->secondary_head, amount)) {
				retval = -EFAULT;
				goto exit;
			}
			dev->secondary_head += amount;
			bytes_read += amount;
			bytes_to_read -= amount;
		} else {
			 
			spin_lock_irqsave (&dev->buflock, flags);
			if (dev->read_buffer_length) {
				 
				dev_dbg(&dev->udev->dev,
					"%s : swap, read_buffer_length = %d\n",
					__func__, dev->read_buffer_length);
				swap(dev->read_buffer_primary, dev->read_buffer_secondary);
				dev->secondary_head = 0;
				dev->secondary_tail = dev->read_buffer_length;
				dev->read_buffer_length = 0;
				spin_unlock_irqrestore(&dev->buflock, flags);
				 
				should_submit = 1;
			} else {
				 
				if (!dev->read_urb_finished) {
					 
					spin_unlock_irqrestore(&dev->buflock, flags);
					dev_dbg(&dev->udev->dev,
						"%s : submitted already\n",
						__func__);
				} else {
					 
					dev_dbg(&dev->udev->dev,
						"%s : initiate input\n",
						__func__);
					dev->read_urb_finished = 0;
					spin_unlock_irqrestore(&dev->buflock, flags);

					usb_fill_int_urb(dev->interrupt_in_urb, dev->udev,
							usb_rcvintpipe(dev->udev,
								dev->interrupt_in_endpoint->bEndpointAddress),
							 dev->interrupt_in_buffer,
							 usb_endpoint_maxp(dev->interrupt_in_endpoint),
							 adu_interrupt_in_callback,
							 dev,
							 dev->interrupt_in_endpoint->bInterval);
					retval = usb_submit_urb(dev->interrupt_in_urb, GFP_KERNEL);
					if (retval) {
						dev->read_urb_finished = 1;
						if (retval == -ENOMEM) {
							retval = bytes_read ? bytes_read : -ENOMEM;
						}
						dev_dbg(&dev->udev->dev,
							"%s : submit failed\n",
							__func__);
						goto exit;
					}
				}

				 
				set_current_state(TASK_INTERRUPTIBLE);
				add_wait_queue(&dev->read_wait, &wait);
				spin_lock_irqsave(&dev->buflock, flags);
				if (!dev->read_urb_finished) {
					spin_unlock_irqrestore(&dev->buflock, flags);
					timeout = schedule_timeout(COMMAND_TIMEOUT);
				} else {
					spin_unlock_irqrestore(&dev->buflock, flags);
					set_current_state(TASK_RUNNING);
				}
				remove_wait_queue(&dev->read_wait, &wait);

				if (timeout <= 0) {
					dev_dbg(&dev->udev->dev,
						"%s : timeout\n", __func__);
					retval = bytes_read ? bytes_read : -ETIMEDOUT;
					goto exit;
				}

				if (signal_pending(current)) {
					dev_dbg(&dev->udev->dev,
						"%s : signal pending\n",
						__func__);
					retval = bytes_read ? bytes_read : -EINTR;
					goto exit;
				}
			}
		}
	}

	retval = bytes_read;
	 
	spin_lock_irqsave(&dev->buflock, flags);
	if (should_submit && dev->read_urb_finished) {
		dev->read_urb_finished = 0;
		spin_unlock_irqrestore(&dev->buflock, flags);
		usb_fill_int_urb(dev->interrupt_in_urb, dev->udev,
				 usb_rcvintpipe(dev->udev,
					dev->interrupt_in_endpoint->bEndpointAddress),
				dev->interrupt_in_buffer,
				usb_endpoint_maxp(dev->interrupt_in_endpoint),
				adu_interrupt_in_callback,
				dev,
				dev->interrupt_in_endpoint->bInterval);
		if (usb_submit_urb(dev->interrupt_in_urb, GFP_KERNEL) != 0)
			dev->read_urb_finished = 1;
		 
	} else {
		spin_unlock_irqrestore(&dev->buflock, flags);
	}

exit:
	 
	mutex_unlock(&dev->mtx);

	return retval;
}

static ssize_t adu_write(struct file *file, const __user char *buffer,
			 size_t count, loff_t *ppos)
{
	DECLARE_WAITQUEUE(waita, current);
	struct adu_device *dev;
	size_t bytes_written = 0;
	size_t bytes_to_write;
	size_t buffer_size;
	unsigned long flags;
	int retval;

	dev = file->private_data;

	retval = mutex_lock_interruptible(&dev->mtx);
	if (retval)
		goto exit_nolock;

	 
	if (dev->disconnected) {
		retval = -ENODEV;
		pr_err("No device or device unplugged %d\n", retval);
		goto exit;
	}

	 
	if (count == 0) {
		dev_dbg(&dev->udev->dev, "%s : write request of 0 bytes\n",
			__func__);
		goto exit;
	}

	while (count > 0) {
		add_wait_queue(&dev->write_wait, &waita);
		set_current_state(TASK_INTERRUPTIBLE);
		spin_lock_irqsave(&dev->buflock, flags);
		if (!dev->out_urb_finished) {
			spin_unlock_irqrestore(&dev->buflock, flags);

			mutex_unlock(&dev->mtx);
			if (signal_pending(current)) {
				dev_dbg(&dev->udev->dev, "%s : interrupted\n",
					__func__);
				set_current_state(TASK_RUNNING);
				retval = -EINTR;
				goto exit_onqueue;
			}
			if (schedule_timeout(COMMAND_TIMEOUT) == 0) {
				dev_dbg(&dev->udev->dev,
					"%s - command timed out.\n", __func__);
				retval = -ETIMEDOUT;
				goto exit_onqueue;
			}
			remove_wait_queue(&dev->write_wait, &waita);
			retval = mutex_lock_interruptible(&dev->mtx);
			if (retval) {
				retval = bytes_written ? bytes_written : retval;
				goto exit_nolock;
			}

			dev_dbg(&dev->udev->dev,
				"%s : in progress, count = %zd\n",
				__func__, count);
		} else {
			spin_unlock_irqrestore(&dev->buflock, flags);
			set_current_state(TASK_RUNNING);
			remove_wait_queue(&dev->write_wait, &waita);
			dev_dbg(&dev->udev->dev, "%s : sending, count = %zd\n",
				__func__, count);

			 
			buffer_size = usb_endpoint_maxp(dev->interrupt_out_endpoint);
			bytes_to_write = count > buffer_size ? buffer_size : count;
			dev_dbg(&dev->udev->dev,
				"%s : buffer_size = %zd, count = %zd, bytes_to_write = %zd\n",
				__func__, buffer_size, count, bytes_to_write);

			if (copy_from_user(dev->interrupt_out_buffer, buffer, bytes_to_write) != 0) {
				retval = -EFAULT;
				goto exit;
			}

			 
			usb_fill_int_urb(
				dev->interrupt_out_urb,
				dev->udev,
				usb_sndintpipe(dev->udev, dev->interrupt_out_endpoint->bEndpointAddress),
				dev->interrupt_out_buffer,
				bytes_to_write,
				adu_interrupt_out_callback,
				dev,
				dev->interrupt_out_endpoint->bInterval);
			dev->interrupt_out_urb->actual_length = bytes_to_write;
			dev->out_urb_finished = 0;
			retval = usb_submit_urb(dev->interrupt_out_urb, GFP_KERNEL);
			if (retval < 0) {
				dev->out_urb_finished = 1;
				dev_err(&dev->udev->dev, "Couldn't submit "
					"interrupt_out_urb %d\n", retval);
				goto exit;
			}

			buffer += bytes_to_write;
			count -= bytes_to_write;

			bytes_written += bytes_to_write;
		}
	}
	mutex_unlock(&dev->mtx);
	return bytes_written;

exit:
	mutex_unlock(&dev->mtx);
exit_nolock:
	return retval;

exit_onqueue:
	remove_wait_queue(&dev->write_wait, &waita);
	return retval;
}

 
static const struct file_operations adu_fops = {
	.owner = THIS_MODULE,
	.read  = adu_read,
	.write = adu_write,
	.open = adu_open,
	.release = adu_release,
	.llseek = noop_llseek,
};

 
static struct usb_class_driver adu_class = {
	.name = "usb/adutux%d",
	.fops = &adu_fops,
	.minor_base = ADU_MINOR_BASE,
};

 
static int adu_probe(struct usb_interface *interface,
		     const struct usb_device_id *id)
{
	struct usb_device *udev = interface_to_usbdev(interface);
	struct adu_device *dev = NULL;
	int retval = -ENOMEM;
	int in_end_size;
	int out_end_size;
	int res;

	 
	dev = kzalloc(sizeof(struct adu_device), GFP_KERNEL);
	if (!dev)
		return -ENOMEM;

	mutex_init(&dev->mtx);
	spin_lock_init(&dev->buflock);
	dev->udev = usb_get_dev(udev);
	init_waitqueue_head(&dev->read_wait);
	init_waitqueue_head(&dev->write_wait);

	res = usb_find_common_endpoints_reverse(interface->cur_altsetting,
			NULL, NULL,
			&dev->interrupt_in_endpoint,
			&dev->interrupt_out_endpoint);
	if (res) {
		dev_err(&interface->dev, "interrupt endpoints not found\n");
		retval = res;
		goto error;
	}

	in_end_size = usb_endpoint_maxp(dev->interrupt_in_endpoint);
	out_end_size = usb_endpoint_maxp(dev->interrupt_out_endpoint);

	dev->read_buffer_primary = kmalloc((4 * in_end_size), GFP_KERNEL);
	if (!dev->read_buffer_primary)
		goto error;

	 
	memset(dev->read_buffer_primary, 'a', in_end_size);
	memset(dev->read_buffer_primary + in_end_size, 'b', in_end_size);
	memset(dev->read_buffer_primary + (2 * in_end_size), 'c', in_end_size);
	memset(dev->read_buffer_primary + (3 * in_end_size), 'd', in_end_size);

	dev->read_buffer_secondary = kmalloc((4 * in_end_size), GFP_KERNEL);
	if (!dev->read_buffer_secondary)
		goto error;

	 
	memset(dev->read_buffer_secondary, 'e', in_end_size);
	memset(dev->read_buffer_secondary + in_end_size, 'f', in_end_size);
	memset(dev->read_buffer_secondary + (2 * in_end_size), 'g', in_end_size);
	memset(dev->read_buffer_secondary + (3 * in_end_size), 'h', in_end_size);

	dev->interrupt_in_buffer = kmalloc(in_end_size, GFP_KERNEL);
	if (!dev->interrupt_in_buffer)
		goto error;

	 
	memset(dev->interrupt_in_buffer, 'i', in_end_size);

	dev->interrupt_in_urb = usb_alloc_urb(0, GFP_KERNEL);
	if (!dev->interrupt_in_urb)
		goto error;
	dev->interrupt_out_buffer = kmalloc(out_end_size, GFP_KERNEL);
	if (!dev->interrupt_out_buffer)
		goto error;
	dev->interrupt_out_urb = usb_alloc_urb(0, GFP_KERNEL);
	if (!dev->interrupt_out_urb)
		goto error;

	if (!usb_string(udev, udev->descriptor.iSerialNumber, dev->serial_number,
			sizeof(dev->serial_number))) {
		dev_err(&interface->dev, "Could not retrieve serial number\n");
		retval = -EIO;
		goto error;
	}
	dev_dbg(&interface->dev, "serial_number=%s", dev->serial_number);

	 
	usb_set_intfdata(interface, dev);

	retval = usb_register_dev(interface, &adu_class);

	if (retval) {
		 
		dev_err(&interface->dev, "Not able to get a minor for this device.\n");
		usb_set_intfdata(interface, NULL);
		goto error;
	}

	dev->minor = interface->minor;

	 
	dev_info(&interface->dev, "ADU%d %s now attached to /dev/usb/adutux%d\n",
		 le16_to_cpu(udev->descriptor.idProduct), dev->serial_number,
		 (dev->minor - ADU_MINOR_BASE));

	return 0;

error:
	adu_delete(dev);
	return retval;
}

 
static void adu_disconnect(struct usb_interface *interface)
{
	struct adu_device *dev;

	dev = usb_get_intfdata(interface);

	usb_deregister_dev(interface, &adu_class);

	usb_poison_urb(dev->interrupt_in_urb);
	usb_poison_urb(dev->interrupt_out_urb);

	mutex_lock(&adutux_mutex);
	usb_set_intfdata(interface, NULL);

	mutex_lock(&dev->mtx);	 
	dev->disconnected = 1;
	mutex_unlock(&dev->mtx);

	 
	if (!dev->open_count)
		adu_delete(dev);

	mutex_unlock(&adutux_mutex);
}

 
static struct usb_driver adu_driver = {
	.name = "adutux",
	.probe = adu_probe,
	.disconnect = adu_disconnect,
	.id_table = device_table,
};

module_usb_driver(adu_driver);

MODULE_AUTHOR(DRIVER_AUTHOR);
MODULE_DESCRIPTION(DRIVER_DESC);
MODULE_LICENSE("GPL");
