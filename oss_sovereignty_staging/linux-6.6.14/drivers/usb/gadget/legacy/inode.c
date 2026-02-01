
 


 

#include <linux/init.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/fs_context.h>
#include <linux/pagemap.h>
#include <linux/uts.h>
#include <linux/wait.h>
#include <linux/compiler.h>
#include <linux/uaccess.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/poll.h>
#include <linux/kthread.h>
#include <linux/aio.h>
#include <linux/uio.h>
#include <linux/refcount.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/moduleparam.h>

#include <linux/usb/gadgetfs.h>
#include <linux/usb/gadget.h>


 

#define	DRIVER_DESC	"USB Gadget filesystem"
#define	DRIVER_VERSION	"24 Aug 2004"

static const char driver_desc [] = DRIVER_DESC;
static const char shortname [] = "gadgetfs";

MODULE_DESCRIPTION (DRIVER_DESC);
MODULE_AUTHOR ("David Brownell");
MODULE_LICENSE ("GPL");

static int ep_open(struct inode *, struct file *);


 

#define GADGETFS_MAGIC		0xaee71ee7

 
enum ep0_state {
	 
	STATE_DEV_DISABLED = 0,

	 
	STATE_DEV_OPENED,

	 
	STATE_DEV_UNCONNECTED,
	STATE_DEV_CONNECTED,
	STATE_DEV_SETUP,

	 
	STATE_DEV_UNBOUND,
};

 
#define	N_EVENT			5

#define RBUF_SIZE		256

struct dev_data {
	spinlock_t			lock;
	refcount_t			count;
	int				udc_usage;
	enum ep0_state			state;		 
	struct usb_gadgetfs_event	event [N_EVENT];
	unsigned			ev_next;
	struct fasync_struct		*fasync;
	u8				current_config;

	 
	unsigned			usermode_setup : 1,
					setup_in : 1,
					setup_can_stall : 1,
					setup_out_ready : 1,
					setup_out_error : 1,
					setup_abort : 1,
					gadget_registered : 1;
	unsigned			setup_wLength;

	 
	struct usb_config_descriptor	*config, *hs_config;
	struct usb_device_descriptor	*dev;
	struct usb_request		*req;
	struct usb_gadget		*gadget;
	struct list_head		epfiles;
	void				*buf;
	wait_queue_head_t		wait;
	struct super_block		*sb;
	struct dentry			*dentry;

	 
	u8				rbuf[RBUF_SIZE];
};

static inline void get_dev (struct dev_data *data)
{
	refcount_inc (&data->count);
}

static void put_dev (struct dev_data *data)
{
	if (likely (!refcount_dec_and_test (&data->count)))
		return;
	 
	BUG_ON (waitqueue_active (&data->wait));
	kfree (data);
}

static struct dev_data *dev_new (void)
{
	struct dev_data		*dev;

	dev = kzalloc(sizeof(*dev), GFP_KERNEL);
	if (!dev)
		return NULL;
	dev->state = STATE_DEV_DISABLED;
	refcount_set (&dev->count, 1);
	spin_lock_init (&dev->lock);
	INIT_LIST_HEAD (&dev->epfiles);
	init_waitqueue_head (&dev->wait);
	return dev;
}

 

 
enum ep_state {
	STATE_EP_DISABLED = 0,
	STATE_EP_READY,
	STATE_EP_ENABLED,
	STATE_EP_UNBOUND,
};

struct ep_data {
	struct mutex			lock;
	enum ep_state			state;
	refcount_t			count;
	struct dev_data			*dev;
	 
	struct usb_ep			*ep;
	struct usb_request		*req;
	ssize_t				status;
	char				name [16];
	struct usb_endpoint_descriptor	desc, hs_desc;
	struct list_head		epfiles;
	wait_queue_head_t		wait;
	struct dentry			*dentry;
};

static inline void get_ep (struct ep_data *data)
{
	refcount_inc (&data->count);
}

static void put_ep (struct ep_data *data)
{
	if (likely (!refcount_dec_and_test (&data->count)))
		return;
	put_dev (data->dev);
	 
	BUG_ON (!list_empty (&data->epfiles));
	BUG_ON (waitqueue_active (&data->wait));
	kfree (data);
}

 

 

static const char *CHIP;
static DEFINE_MUTEX(sb_mutex);		 

 

 

 
#define xprintk(d,level,fmt,args...) \
	printk(level "%s: " fmt , shortname , ## args)

#ifdef DEBUG
#define DBG(dev,fmt,args...) \
	xprintk(dev , KERN_DEBUG , fmt , ## args)
#else
#define DBG(dev,fmt,args...) \
	do { } while (0)
#endif  

#ifdef VERBOSE_DEBUG
#define VDEBUG	DBG
#else
#define VDEBUG(dev,fmt,args...) \
	do { } while (0)
#endif  

#define ERROR(dev,fmt,args...) \
	xprintk(dev , KERN_ERR , fmt , ## args)
#define INFO(dev,fmt,args...) \
	xprintk(dev , KERN_INFO , fmt , ## args)


 

 

static void epio_complete (struct usb_ep *ep, struct usb_request *req)
{
	struct ep_data	*epdata = ep->driver_data;

	if (!req->context)
		return;
	if (req->status)
		epdata->status = req->status;
	else
		epdata->status = req->actual;
	complete ((struct completion *)req->context);
}

 
static int
get_ready_ep (unsigned f_flags, struct ep_data *epdata, bool is_write)
{
	int	val;

	if (f_flags & O_NONBLOCK) {
		if (!mutex_trylock(&epdata->lock))
			goto nonblock;
		if (epdata->state != STATE_EP_ENABLED &&
		    (!is_write || epdata->state != STATE_EP_READY)) {
			mutex_unlock(&epdata->lock);
nonblock:
			val = -EAGAIN;
		} else
			val = 0;
		return val;
	}

	val = mutex_lock_interruptible(&epdata->lock);
	if (val < 0)
		return val;

	switch (epdata->state) {
	case STATE_EP_ENABLED:
		return 0;
	case STATE_EP_READY:			 
		if (is_write)
			return 0;
		fallthrough;
	case STATE_EP_UNBOUND:			 
		break;
	 
	default:				 
		pr_debug ("%s: ep %p not available, state %d\n",
				shortname, epdata, epdata->state);
	}
	mutex_unlock(&epdata->lock);
	return -ENODEV;
}

static ssize_t
ep_io (struct ep_data *epdata, void *buf, unsigned len)
{
	DECLARE_COMPLETION_ONSTACK (done);
	int value;

	spin_lock_irq (&epdata->dev->lock);
	if (likely (epdata->ep != NULL)) {
		struct usb_request	*req = epdata->req;

		req->context = &done;
		req->complete = epio_complete;
		req->buf = buf;
		req->length = len;
		value = usb_ep_queue (epdata->ep, req, GFP_ATOMIC);
	} else
		value = -ENODEV;
	spin_unlock_irq (&epdata->dev->lock);

	if (likely (value == 0)) {
		value = wait_for_completion_interruptible(&done);
		if (value != 0) {
			spin_lock_irq (&epdata->dev->lock);
			if (likely (epdata->ep != NULL)) {
				DBG (epdata->dev, "%s i/o interrupted\n",
						epdata->name);
				usb_ep_dequeue (epdata->ep, epdata->req);
				spin_unlock_irq (&epdata->dev->lock);

				wait_for_completion(&done);
				if (epdata->status == -ECONNRESET)
					epdata->status = -EINTR;
			} else {
				spin_unlock_irq (&epdata->dev->lock);

				DBG (epdata->dev, "endpoint gone\n");
				wait_for_completion(&done);
				epdata->status = -ENODEV;
			}
		}
		return epdata->status;
	}
	return value;
}

static int
ep_release (struct inode *inode, struct file *fd)
{
	struct ep_data		*data = fd->private_data;
	int value;

	value = mutex_lock_interruptible(&data->lock);
	if (value < 0)
		return value;

	 
	if (data->state != STATE_EP_UNBOUND) {
		data->state = STATE_EP_DISABLED;
		data->desc.bDescriptorType = 0;
		data->hs_desc.bDescriptorType = 0;
		usb_ep_disable(data->ep);
	}
	mutex_unlock(&data->lock);
	put_ep (data);
	return 0;
}

static long ep_ioctl(struct file *fd, unsigned code, unsigned long value)
{
	struct ep_data		*data = fd->private_data;
	int			status;

	if ((status = get_ready_ep (fd->f_flags, data, false)) < 0)
		return status;

	spin_lock_irq (&data->dev->lock);
	if (likely (data->ep != NULL)) {
		switch (code) {
		case GADGETFS_FIFO_STATUS:
			status = usb_ep_fifo_status (data->ep);
			break;
		case GADGETFS_FIFO_FLUSH:
			usb_ep_fifo_flush (data->ep);
			break;
		case GADGETFS_CLEAR_HALT:
			status = usb_ep_clear_halt (data->ep);
			break;
		default:
			status = -ENOTTY;
		}
	} else
		status = -ENODEV;
	spin_unlock_irq (&data->dev->lock);
	mutex_unlock(&data->lock);
	return status;
}

 

 

struct kiocb_priv {
	struct usb_request	*req;
	struct ep_data		*epdata;
	struct kiocb		*iocb;
	struct mm_struct	*mm;
	struct work_struct	work;
	void			*buf;
	struct iov_iter		to;
	const void		*to_free;
	unsigned		actual;
};

static int ep_aio_cancel(struct kiocb *iocb)
{
	struct kiocb_priv	*priv = iocb->private;
	struct ep_data		*epdata;
	int			value;

	local_irq_disable();
	epdata = priv->epdata;
	 
	if (likely(epdata && epdata->ep && priv->req))
		value = usb_ep_dequeue (epdata->ep, priv->req);
	else
		value = -EINVAL;
	 
	local_irq_enable();

	return value;
}

static void ep_user_copy_worker(struct work_struct *work)
{
	struct kiocb_priv *priv = container_of(work, struct kiocb_priv, work);
	struct mm_struct *mm = priv->mm;
	struct kiocb *iocb = priv->iocb;
	size_t ret;

	kthread_use_mm(mm);
	ret = copy_to_iter(priv->buf, priv->actual, &priv->to);
	kthread_unuse_mm(mm);
	if (!ret)
		ret = -EFAULT;

	 
	iocb->ki_complete(iocb, ret);

	kfree(priv->buf);
	kfree(priv->to_free);
	kfree(priv);
}

static void ep_aio_complete(struct usb_ep *ep, struct usb_request *req)
{
	struct kiocb		*iocb = req->context;
	struct kiocb_priv	*priv = iocb->private;
	struct ep_data		*epdata = priv->epdata;

	 
	spin_lock(&epdata->dev->lock);
	priv->req = NULL;
	priv->epdata = NULL;

	 
	if (priv->to_free == NULL || unlikely(req->actual == 0)) {
		kfree(req->buf);
		kfree(priv->to_free);
		kfree(priv);
		iocb->private = NULL;
		iocb->ki_complete(iocb,
				req->actual ? req->actual : (long)req->status);
	} else {
		 
		if (unlikely(0 != req->status))
			DBG(epdata->dev, "%s fault %d len %d\n",
				ep->name, req->status, req->actual);

		priv->buf = req->buf;
		priv->actual = req->actual;
		INIT_WORK(&priv->work, ep_user_copy_worker);
		schedule_work(&priv->work);
	}

	usb_ep_free_request(ep, req);
	spin_unlock(&epdata->dev->lock);
	put_ep(epdata);
}

static ssize_t ep_aio(struct kiocb *iocb,
		      struct kiocb_priv *priv,
		      struct ep_data *epdata,
		      char *buf,
		      size_t len)
{
	struct usb_request *req;
	ssize_t value;

	iocb->private = priv;
	priv->iocb = iocb;

	kiocb_set_cancel_fn(iocb, ep_aio_cancel);
	get_ep(epdata);
	priv->epdata = epdata;
	priv->actual = 0;
	priv->mm = current->mm;  

	 
	spin_lock_irq(&epdata->dev->lock);
	value = -ENODEV;
	if (unlikely(epdata->ep == NULL))
		goto fail;

	req = usb_ep_alloc_request(epdata->ep, GFP_ATOMIC);
	value = -ENOMEM;
	if (unlikely(!req))
		goto fail;

	priv->req = req;
	req->buf = buf;
	req->length = len;
	req->complete = ep_aio_complete;
	req->context = iocb;
	value = usb_ep_queue(epdata->ep, req, GFP_ATOMIC);
	if (unlikely(0 != value)) {
		usb_ep_free_request(epdata->ep, req);
		goto fail;
	}
	spin_unlock_irq(&epdata->dev->lock);
	return -EIOCBQUEUED;

fail:
	spin_unlock_irq(&epdata->dev->lock);
	kfree(priv->to_free);
	kfree(priv);
	put_ep(epdata);
	return value;
}

static ssize_t
ep_read_iter(struct kiocb *iocb, struct iov_iter *to)
{
	struct file *file = iocb->ki_filp;
	struct ep_data *epdata = file->private_data;
	size_t len = iov_iter_count(to);
	ssize_t value;
	char *buf;

	if ((value = get_ready_ep(file->f_flags, epdata, false)) < 0)
		return value;

	 
	if (usb_endpoint_dir_in(&epdata->desc)) {
		if (usb_endpoint_xfer_isoc(&epdata->desc) ||
		    !is_sync_kiocb(iocb)) {
			mutex_unlock(&epdata->lock);
			return -EINVAL;
		}
		DBG (epdata->dev, "%s halt\n", epdata->name);
		spin_lock_irq(&epdata->dev->lock);
		if (likely(epdata->ep != NULL))
			usb_ep_set_halt(epdata->ep);
		spin_unlock_irq(&epdata->dev->lock);
		mutex_unlock(&epdata->lock);
		return -EBADMSG;
	}

	buf = kmalloc(len, GFP_KERNEL);
	if (unlikely(!buf)) {
		mutex_unlock(&epdata->lock);
		return -ENOMEM;
	}
	if (is_sync_kiocb(iocb)) {
		value = ep_io(epdata, buf, len);
		if (value >= 0 && (copy_to_iter(buf, value, to) != value))
			value = -EFAULT;
	} else {
		struct kiocb_priv *priv = kzalloc(sizeof *priv, GFP_KERNEL);
		value = -ENOMEM;
		if (!priv)
			goto fail;
		priv->to_free = dup_iter(&priv->to, to, GFP_KERNEL);
		if (!iter_is_ubuf(&priv->to) && !priv->to_free) {
			kfree(priv);
			goto fail;
		}
		value = ep_aio(iocb, priv, epdata, buf, len);
		if (value == -EIOCBQUEUED)
			buf = NULL;
	}
fail:
	kfree(buf);
	mutex_unlock(&epdata->lock);
	return value;
}

static ssize_t ep_config(struct ep_data *, const char *, size_t);

static ssize_t
ep_write_iter(struct kiocb *iocb, struct iov_iter *from)
{
	struct file *file = iocb->ki_filp;
	struct ep_data *epdata = file->private_data;
	size_t len = iov_iter_count(from);
	bool configured;
	ssize_t value;
	char *buf;

	if ((value = get_ready_ep(file->f_flags, epdata, true)) < 0)
		return value;

	configured = epdata->state == STATE_EP_ENABLED;

	 
	if (configured && !usb_endpoint_dir_in(&epdata->desc)) {
		if (usb_endpoint_xfer_isoc(&epdata->desc) ||
		    !is_sync_kiocb(iocb)) {
			mutex_unlock(&epdata->lock);
			return -EINVAL;
		}
		DBG (epdata->dev, "%s halt\n", epdata->name);
		spin_lock_irq(&epdata->dev->lock);
		if (likely(epdata->ep != NULL))
			usb_ep_set_halt(epdata->ep);
		spin_unlock_irq(&epdata->dev->lock);
		mutex_unlock(&epdata->lock);
		return -EBADMSG;
	}

	buf = kmalloc(len, GFP_KERNEL);
	if (unlikely(!buf)) {
		mutex_unlock(&epdata->lock);
		return -ENOMEM;
	}

	if (unlikely(!copy_from_iter_full(buf, len, from))) {
		value = -EFAULT;
		goto out;
	}

	if (unlikely(!configured)) {
		value = ep_config(epdata, buf, len);
	} else if (is_sync_kiocb(iocb)) {
		value = ep_io(epdata, buf, len);
	} else {
		struct kiocb_priv *priv = kzalloc(sizeof *priv, GFP_KERNEL);
		value = -ENOMEM;
		if (priv) {
			value = ep_aio(iocb, priv, epdata, buf, len);
			if (value == -EIOCBQUEUED)
				buf = NULL;
		}
	}
out:
	kfree(buf);
	mutex_unlock(&epdata->lock);
	return value;
}

 

 
static const struct file_operations ep_io_operations = {
	.owner =	THIS_MODULE,

	.open =		ep_open,
	.release =	ep_release,
	.llseek =	no_llseek,
	.unlocked_ioctl = ep_ioctl,
	.read_iter =	ep_read_iter,
	.write_iter =	ep_write_iter,
};

 
static ssize_t
ep_config (struct ep_data *data, const char *buf, size_t len)
{
	struct usb_ep		*ep;
	u32			tag;
	int			value, length = len;

	if (data->state != STATE_EP_READY) {
		value = -EL2HLT;
		goto fail;
	}

	value = len;
	if (len < USB_DT_ENDPOINT_SIZE + 4)
		goto fail0;

	 
	memcpy(&tag, buf, 4);
	if (tag != 1) {
		DBG(data->dev, "config %s, bad tag %d\n", data->name, tag);
		goto fail0;
	}
	buf += 4;
	len -= 4;

	 

	 
	memcpy(&data->desc, buf, USB_DT_ENDPOINT_SIZE);
	if (data->desc.bLength != USB_DT_ENDPOINT_SIZE
			|| data->desc.bDescriptorType != USB_DT_ENDPOINT)
		goto fail0;
	if (len != USB_DT_ENDPOINT_SIZE) {
		if (len != 2 * USB_DT_ENDPOINT_SIZE)
			goto fail0;
		memcpy(&data->hs_desc, buf + USB_DT_ENDPOINT_SIZE,
			USB_DT_ENDPOINT_SIZE);
		if (data->hs_desc.bLength != USB_DT_ENDPOINT_SIZE
				|| data->hs_desc.bDescriptorType
					!= USB_DT_ENDPOINT) {
			DBG(data->dev, "config %s, bad hs length or type\n",
					data->name);
			goto fail0;
		}
	}

	spin_lock_irq (&data->dev->lock);
	if (data->dev->state == STATE_DEV_UNBOUND) {
		value = -ENOENT;
		goto gone;
	} else {
		ep = data->ep;
		if (ep == NULL) {
			value = -ENODEV;
			goto gone;
		}
	}
	switch (data->dev->gadget->speed) {
	case USB_SPEED_LOW:
	case USB_SPEED_FULL:
		ep->desc = &data->desc;
		break;
	case USB_SPEED_HIGH:
		 
		ep->desc = &data->hs_desc;
		break;
	default:
		DBG(data->dev, "unconnected, %s init abandoned\n",
				data->name);
		value = -EINVAL;
		goto gone;
	}
	value = usb_ep_enable(ep);
	if (value == 0) {
		data->state = STATE_EP_ENABLED;
		value = length;
	}
gone:
	spin_unlock_irq (&data->dev->lock);
	if (value < 0) {
fail:
		data->desc.bDescriptorType = 0;
		data->hs_desc.bDescriptorType = 0;
	}
	return value;
fail0:
	value = -EINVAL;
	goto fail;
}

static int
ep_open (struct inode *inode, struct file *fd)
{
	struct ep_data		*data = inode->i_private;
	int			value = -EBUSY;

	if (mutex_lock_interruptible(&data->lock) != 0)
		return -EINTR;
	spin_lock_irq (&data->dev->lock);
	if (data->dev->state == STATE_DEV_UNBOUND)
		value = -ENOENT;
	else if (data->state == STATE_EP_DISABLED) {
		value = 0;
		data->state = STATE_EP_READY;
		get_ep (data);
		fd->private_data = data;
		VDEBUG (data->dev, "%s ready\n", data->name);
	} else
		DBG (data->dev, "%s state %d\n",
			data->name, data->state);
	spin_unlock_irq (&data->dev->lock);
	mutex_unlock(&data->lock);
	return value;
}

 

 

static inline void ep0_readable (struct dev_data *dev)
{
	wake_up (&dev->wait);
	kill_fasync (&dev->fasync, SIGIO, POLL_IN);
}

static void clean_req (struct usb_ep *ep, struct usb_request *req)
{
	struct dev_data		*dev = ep->driver_data;

	if (req->buf != dev->rbuf) {
		kfree(req->buf);
		req->buf = dev->rbuf;
	}
	req->complete = epio_complete;
	dev->setup_out_ready = 0;
}

static void ep0_complete (struct usb_ep *ep, struct usb_request *req)
{
	struct dev_data		*dev = ep->driver_data;
	unsigned long		flags;
	int			free = 1;

	 
	spin_lock_irqsave(&dev->lock, flags);
	if (!dev->setup_in) {
		dev->setup_out_error = (req->status != 0);
		if (!dev->setup_out_error)
			free = 0;
		dev->setup_out_ready = 1;
		ep0_readable (dev);
	}

	 
	if (free && req->buf != &dev->rbuf)
		clean_req (ep, req);
	req->complete = epio_complete;
	spin_unlock_irqrestore(&dev->lock, flags);
}

static int setup_req (struct usb_ep *ep, struct usb_request *req, u16 len)
{
	struct dev_data	*dev = ep->driver_data;

	if (dev->setup_out_ready) {
		DBG (dev, "ep0 request busy!\n");
		return -EBUSY;
	}
	if (len > sizeof (dev->rbuf))
		req->buf = kmalloc(len, GFP_ATOMIC);
	if (req->buf == NULL) {
		req->buf = dev->rbuf;
		return -ENOMEM;
	}
	req->complete = ep0_complete;
	req->length = len;
	req->zero = 0;
	return 0;
}

static ssize_t
ep0_read (struct file *fd, char __user *buf, size_t len, loff_t *ptr)
{
	struct dev_data			*dev = fd->private_data;
	ssize_t				retval;
	enum ep0_state			state;

	spin_lock_irq (&dev->lock);
	if (dev->state <= STATE_DEV_OPENED) {
		retval = -EINVAL;
		goto done;
	}

	 
	if (dev->setup_abort) {
		dev->setup_abort = 0;
		retval = -EIDRM;
		goto done;
	}

	 
	if ((state = dev->state) == STATE_DEV_SETUP) {

		if (dev->setup_in) {		 
			VDEBUG(dev, "ep0in stall\n");
			(void) usb_ep_set_halt (dev->gadget->ep0);
			retval = -EL2HLT;
			dev->state = STATE_DEV_CONNECTED;

		} else if (len == 0) {		 
			struct usb_ep		*ep = dev->gadget->ep0;
			struct usb_request	*req = dev->req;

			if ((retval = setup_req (ep, req, 0)) == 0) {
				++dev->udc_usage;
				spin_unlock_irq (&dev->lock);
				retval = usb_ep_queue (ep, req, GFP_KERNEL);
				spin_lock_irq (&dev->lock);
				--dev->udc_usage;
			}
			dev->state = STATE_DEV_CONNECTED;

			 
			if (dev->current_config) {
				unsigned power;

				if (gadget_is_dualspeed(dev->gadget)
						&& (dev->gadget->speed
							== USB_SPEED_HIGH))
					power = dev->hs_config->bMaxPower;
				else
					power = dev->config->bMaxPower;
				usb_gadget_vbus_draw(dev->gadget, 2 * power);
			}

		} else {			 
			if ((fd->f_flags & O_NONBLOCK) != 0
					&& !dev->setup_out_ready) {
				retval = -EAGAIN;
				goto done;
			}
			spin_unlock_irq (&dev->lock);
			retval = wait_event_interruptible (dev->wait,
					dev->setup_out_ready != 0);

			 
			spin_lock_irq (&dev->lock);
			if (retval)
				goto done;

			if (dev->state != STATE_DEV_SETUP) {
				retval = -ECANCELED;
				goto done;
			}
			dev->state = STATE_DEV_CONNECTED;

			if (dev->setup_out_error)
				retval = -EIO;
			else {
				len = min (len, (size_t)dev->req->actual);
				++dev->udc_usage;
				spin_unlock_irq(&dev->lock);
				if (copy_to_user (buf, dev->req->buf, len))
					retval = -EFAULT;
				else
					retval = len;
				spin_lock_irq(&dev->lock);
				--dev->udc_usage;
				clean_req (dev->gadget->ep0, dev->req);
				 
			}
		}
		goto done;
	}

	 
	if (len < sizeof dev->event [0]) {
		retval = -EINVAL;
		goto done;
	}
	len -= len % sizeof (struct usb_gadgetfs_event);
	dev->usermode_setup = 1;

scan:
	 
	if (dev->ev_next != 0) {
		unsigned		i, n;

		n = len / sizeof (struct usb_gadgetfs_event);
		if (dev->ev_next < n)
			n = dev->ev_next;

		 
		for (i = 0; i < n; i++) {
			if (dev->event [i].type == GADGETFS_SETUP) {
				dev->state = STATE_DEV_SETUP;
				n = i + 1;
				break;
			}
		}
		spin_unlock_irq (&dev->lock);
		len = n * sizeof (struct usb_gadgetfs_event);
		if (copy_to_user (buf, &dev->event, len))
			retval = -EFAULT;
		else
			retval = len;
		if (len > 0) {
			 
			spin_lock_irq (&dev->lock);
			if (dev->ev_next > n) {
				memmove(&dev->event[0], &dev->event[n],
					sizeof (struct usb_gadgetfs_event)
						* (dev->ev_next - n));
			}
			dev->ev_next -= n;
			spin_unlock_irq (&dev->lock);
		}
		return retval;
	}
	if (fd->f_flags & O_NONBLOCK) {
		retval = -EAGAIN;
		goto done;
	}

	switch (state) {
	default:
		DBG (dev, "fail %s, state %d\n", __func__, state);
		retval = -ESRCH;
		break;
	case STATE_DEV_UNCONNECTED:
	case STATE_DEV_CONNECTED:
		spin_unlock_irq (&dev->lock);
		DBG (dev, "%s wait\n", __func__);

		 
		retval = wait_event_interruptible (dev->wait,
				dev->ev_next != 0);
		if (retval < 0)
			return retval;
		spin_lock_irq (&dev->lock);
		goto scan;
	}

done:
	spin_unlock_irq (&dev->lock);
	return retval;
}

static struct usb_gadgetfs_event *
next_event (struct dev_data *dev, enum usb_gadgetfs_event_type type)
{
	struct usb_gadgetfs_event	*event;
	unsigned			i;

	switch (type) {
	 
	case GADGETFS_DISCONNECT:
		if (dev->state == STATE_DEV_SETUP)
			dev->setup_abort = 1;
		fallthrough;
	case GADGETFS_CONNECT:
		dev->ev_next = 0;
		break;
	case GADGETFS_SETUP:		 
	case GADGETFS_SUSPEND:		 
		 
		for (i = 0; i != dev->ev_next; i++) {
			if (dev->event [i].type != type)
				continue;
			DBG(dev, "discard old event[%d] %d\n", i, type);
			dev->ev_next--;
			if (i == dev->ev_next)
				break;
			 
			memmove (&dev->event [i], &dev->event [i + 1],
				sizeof (struct usb_gadgetfs_event)
					* (dev->ev_next - i));
		}
		break;
	default:
		BUG ();
	}
	VDEBUG(dev, "event[%d] = %d\n", dev->ev_next, type);
	event = &dev->event [dev->ev_next++];
	BUG_ON (dev->ev_next > N_EVENT);
	memset (event, 0, sizeof *event);
	event->type = type;
	return event;
}

static ssize_t
ep0_write (struct file *fd, const char __user *buf, size_t len, loff_t *ptr)
{
	struct dev_data		*dev = fd->private_data;
	ssize_t			retval = -ESRCH;

	 
	if (dev->setup_abort) {
		dev->setup_abort = 0;
		retval = -EIDRM;

	 
	} else if (dev->state == STATE_DEV_SETUP) {

		len = min_t(size_t, len, dev->setup_wLength);
		if (dev->setup_in) {
			retval = setup_req (dev->gadget->ep0, dev->req, len);
			if (retval == 0) {
				dev->state = STATE_DEV_CONNECTED;
				++dev->udc_usage;
				spin_unlock_irq (&dev->lock);
				if (copy_from_user (dev->req->buf, buf, len))
					retval = -EFAULT;
				else {
					if (len < dev->setup_wLength)
						dev->req->zero = 1;
					retval = usb_ep_queue (
						dev->gadget->ep0, dev->req,
						GFP_KERNEL);
				}
				spin_lock_irq(&dev->lock);
				--dev->udc_usage;
				if (retval < 0) {
					clean_req (dev->gadget->ep0, dev->req);
				} else
					retval = len;

				return retval;
			}

		 
		} else if (dev->setup_can_stall) {
			VDEBUG(dev, "ep0out stall\n");
			(void) usb_ep_set_halt (dev->gadget->ep0);
			retval = -EL2HLT;
			dev->state = STATE_DEV_CONNECTED;
		} else {
			DBG(dev, "bogus ep0out stall!\n");
		}
	} else
		DBG (dev, "fail %s, state %d\n", __func__, dev->state);

	return retval;
}

static int
ep0_fasync (int f, struct file *fd, int on)
{
	struct dev_data		*dev = fd->private_data;
	 
	VDEBUG (dev, "%s %s\n", __func__, on ? "on" : "off");
	return fasync_helper (f, fd, on, &dev->fasync);
}

static struct usb_gadget_driver gadgetfs_driver;

static int
dev_release (struct inode *inode, struct file *fd)
{
	struct dev_data		*dev = fd->private_data;

	 

	if (dev->gadget_registered) {
		usb_gadget_unregister_driver (&gadgetfs_driver);
		dev->gadget_registered = false;
	}

	 

	kfree (dev->buf);
	dev->buf = NULL;

	 
	spin_lock_irq(&dev->lock);
	dev->state = STATE_DEV_DISABLED;
	spin_unlock_irq(&dev->lock);

	put_dev (dev);
	return 0;
}

static __poll_t
ep0_poll (struct file *fd, poll_table *wait)
{
	struct dev_data         *dev = fd->private_data;
	__poll_t                mask = 0;

	if (dev->state <= STATE_DEV_OPENED)
		return DEFAULT_POLLMASK;

	poll_wait(fd, &dev->wait, wait);

	spin_lock_irq(&dev->lock);

	 
	if (dev->setup_abort) {
		dev->setup_abort = 0;
		mask = EPOLLHUP;
		goto out;
	}

	if (dev->state == STATE_DEV_SETUP) {
		if (dev->setup_in || dev->setup_can_stall)
			mask = EPOLLOUT;
	} else {
		if (dev->ev_next != 0)
			mask = EPOLLIN;
	}
out:
	spin_unlock_irq(&dev->lock);
	return mask;
}

static long gadget_dev_ioctl (struct file *fd, unsigned code, unsigned long value)
{
	struct dev_data		*dev = fd->private_data;
	struct usb_gadget	*gadget = dev->gadget;
	long ret = -ENOTTY;

	spin_lock_irq(&dev->lock);
	if (dev->state == STATE_DEV_OPENED ||
			dev->state == STATE_DEV_UNBOUND) {
		 
	} else if (gadget->ops->ioctl) {
		++dev->udc_usage;
		spin_unlock_irq(&dev->lock);

		ret = gadget->ops->ioctl (gadget, code, value);

		spin_lock_irq(&dev->lock);
		--dev->udc_usage;
	}
	spin_unlock_irq(&dev->lock);

	return ret;
}

 

 

static void make_qualifier (struct dev_data *dev)
{
	struct usb_qualifier_descriptor		qual;
	struct usb_device_descriptor		*desc;

	qual.bLength = sizeof qual;
	qual.bDescriptorType = USB_DT_DEVICE_QUALIFIER;
	qual.bcdUSB = cpu_to_le16 (0x0200);

	desc = dev->dev;
	qual.bDeviceClass = desc->bDeviceClass;
	qual.bDeviceSubClass = desc->bDeviceSubClass;
	qual.bDeviceProtocol = desc->bDeviceProtocol;

	 
	qual.bMaxPacketSize0 = dev->gadget->ep0->maxpacket;

	qual.bNumConfigurations = 1;
	qual.bRESERVED = 0;

	memcpy (dev->rbuf, &qual, sizeof qual);
}

static int
config_buf (struct dev_data *dev, u8 type, unsigned index)
{
	int		len;
	int		hs = 0;

	 
	if (index > 0)
		return -EINVAL;

	if (gadget_is_dualspeed(dev->gadget)) {
		hs = (dev->gadget->speed == USB_SPEED_HIGH);
		if (type == USB_DT_OTHER_SPEED_CONFIG)
			hs = !hs;
	}
	if (hs) {
		dev->req->buf = dev->hs_config;
		len = le16_to_cpu(dev->hs_config->wTotalLength);
	} else {
		dev->req->buf = dev->config;
		len = le16_to_cpu(dev->config->wTotalLength);
	}
	((u8 *)dev->req->buf) [1] = type;
	return len;
}

static int
gadgetfs_setup (struct usb_gadget *gadget, const struct usb_ctrlrequest *ctrl)
{
	struct dev_data			*dev = get_gadget_data (gadget);
	struct usb_request		*req = dev->req;
	int				value = -EOPNOTSUPP;
	struct usb_gadgetfs_event	*event;
	u16				w_value = le16_to_cpu(ctrl->wValue);
	u16				w_length = le16_to_cpu(ctrl->wLength);

	if (w_length > RBUF_SIZE) {
		if (ctrl->bRequestType & USB_DIR_IN) {
			 
			__le16 *temp = (__le16 *)&ctrl->wLength;

			*temp = cpu_to_le16(RBUF_SIZE);
			w_length = RBUF_SIZE;
		} else {
			return value;
		}
	}

	spin_lock (&dev->lock);
	dev->setup_abort = 0;
	if (dev->state == STATE_DEV_UNCONNECTED) {
		if (gadget_is_dualspeed(gadget)
				&& gadget->speed == USB_SPEED_HIGH
				&& dev->hs_config == NULL) {
			spin_unlock(&dev->lock);
			ERROR (dev, "no high speed config??\n");
			return -EINVAL;
		}

		dev->state = STATE_DEV_CONNECTED;

		INFO (dev, "connected\n");
		event = next_event (dev, GADGETFS_CONNECT);
		event->u.speed = gadget->speed;
		ep0_readable (dev);

	 
	} else if (dev->state == STATE_DEV_SETUP)
		dev->setup_abort = 1;

	req->buf = dev->rbuf;
	req->context = NULL;
	switch (ctrl->bRequest) {

	case USB_REQ_GET_DESCRIPTOR:
		if (ctrl->bRequestType != USB_DIR_IN)
			goto unrecognized;
		switch (w_value >> 8) {

		case USB_DT_DEVICE:
			value = min (w_length, (u16) sizeof *dev->dev);
			dev->dev->bMaxPacketSize0 = dev->gadget->ep0->maxpacket;
			req->buf = dev->dev;
			break;
		case USB_DT_DEVICE_QUALIFIER:
			if (!dev->hs_config)
				break;
			value = min (w_length, (u16)
				sizeof (struct usb_qualifier_descriptor));
			make_qualifier (dev);
			break;
		case USB_DT_OTHER_SPEED_CONFIG:
		case USB_DT_CONFIG:
			value = config_buf (dev,
					w_value >> 8,
					w_value & 0xff);
			if (value >= 0)
				value = min (w_length, (u16) value);
			break;
		case USB_DT_STRING:
			goto unrecognized;

		default:		 
			break;
		}
		break;

	 
	case USB_REQ_SET_CONFIGURATION:
		if (ctrl->bRequestType != 0)
			goto unrecognized;
		if (0 == (u8) w_value) {
			value = 0;
			dev->current_config = 0;
			usb_gadget_vbus_draw(gadget, 8   );
			 
		} else {
			u8	config, power;

			if (gadget_is_dualspeed(gadget)
					&& gadget->speed == USB_SPEED_HIGH) {
				config = dev->hs_config->bConfigurationValue;
				power = dev->hs_config->bMaxPower;
			} else {
				config = dev->config->bConfigurationValue;
				power = dev->config->bMaxPower;
			}

			if (config == (u8) w_value) {
				value = 0;
				dev->current_config = config;
				usb_gadget_vbus_draw(gadget, 2 * power);
			}
		}

		 
		if (value == 0) {
			INFO (dev, "configuration #%d\n", dev->current_config);
			usb_gadget_set_state(gadget, USB_STATE_CONFIGURED);
			if (dev->usermode_setup) {
				dev->setup_can_stall = 0;
				goto delegate;
			}
		}
		break;

#ifndef	CONFIG_USB_PXA25X
	 
	case USB_REQ_GET_CONFIGURATION:
		if (ctrl->bRequestType != 0x80)
			goto unrecognized;
		*(u8 *)req->buf = dev->current_config;
		value = min (w_length, (u16) 1);
		break;
#endif

	default:
unrecognized:
		VDEBUG (dev, "%s req%02x.%02x v%04x i%04x l%d\n",
			dev->usermode_setup ? "delegate" : "fail",
			ctrl->bRequestType, ctrl->bRequest,
			w_value, le16_to_cpu(ctrl->wIndex), w_length);

		 
		if (dev->usermode_setup) {
			dev->setup_can_stall = 1;
delegate:
			dev->setup_in = (ctrl->bRequestType & USB_DIR_IN)
						? 1 : 0;
			dev->setup_wLength = w_length;
			dev->setup_out_ready = 0;
			dev->setup_out_error = 0;

			 
			if (unlikely (!dev->setup_in && w_length)) {
				value = setup_req (gadget->ep0, dev->req,
							w_length);
				if (value < 0)
					break;

				++dev->udc_usage;
				spin_unlock (&dev->lock);
				value = usb_ep_queue (gadget->ep0, dev->req,
							GFP_KERNEL);
				spin_lock (&dev->lock);
				--dev->udc_usage;
				if (value < 0) {
					clean_req (gadget->ep0, dev->req);
					break;
				}

				 
				dev->setup_can_stall = 0;
			}

			 
			event = next_event (dev, GADGETFS_SETUP);
			event->u.setup = *ctrl;
			ep0_readable (dev);
			spin_unlock (&dev->lock);
			return 0;
		}
	}

	 
	if (value >= 0 && dev->state != STATE_DEV_SETUP) {
		req->length = value;
		req->zero = value < w_length;

		++dev->udc_usage;
		spin_unlock (&dev->lock);
		value = usb_ep_queue (gadget->ep0, req, GFP_KERNEL);
		spin_lock(&dev->lock);
		--dev->udc_usage;
		spin_unlock(&dev->lock);
		if (value < 0) {
			DBG (dev, "ep_queue --> %d\n", value);
			req->status = 0;
		}
		return value;
	}

	 
	spin_unlock (&dev->lock);
	return value;
}

static void destroy_ep_files (struct dev_data *dev)
{
	DBG (dev, "%s %d\n", __func__, dev->state);

	 
	spin_lock_irq (&dev->lock);
	while (!list_empty(&dev->epfiles)) {
		struct ep_data	*ep;
		struct inode	*parent;
		struct dentry	*dentry;

		 
		ep = list_first_entry (&dev->epfiles, struct ep_data, epfiles);
		list_del_init (&ep->epfiles);
		spin_unlock_irq (&dev->lock);

		dentry = ep->dentry;
		ep->dentry = NULL;
		parent = d_inode(dentry->d_parent);

		 
		mutex_lock(&ep->lock);
		if (ep->state == STATE_EP_ENABLED)
			(void) usb_ep_disable (ep->ep);
		ep->state = STATE_EP_UNBOUND;
		usb_ep_free_request (ep->ep, ep->req);
		ep->ep = NULL;
		mutex_unlock(&ep->lock);

		wake_up (&ep->wait);
		put_ep (ep);

		 
		inode_lock(parent);
		d_delete (dentry);
		dput (dentry);
		inode_unlock(parent);

		spin_lock_irq (&dev->lock);
	}
	spin_unlock_irq (&dev->lock);
}


static struct dentry *
gadgetfs_create_file (struct super_block *sb, char const *name,
		void *data, const struct file_operations *fops);

static int activate_ep_files (struct dev_data *dev)
{
	struct usb_ep	*ep;
	struct ep_data	*data;

	gadget_for_each_ep (ep, dev->gadget) {

		data = kzalloc(sizeof(*data), GFP_KERNEL);
		if (!data)
			goto enomem0;
		data->state = STATE_EP_DISABLED;
		mutex_init(&data->lock);
		init_waitqueue_head (&data->wait);

		strncpy (data->name, ep->name, sizeof (data->name) - 1);
		refcount_set (&data->count, 1);
		data->dev = dev;
		get_dev (dev);

		data->ep = ep;
		ep->driver_data = data;

		data->req = usb_ep_alloc_request (ep, GFP_KERNEL);
		if (!data->req)
			goto enomem1;

		data->dentry = gadgetfs_create_file (dev->sb, data->name,
				data, &ep_io_operations);
		if (!data->dentry)
			goto enomem2;
		list_add_tail (&data->epfiles, &dev->epfiles);
	}
	return 0;

enomem2:
	usb_ep_free_request (ep, data->req);
enomem1:
	put_dev (dev);
	kfree (data);
enomem0:
	DBG (dev, "%s enomem\n", __func__);
	destroy_ep_files (dev);
	return -ENOMEM;
}

static void
gadgetfs_unbind (struct usb_gadget *gadget)
{
	struct dev_data		*dev = get_gadget_data (gadget);

	DBG (dev, "%s\n", __func__);

	spin_lock_irq (&dev->lock);
	dev->state = STATE_DEV_UNBOUND;
	while (dev->udc_usage > 0) {
		spin_unlock_irq(&dev->lock);
		usleep_range(1000, 2000);
		spin_lock_irq(&dev->lock);
	}
	spin_unlock_irq (&dev->lock);

	destroy_ep_files (dev);
	gadget->ep0->driver_data = NULL;
	set_gadget_data (gadget, NULL);

	 
	if (dev->req)
		usb_ep_free_request (gadget->ep0, dev->req);
	DBG (dev, "%s done\n", __func__);
	put_dev (dev);
}

static struct dev_data		*the_device;

static int gadgetfs_bind(struct usb_gadget *gadget,
		struct usb_gadget_driver *driver)
{
	struct dev_data		*dev = the_device;

	if (!dev)
		return -ESRCH;
	if (0 != strcmp (CHIP, gadget->name)) {
		pr_err("%s expected %s controller not %s\n",
			shortname, CHIP, gadget->name);
		return -ENODEV;
	}

	set_gadget_data (gadget, dev);
	dev->gadget = gadget;
	gadget->ep0->driver_data = dev;

	 
	dev->req = usb_ep_alloc_request (gadget->ep0, GFP_KERNEL);
	if (!dev->req)
		goto enomem;
	dev->req->context = NULL;
	dev->req->complete = epio_complete;

	if (activate_ep_files (dev) < 0)
		goto enomem;

	INFO (dev, "bound to %s driver\n", gadget->name);
	spin_lock_irq(&dev->lock);
	dev->state = STATE_DEV_UNCONNECTED;
	spin_unlock_irq(&dev->lock);
	get_dev (dev);
	return 0;

enomem:
	gadgetfs_unbind (gadget);
	return -ENOMEM;
}

static void
gadgetfs_disconnect (struct usb_gadget *gadget)
{
	struct dev_data		*dev = get_gadget_data (gadget);
	unsigned long		flags;

	spin_lock_irqsave (&dev->lock, flags);
	if (dev->state == STATE_DEV_UNCONNECTED)
		goto exit;
	dev->state = STATE_DEV_UNCONNECTED;

	INFO (dev, "disconnected\n");
	next_event (dev, GADGETFS_DISCONNECT);
	ep0_readable (dev);
exit:
	spin_unlock_irqrestore (&dev->lock, flags);
}

static void
gadgetfs_suspend (struct usb_gadget *gadget)
{
	struct dev_data		*dev = get_gadget_data (gadget);
	unsigned long		flags;

	INFO (dev, "suspended from state %d\n", dev->state);
	spin_lock_irqsave(&dev->lock, flags);
	switch (dev->state) {
	case STATE_DEV_SETUP:		 
	case STATE_DEV_CONNECTED:
	case STATE_DEV_UNCONNECTED:
		next_event (dev, GADGETFS_SUSPEND);
		ep0_readable (dev);
		fallthrough;
	default:
		break;
	}
	spin_unlock_irqrestore(&dev->lock, flags);
}

static struct usb_gadget_driver gadgetfs_driver = {
	.function	= (char *) driver_desc,
	.bind		= gadgetfs_bind,
	.unbind		= gadgetfs_unbind,
	.setup		= gadgetfs_setup,
	.reset		= gadgetfs_disconnect,
	.disconnect	= gadgetfs_disconnect,
	.suspend	= gadgetfs_suspend,

	.driver	= {
		.name		= shortname,
	},
};

 
 

static int is_valid_config(struct usb_config_descriptor *config,
		unsigned int total)
{
	return config->bDescriptorType == USB_DT_CONFIG
		&& config->bLength == USB_DT_CONFIG_SIZE
		&& total >= USB_DT_CONFIG_SIZE
		&& config->bConfigurationValue != 0
		&& (config->bmAttributes & USB_CONFIG_ATT_ONE) != 0
		&& (config->bmAttributes & USB_CONFIG_ATT_WAKEUP) == 0;
	 
	 
}

static ssize_t
dev_config (struct file *fd, const char __user *buf, size_t len, loff_t *ptr)
{
	struct dev_data		*dev = fd->private_data;
	ssize_t			value, length = len;
	unsigned		total;
	u32			tag;
	char			*kbuf;

	spin_lock_irq(&dev->lock);
	if (dev->state > STATE_DEV_OPENED) {
		value = ep0_write(fd, buf, len, ptr);
		spin_unlock_irq(&dev->lock);
		return value;
	}
	spin_unlock_irq(&dev->lock);

	if ((len < (USB_DT_CONFIG_SIZE + USB_DT_DEVICE_SIZE + 4)) ||
	    (len > PAGE_SIZE * 4))
		return -EINVAL;

	 
	if (copy_from_user (&tag, buf, 4))
		return -EFAULT;
	if (tag != 0)
		return -EINVAL;
	buf += 4;
	length -= 4;

	kbuf = memdup_user(buf, length);
	if (IS_ERR(kbuf))
		return PTR_ERR(kbuf);

	spin_lock_irq (&dev->lock);
	value = -EINVAL;
	if (dev->buf) {
		spin_unlock_irq(&dev->lock);
		kfree(kbuf);
		return value;
	}
	dev->buf = kbuf;

	 
	dev->config = (void *) kbuf;
	total = le16_to_cpu(dev->config->wTotalLength);
	if (!is_valid_config(dev->config, total) ||
			total > length - USB_DT_DEVICE_SIZE)
		goto fail;
	kbuf += total;
	length -= total;

	 
	if (kbuf [1] == USB_DT_CONFIG) {
		dev->hs_config = (void *) kbuf;
		total = le16_to_cpu(dev->hs_config->wTotalLength);
		if (!is_valid_config(dev->hs_config, total) ||
				total > length - USB_DT_DEVICE_SIZE)
			goto fail;
		kbuf += total;
		length -= total;
	} else {
		dev->hs_config = NULL;
	}

	 

	 
	if (length != USB_DT_DEVICE_SIZE)
		goto fail;
	dev->dev = (void *)kbuf;
	if (dev->dev->bLength != USB_DT_DEVICE_SIZE
			|| dev->dev->bDescriptorType != USB_DT_DEVICE
			|| dev->dev->bNumConfigurations != 1)
		goto fail;
	dev->dev->bcdUSB = cpu_to_le16 (0x0200);

	 
	spin_unlock_irq (&dev->lock);
	if (dev->hs_config)
		gadgetfs_driver.max_speed = USB_SPEED_HIGH;
	else
		gadgetfs_driver.max_speed = USB_SPEED_FULL;

	value = usb_gadget_register_driver(&gadgetfs_driver);
	if (value != 0) {
		spin_lock_irq(&dev->lock);
		goto fail;
	} else {
		 
		value = len;
		dev->gadget_registered = true;
	}
	return value;

fail:
	dev->config = NULL;
	dev->hs_config = NULL;
	dev->dev = NULL;
	spin_unlock_irq (&dev->lock);
	pr_debug ("%s: %s fail %zd, %p\n", shortname, __func__, value, dev);
	kfree (dev->buf);
	dev->buf = NULL;
	return value;
}

static int
gadget_dev_open (struct inode *inode, struct file *fd)
{
	struct dev_data		*dev = inode->i_private;
	int			value = -EBUSY;

	spin_lock_irq(&dev->lock);
	if (dev->state == STATE_DEV_DISABLED) {
		dev->ev_next = 0;
		dev->state = STATE_DEV_OPENED;
		fd->private_data = dev;
		get_dev (dev);
		value = 0;
	}
	spin_unlock_irq(&dev->lock);
	return value;
}

static const struct file_operations ep0_operations = {
	.llseek =	no_llseek,

	.open =		gadget_dev_open,
	.read =		ep0_read,
	.write =	dev_config,
	.fasync =	ep0_fasync,
	.poll =		ep0_poll,
	.unlocked_ioctl = gadget_dev_ioctl,
	.release =	dev_release,
};

 

 


 

static unsigned default_uid;
static unsigned default_gid;
static unsigned default_perm = S_IRUSR | S_IWUSR;

module_param (default_uid, uint, 0644);
module_param (default_gid, uint, 0644);
module_param (default_perm, uint, 0644);


static struct inode *
gadgetfs_make_inode (struct super_block *sb,
		void *data, const struct file_operations *fops,
		int mode)
{
	struct inode *inode = new_inode (sb);

	if (inode) {
		inode->i_ino = get_next_ino();
		inode->i_mode = mode;
		inode->i_uid = make_kuid(&init_user_ns, default_uid);
		inode->i_gid = make_kgid(&init_user_ns, default_gid);
		inode->i_atime = inode->i_mtime = inode_set_ctime_current(inode);
		inode->i_private = data;
		inode->i_fop = fops;
	}
	return inode;
}

 
static struct dentry *
gadgetfs_create_file (struct super_block *sb, char const *name,
		void *data, const struct file_operations *fops)
{
	struct dentry	*dentry;
	struct inode	*inode;

	dentry = d_alloc_name(sb->s_root, name);
	if (!dentry)
		return NULL;

	inode = gadgetfs_make_inode (sb, data, fops,
			S_IFREG | (default_perm & S_IRWXUGO));
	if (!inode) {
		dput(dentry);
		return NULL;
	}
	d_add (dentry, inode);
	return dentry;
}

static const struct super_operations gadget_fs_operations = {
	.statfs =	simple_statfs,
	.drop_inode =	generic_delete_inode,
};

static int
gadgetfs_fill_super (struct super_block *sb, struct fs_context *fc)
{
	struct inode	*inode;
	struct dev_data	*dev;
	int		rc;

	mutex_lock(&sb_mutex);

	if (the_device) {
		rc = -ESRCH;
		goto Done;
	}

	CHIP = usb_get_gadget_udc_name();
	if (!CHIP) {
		rc = -ENODEV;
		goto Done;
	}

	 
	sb->s_blocksize = PAGE_SIZE;
	sb->s_blocksize_bits = PAGE_SHIFT;
	sb->s_magic = GADGETFS_MAGIC;
	sb->s_op = &gadget_fs_operations;
	sb->s_time_gran = 1;

	 
	inode = gadgetfs_make_inode (sb,
			NULL, &simple_dir_operations,
			S_IFDIR | S_IRUGO | S_IXUGO);
	if (!inode)
		goto Enomem;
	inode->i_op = &simple_dir_inode_operations;
	if (!(sb->s_root = d_make_root (inode)))
		goto Enomem;

	 
	dev = dev_new ();
	if (!dev)
		goto Enomem;

	dev->sb = sb;
	dev->dentry = gadgetfs_create_file(sb, CHIP, dev, &ep0_operations);
	if (!dev->dentry) {
		put_dev(dev);
		goto Enomem;
	}

	 
	the_device = dev;
	rc = 0;
	goto Done;

 Enomem:
	kfree(CHIP);
	CHIP = NULL;
	rc = -ENOMEM;

 Done:
	mutex_unlock(&sb_mutex);
	return rc;
}

 
static int gadgetfs_get_tree(struct fs_context *fc)
{
	return get_tree_single(fc, gadgetfs_fill_super);
}

static const struct fs_context_operations gadgetfs_context_ops = {
	.get_tree	= gadgetfs_get_tree,
};

static int gadgetfs_init_fs_context(struct fs_context *fc)
{
	fc->ops = &gadgetfs_context_ops;
	return 0;
}

static void
gadgetfs_kill_sb (struct super_block *sb)
{
	mutex_lock(&sb_mutex);
	kill_litter_super (sb);
	if (the_device) {
		put_dev (the_device);
		the_device = NULL;
	}
	kfree(CHIP);
	CHIP = NULL;
	mutex_unlock(&sb_mutex);
}

 

static struct file_system_type gadgetfs_type = {
	.owner		= THIS_MODULE,
	.name		= shortname,
	.init_fs_context = gadgetfs_init_fs_context,
	.kill_sb	= gadgetfs_kill_sb,
};
MODULE_ALIAS_FS("gadgetfs");

 

static int __init gadgetfs_init (void)
{
	int status;

	status = register_filesystem (&gadgetfs_type);
	if (status == 0)
		pr_info ("%s: %s, version " DRIVER_VERSION "\n",
			shortname, driver_desc);
	return status;
}
module_init (gadgetfs_init);

static void __exit gadgetfs_cleanup (void)
{
	pr_debug ("unregister %s\n", shortname);
	unregister_filesystem (&gadgetfs_type);
}
module_exit (gadgetfs_cleanup);

