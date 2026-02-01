
 


 
 

#include <linux/blkdev.h>
#include <linux/pagemap.h>
#include <linux/export.h>
#include <linux/fs_parser.h>
#include <linux/hid.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/scatterlist.h>
#include <linux/sched/signal.h>
#include <linux/uio.h>
#include <linux/vmalloc.h>
#include <asm/unaligned.h>

#include <linux/usb/ccid.h>
#include <linux/usb/composite.h>
#include <linux/usb/functionfs.h>

#include <linux/aio.h>
#include <linux/kthread.h>
#include <linux/poll.h>
#include <linux/eventfd.h>

#include "u_fs.h"
#include "u_f.h"
#include "u_os_desc.h"
#include "configfs.h"

#define FUNCTIONFS_MAGIC	0xa647361  

 
static void ffs_data_get(struct ffs_data *ffs);
static void ffs_data_put(struct ffs_data *ffs);
 
static struct ffs_data *__must_check ffs_data_new(const char *dev_name)
	__attribute__((malloc));

 
static void ffs_data_opened(struct ffs_data *ffs);
static void ffs_data_closed(struct ffs_data *ffs);

 
static int __must_check
__ffs_data_got_descs(struct ffs_data *ffs, char *data, size_t len);
static int __must_check
__ffs_data_got_strings(struct ffs_data *ffs, char *data, size_t len);


 

struct ffs_ep;

struct ffs_function {
	struct usb_configuration	*conf;
	struct usb_gadget		*gadget;
	struct ffs_data			*ffs;

	struct ffs_ep			*eps;
	u8				eps_revmap[16];
	short				*interfaces_nums;

	struct usb_function		function;
};


static struct ffs_function *ffs_func_from_usb(struct usb_function *f)
{
	return container_of(f, struct ffs_function, function);
}


static inline enum ffs_setup_state
ffs_setup_state_clear_cancelled(struct ffs_data *ffs)
{
	return (enum ffs_setup_state)
		cmpxchg(&ffs->setup_state, FFS_SETUP_CANCELLED, FFS_NO_SETUP);
}


static void ffs_func_eps_disable(struct ffs_function *func);
static int __must_check ffs_func_eps_enable(struct ffs_function *func);

static int ffs_func_bind(struct usb_configuration *,
			 struct usb_function *);
static int ffs_func_set_alt(struct usb_function *, unsigned, unsigned);
static void ffs_func_disable(struct usb_function *);
static int ffs_func_setup(struct usb_function *,
			  const struct usb_ctrlrequest *);
static bool ffs_func_req_match(struct usb_function *,
			       const struct usb_ctrlrequest *,
			       bool config0);
static void ffs_func_suspend(struct usb_function *);
static void ffs_func_resume(struct usb_function *);


static int ffs_func_revmap_ep(struct ffs_function *func, u8 num);
static int ffs_func_revmap_intf(struct ffs_function *func, u8 intf);


 

struct ffs_ep {
	struct usb_ep			*ep;	 
	struct usb_request		*req;	 

	 
	struct usb_endpoint_descriptor	*descs[3];

	u8				num;
};

struct ffs_epfile {
	 
	struct mutex			mutex;

	struct ffs_data			*ffs;
	struct ffs_ep			*ep;	 

	struct dentry			*dentry;

	 
	struct ffs_buffer		*read_buffer;
#define READ_BUFFER_DROP ((struct ffs_buffer *)ERR_PTR(-ESHUTDOWN))

	char				name[5];

	unsigned char			in;	 
	unsigned char			isoc;	 

	unsigned char			_pad;
};

struct ffs_buffer {
	size_t length;
	char *data;
	char storage[];
};

 

struct ffs_io_data {
	bool aio;
	bool read;

	struct kiocb *kiocb;
	struct iov_iter data;
	const void *to_free;
	char *buf;

	struct mm_struct *mm;
	struct work_struct work;

	struct usb_ep *ep;
	struct usb_request *req;
	struct sg_table sgt;
	bool use_sg;

	struct ffs_data *ffs;

	int status;
	struct completion done;
};

struct ffs_desc_helper {
	struct ffs_data *ffs;
	unsigned interfaces_count;
	unsigned eps_count;
};

static int  __must_check ffs_epfiles_create(struct ffs_data *ffs);
static void ffs_epfiles_destroy(struct ffs_epfile *epfiles, unsigned count);

static struct dentry *
ffs_sb_create_file(struct super_block *sb, const char *name, void *data,
		   const struct file_operations *fops);

 

DEFINE_MUTEX(ffs_lock);
EXPORT_SYMBOL_GPL(ffs_lock);

static struct ffs_dev *_ffs_find_dev(const char *name);
static struct ffs_dev *_ffs_alloc_dev(void);
static void _ffs_free_dev(struct ffs_dev *dev);
static int ffs_acquire_dev(const char *dev_name, struct ffs_data *ffs_data);
static void ffs_release_dev(struct ffs_dev *ffs_dev);
static int ffs_ready(struct ffs_data *ffs);
static void ffs_closed(struct ffs_data *ffs);

 

static int ffs_mutex_lock(struct mutex *mutex, unsigned nonblock)
	__attribute__((warn_unused_result, nonnull));
static char *ffs_prepare_buffer(const char __user *buf, size_t len)
	__attribute__((warn_unused_result, nonnull));


 

static void ffs_ep0_complete(struct usb_ep *ep, struct usb_request *req)
{
	struct ffs_data *ffs = req->context;

	complete(&ffs->ep0req_completion);
}

static int __ffs_ep0_queue_wait(struct ffs_data *ffs, char *data, size_t len)
	__releases(&ffs->ev.waitq.lock)
{
	struct usb_request *req = ffs->ep0req;
	int ret;

	if (!req) {
		spin_unlock_irq(&ffs->ev.waitq.lock);
		return -EINVAL;
	}

	req->zero     = len < le16_to_cpu(ffs->ev.setup.wLength);

	spin_unlock_irq(&ffs->ev.waitq.lock);

	req->buf      = data;
	req->length   = len;

	 
	if (req->buf == NULL)
		req->buf = (void *)0xDEADBABE;

	reinit_completion(&ffs->ep0req_completion);

	ret = usb_ep_queue(ffs->gadget->ep0, req, GFP_ATOMIC);
	if (ret < 0)
		return ret;

	ret = wait_for_completion_interruptible(&ffs->ep0req_completion);
	if (ret) {
		usb_ep_dequeue(ffs->gadget->ep0, req);
		return -EINTR;
	}

	ffs->setup_state = FFS_NO_SETUP;
	return req->status ? req->status : req->actual;
}

static int __ffs_ep0_stall(struct ffs_data *ffs)
{
	if (ffs->ev.can_stall) {
		pr_vdebug("ep0 stall\n");
		usb_ep_set_halt(ffs->gadget->ep0);
		ffs->setup_state = FFS_NO_SETUP;
		return -EL2HLT;
	} else {
		pr_debug("bogus ep0 stall!\n");
		return -ESRCH;
	}
}

static ssize_t ffs_ep0_write(struct file *file, const char __user *buf,
			     size_t len, loff_t *ptr)
{
	struct ffs_data *ffs = file->private_data;
	ssize_t ret;
	char *data;

	 
	if (ffs_setup_state_clear_cancelled(ffs) == FFS_SETUP_CANCELLED)
		return -EIDRM;

	 
	ret = ffs_mutex_lock(&ffs->mutex, file->f_flags & O_NONBLOCK);
	if (ret < 0)
		return ret;

	 
	switch (ffs->state) {
	case FFS_READ_DESCRIPTORS:
	case FFS_READ_STRINGS:
		 
		if (len < 16) {
			ret = -EINVAL;
			break;
		}

		data = ffs_prepare_buffer(buf, len);
		if (IS_ERR(data)) {
			ret = PTR_ERR(data);
			break;
		}

		 
		if (ffs->state == FFS_READ_DESCRIPTORS) {
			pr_info("read descriptors\n");
			ret = __ffs_data_got_descs(ffs, data, len);
			if (ret < 0)
				break;

			ffs->state = FFS_READ_STRINGS;
			ret = len;
		} else {
			pr_info("read strings\n");
			ret = __ffs_data_got_strings(ffs, data, len);
			if (ret < 0)
				break;

			ret = ffs_epfiles_create(ffs);
			if (ret) {
				ffs->state = FFS_CLOSING;
				break;
			}

			ffs->state = FFS_ACTIVE;
			mutex_unlock(&ffs->mutex);

			ret = ffs_ready(ffs);
			if (ret < 0) {
				ffs->state = FFS_CLOSING;
				return ret;
			}

			return len;
		}
		break;

	case FFS_ACTIVE:
		data = NULL;
		 
		spin_lock_irq(&ffs->ev.waitq.lock);
		switch (ffs_setup_state_clear_cancelled(ffs)) {
		case FFS_SETUP_CANCELLED:
			ret = -EIDRM;
			goto done_spin;

		case FFS_NO_SETUP:
			ret = -ESRCH;
			goto done_spin;

		case FFS_SETUP_PENDING:
			break;
		}

		 
		if (!(ffs->ev.setup.bRequestType & USB_DIR_IN)) {
			spin_unlock_irq(&ffs->ev.waitq.lock);
			ret = __ffs_ep0_stall(ffs);
			break;
		}

		 
		len = min(len, (size_t)le16_to_cpu(ffs->ev.setup.wLength));

		spin_unlock_irq(&ffs->ev.waitq.lock);

		data = ffs_prepare_buffer(buf, len);
		if (IS_ERR(data)) {
			ret = PTR_ERR(data);
			break;
		}

		spin_lock_irq(&ffs->ev.waitq.lock);

		 
		if (ffs_setup_state_clear_cancelled(ffs) ==
		    FFS_SETUP_CANCELLED) {
			ret = -EIDRM;
done_spin:
			spin_unlock_irq(&ffs->ev.waitq.lock);
		} else {
			 
			ret = __ffs_ep0_queue_wait(ffs, data, len);
		}
		kfree(data);
		break;

	default:
		ret = -EBADFD;
		break;
	}

	mutex_unlock(&ffs->mutex);
	return ret;
}

 
static ssize_t __ffs_ep0_read_events(struct ffs_data *ffs, char __user *buf,
				     size_t n)
	__releases(&ffs->ev.waitq.lock)
{
	 
	struct usb_functionfs_event events[ARRAY_SIZE(ffs->ev.types)];
	const size_t size = n * sizeof *events;
	unsigned i = 0;

	memset(events, 0, size);

	do {
		events[i].type = ffs->ev.types[i];
		if (events[i].type == FUNCTIONFS_SETUP) {
			events[i].u.setup = ffs->ev.setup;
			ffs->setup_state = FFS_SETUP_PENDING;
		}
	} while (++i < n);

	ffs->ev.count -= n;
	if (ffs->ev.count)
		memmove(ffs->ev.types, ffs->ev.types + n,
			ffs->ev.count * sizeof *ffs->ev.types);

	spin_unlock_irq(&ffs->ev.waitq.lock);
	mutex_unlock(&ffs->mutex);

	return copy_to_user(buf, events, size) ? -EFAULT : size;
}

static ssize_t ffs_ep0_read(struct file *file, char __user *buf,
			    size_t len, loff_t *ptr)
{
	struct ffs_data *ffs = file->private_data;
	char *data = NULL;
	size_t n;
	int ret;

	 
	if (ffs_setup_state_clear_cancelled(ffs) == FFS_SETUP_CANCELLED)
		return -EIDRM;

	 
	ret = ffs_mutex_lock(&ffs->mutex, file->f_flags & O_NONBLOCK);
	if (ret < 0)
		return ret;

	 
	if (ffs->state != FFS_ACTIVE) {
		ret = -EBADFD;
		goto done_mutex;
	}

	 
	spin_lock_irq(&ffs->ev.waitq.lock);

	switch (ffs_setup_state_clear_cancelled(ffs)) {
	case FFS_SETUP_CANCELLED:
		ret = -EIDRM;
		break;

	case FFS_NO_SETUP:
		n = len / sizeof(struct usb_functionfs_event);
		if (!n) {
			ret = -EINVAL;
			break;
		}

		if ((file->f_flags & O_NONBLOCK) && !ffs->ev.count) {
			ret = -EAGAIN;
			break;
		}

		if (wait_event_interruptible_exclusive_locked_irq(ffs->ev.waitq,
							ffs->ev.count)) {
			ret = -EINTR;
			break;
		}

		 
		return __ffs_ep0_read_events(ffs, buf,
					     min(n, (size_t)ffs->ev.count));

	case FFS_SETUP_PENDING:
		if (ffs->ev.setup.bRequestType & USB_DIR_IN) {
			spin_unlock_irq(&ffs->ev.waitq.lock);
			ret = __ffs_ep0_stall(ffs);
			goto done_mutex;
		}

		len = min(len, (size_t)le16_to_cpu(ffs->ev.setup.wLength));

		spin_unlock_irq(&ffs->ev.waitq.lock);

		if (len) {
			data = kmalloc(len, GFP_KERNEL);
			if (!data) {
				ret = -ENOMEM;
				goto done_mutex;
			}
		}

		spin_lock_irq(&ffs->ev.waitq.lock);

		 
		if (ffs_setup_state_clear_cancelled(ffs) ==
		    FFS_SETUP_CANCELLED) {
			ret = -EIDRM;
			break;
		}

		 
		ret = __ffs_ep0_queue_wait(ffs, data, len);
		if ((ret > 0) && (copy_to_user(buf, data, len)))
			ret = -EFAULT;
		goto done_mutex;

	default:
		ret = -EBADFD;
		break;
	}

	spin_unlock_irq(&ffs->ev.waitq.lock);
done_mutex:
	mutex_unlock(&ffs->mutex);
	kfree(data);
	return ret;
}

static int ffs_ep0_open(struct inode *inode, struct file *file)
{
	struct ffs_data *ffs = inode->i_private;

	if (ffs->state == FFS_CLOSING)
		return -EBUSY;

	file->private_data = ffs;
	ffs_data_opened(ffs);

	return stream_open(inode, file);
}

static int ffs_ep0_release(struct inode *inode, struct file *file)
{
	struct ffs_data *ffs = file->private_data;

	ffs_data_closed(ffs);

	return 0;
}

static long ffs_ep0_ioctl(struct file *file, unsigned code, unsigned long value)
{
	struct ffs_data *ffs = file->private_data;
	struct usb_gadget *gadget = ffs->gadget;
	long ret;

	if (code == FUNCTIONFS_INTERFACE_REVMAP) {
		struct ffs_function *func = ffs->func;
		ret = func ? ffs_func_revmap_intf(func, value) : -ENODEV;
	} else if (gadget && gadget->ops->ioctl) {
		ret = gadget->ops->ioctl(gadget, code, value);
	} else {
		ret = -ENOTTY;
	}

	return ret;
}

static __poll_t ffs_ep0_poll(struct file *file, poll_table *wait)
{
	struct ffs_data *ffs = file->private_data;
	__poll_t mask = EPOLLWRNORM;
	int ret;

	poll_wait(file, &ffs->ev.waitq, wait);

	ret = ffs_mutex_lock(&ffs->mutex, file->f_flags & O_NONBLOCK);
	if (ret < 0)
		return mask;

	switch (ffs->state) {
	case FFS_READ_DESCRIPTORS:
	case FFS_READ_STRINGS:
		mask |= EPOLLOUT;
		break;

	case FFS_ACTIVE:
		switch (ffs->setup_state) {
		case FFS_NO_SETUP:
			if (ffs->ev.count)
				mask |= EPOLLIN;
			break;

		case FFS_SETUP_PENDING:
		case FFS_SETUP_CANCELLED:
			mask |= (EPOLLIN | EPOLLOUT);
			break;
		}
		break;

	case FFS_CLOSING:
		break;
	case FFS_DEACTIVATED:
		break;
	}

	mutex_unlock(&ffs->mutex);

	return mask;
}

static const struct file_operations ffs_ep0_operations = {
	.llseek =	no_llseek,

	.open =		ffs_ep0_open,
	.write =	ffs_ep0_write,
	.read =		ffs_ep0_read,
	.release =	ffs_ep0_release,
	.unlocked_ioctl =	ffs_ep0_ioctl,
	.poll =		ffs_ep0_poll,
};


 

static void ffs_epfile_io_complete(struct usb_ep *_ep, struct usb_request *req)
{
	struct ffs_io_data *io_data = req->context;

	if (req->status)
		io_data->status = req->status;
	else
		io_data->status = req->actual;

	complete(&io_data->done);
}

static ssize_t ffs_copy_to_iter(void *data, int data_len, struct iov_iter *iter)
{
	ssize_t ret = copy_to_iter(data, data_len, iter);
	if (ret == data_len)
		return ret;

	if (iov_iter_count(iter))
		return -EFAULT;

	 
	pr_err("functionfs read size %d > requested size %zd, dropping excess data. "
	       "Align read buffer size to max packet size to avoid the problem.\n",
	       data_len, ret);

	return ret;
}

 
static void *ffs_build_sg_list(struct sg_table *sgt, size_t sz)
{
	struct page **pages;
	void *vaddr, *ptr;
	unsigned int n_pages;
	int i;

	vaddr = vmalloc(sz);
	if (!vaddr)
		return NULL;

	n_pages = PAGE_ALIGN(sz) >> PAGE_SHIFT;
	pages = kvmalloc_array(n_pages, sizeof(struct page *), GFP_KERNEL);
	if (!pages) {
		vfree(vaddr);

		return NULL;
	}
	for (i = 0, ptr = vaddr; i < n_pages; ++i, ptr += PAGE_SIZE)
		pages[i] = vmalloc_to_page(ptr);

	if (sg_alloc_table_from_pages(sgt, pages, n_pages, 0, sz, GFP_KERNEL)) {
		kvfree(pages);
		vfree(vaddr);

		return NULL;
	}
	kvfree(pages);

	return vaddr;
}

static inline void *ffs_alloc_buffer(struct ffs_io_data *io_data,
	size_t data_len)
{
	if (io_data->use_sg)
		return ffs_build_sg_list(&io_data->sgt, data_len);

	return kmalloc(data_len, GFP_KERNEL);
}

static inline void ffs_free_buffer(struct ffs_io_data *io_data)
{
	if (!io_data->buf)
		return;

	if (io_data->use_sg) {
		sg_free_table(&io_data->sgt);
		vfree(io_data->buf);
	} else {
		kfree(io_data->buf);
	}
}

static void ffs_user_copy_worker(struct work_struct *work)
{
	struct ffs_io_data *io_data = container_of(work, struct ffs_io_data,
						   work);
	int ret = io_data->status;
	bool kiocb_has_eventfd = io_data->kiocb->ki_flags & IOCB_EVENTFD;

	if (io_data->read && ret > 0) {
		kthread_use_mm(io_data->mm);
		ret = ffs_copy_to_iter(io_data->buf, ret, &io_data->data);
		kthread_unuse_mm(io_data->mm);
	}

	io_data->kiocb->ki_complete(io_data->kiocb, ret);

	if (io_data->ffs->ffs_eventfd && !kiocb_has_eventfd)
		eventfd_signal(io_data->ffs->ffs_eventfd, 1);

	if (io_data->read)
		kfree(io_data->to_free);
	ffs_free_buffer(io_data);
	kfree(io_data);
}

static void ffs_epfile_async_io_complete(struct usb_ep *_ep,
					 struct usb_request *req)
{
	struct ffs_io_data *io_data = req->context;
	struct ffs_data *ffs = io_data->ffs;

	io_data->status = req->status ? req->status : req->actual;
	usb_ep_free_request(_ep, req);

	INIT_WORK(&io_data->work, ffs_user_copy_worker);
	queue_work(ffs->io_completion_wq, &io_data->work);
}

static void __ffs_epfile_read_buffer_free(struct ffs_epfile *epfile)
{
	 
	struct ffs_buffer *buf = xchg(&epfile->read_buffer, READ_BUFFER_DROP);
	if (buf && buf != READ_BUFFER_DROP)
		kfree(buf);
}

 
static ssize_t __ffs_epfile_read_buffered(struct ffs_epfile *epfile,
					  struct iov_iter *iter)
{
	 
	struct ffs_buffer *buf = xchg(&epfile->read_buffer, NULL);
	ssize_t ret;
	if (!buf || buf == READ_BUFFER_DROP)
		return 0;

	ret = copy_to_iter(buf->data, buf->length, iter);
	if (buf->length == ret) {
		kfree(buf);
		return ret;
	}

	if (iov_iter_count(iter)) {
		ret = -EFAULT;
	} else {
		buf->length -= ret;
		buf->data += ret;
	}

	if (cmpxchg(&epfile->read_buffer, NULL, buf))
		kfree(buf);

	return ret;
}

 
static ssize_t __ffs_epfile_read_data(struct ffs_epfile *epfile,
				      void *data, int data_len,
				      struct iov_iter *iter)
{
	struct ffs_buffer *buf;

	ssize_t ret = copy_to_iter(data, data_len, iter);
	if (data_len == ret)
		return ret;

	if (iov_iter_count(iter))
		return -EFAULT;

	 
	pr_warn("functionfs read size %d > requested size %zd, splitting request into multiple reads.",
		data_len, ret);

	data_len -= ret;
	buf = kmalloc(struct_size(buf, storage, data_len), GFP_KERNEL);
	if (!buf)
		return -ENOMEM;
	buf->length = data_len;
	buf->data = buf->storage;
	memcpy(buf->storage, data + ret, flex_array_size(buf, storage, data_len));

	 
	if (cmpxchg(&epfile->read_buffer, NULL, buf))
		kfree(buf);

	return ret;
}

static ssize_t ffs_epfile_io(struct file *file, struct ffs_io_data *io_data)
{
	struct ffs_epfile *epfile = file->private_data;
	struct usb_request *req;
	struct ffs_ep *ep;
	char *data = NULL;
	ssize_t ret, data_len = -EINVAL;
	int halt;

	 
	if (WARN_ON(epfile->ffs->state != FFS_ACTIVE))
		return -ENODEV;

	 
	ep = epfile->ep;
	if (!ep) {
		if (file->f_flags & O_NONBLOCK)
			return -EAGAIN;

		ret = wait_event_interruptible(
				epfile->ffs->wait, (ep = epfile->ep));
		if (ret)
			return -EINTR;
	}

	 
	halt = (!io_data->read == !epfile->in);
	if (halt && epfile->isoc)
		return -EINVAL;

	 
	ret = ffs_mutex_lock(&epfile->mutex, file->f_flags & O_NONBLOCK);
	if (ret)
		goto error;

	 
	if (!halt) {
		struct usb_gadget *gadget;

		 
		if (!io_data->aio && io_data->read) {
			ret = __ffs_epfile_read_buffered(epfile, &io_data->data);
			if (ret)
				goto error_mutex;
		}

		 
		gadget = epfile->ffs->gadget;

		spin_lock_irq(&epfile->ffs->eps_lock);
		 
		if (epfile->ep != ep) {
			ret = -ESHUTDOWN;
			goto error_lock;
		}
		data_len = iov_iter_count(&io_data->data);
		 
		if (io_data->read)
			data_len = usb_ep_align_maybe(gadget, ep->ep, data_len);

		io_data->use_sg = gadget->sg_supported && data_len > PAGE_SIZE;
		spin_unlock_irq(&epfile->ffs->eps_lock);

		data = ffs_alloc_buffer(io_data, data_len);
		if (!data) {
			ret = -ENOMEM;
			goto error_mutex;
		}
		if (!io_data->read &&
		    !copy_from_iter_full(data, data_len, &io_data->data)) {
			ret = -EFAULT;
			goto error_mutex;
		}
	}

	spin_lock_irq(&epfile->ffs->eps_lock);

	if (epfile->ep != ep) {
		 
		ret = -ESHUTDOWN;
	} else if (halt) {
		ret = usb_ep_set_halt(ep->ep);
		if (!ret)
			ret = -EBADMSG;
	} else if (data_len == -EINVAL) {
		 
		WARN(1, "%s: data_len == -EINVAL\n", __func__);
		ret = -EINVAL;
	} else if (!io_data->aio) {
		bool interrupted = false;

		req = ep->req;
		if (io_data->use_sg) {
			req->buf = NULL;
			req->sg	= io_data->sgt.sgl;
			req->num_sgs = io_data->sgt.nents;
		} else {
			req->buf = data;
			req->num_sgs = 0;
		}
		req->length = data_len;

		io_data->buf = data;

		init_completion(&io_data->done);
		req->context  = io_data;
		req->complete = ffs_epfile_io_complete;

		ret = usb_ep_queue(ep->ep, req, GFP_ATOMIC);
		if (ret < 0)
			goto error_lock;

		spin_unlock_irq(&epfile->ffs->eps_lock);

		if (wait_for_completion_interruptible(&io_data->done)) {
			spin_lock_irq(&epfile->ffs->eps_lock);
			if (epfile->ep != ep) {
				ret = -ESHUTDOWN;
				goto error_lock;
			}
			 
			usb_ep_dequeue(ep->ep, req);
			spin_unlock_irq(&epfile->ffs->eps_lock);
			wait_for_completion(&io_data->done);
			interrupted = io_data->status < 0;
		}

		if (interrupted)
			ret = -EINTR;
		else if (io_data->read && io_data->status > 0)
			ret = __ffs_epfile_read_data(epfile, data, io_data->status,
						     &io_data->data);
		else
			ret = io_data->status;
		goto error_mutex;
	} else if (!(req = usb_ep_alloc_request(ep->ep, GFP_ATOMIC))) {
		ret = -ENOMEM;
	} else {
		if (io_data->use_sg) {
			req->buf = NULL;
			req->sg	= io_data->sgt.sgl;
			req->num_sgs = io_data->sgt.nents;
		} else {
			req->buf = data;
			req->num_sgs = 0;
		}
		req->length = data_len;

		io_data->buf = data;
		io_data->ep = ep->ep;
		io_data->req = req;
		io_data->ffs = epfile->ffs;

		req->context  = io_data;
		req->complete = ffs_epfile_async_io_complete;

		ret = usb_ep_queue(ep->ep, req, GFP_ATOMIC);
		if (ret) {
			io_data->req = NULL;
			usb_ep_free_request(ep->ep, req);
			goto error_lock;
		}

		ret = -EIOCBQUEUED;
		 
		data = NULL;
	}

error_lock:
	spin_unlock_irq(&epfile->ffs->eps_lock);
error_mutex:
	mutex_unlock(&epfile->mutex);
error:
	if (ret != -EIOCBQUEUED)  
		ffs_free_buffer(io_data);
	return ret;
}

static int
ffs_epfile_open(struct inode *inode, struct file *file)
{
	struct ffs_epfile *epfile = inode->i_private;

	if (WARN_ON(epfile->ffs->state != FFS_ACTIVE))
		return -ENODEV;

	file->private_data = epfile;
	ffs_data_opened(epfile->ffs);

	return stream_open(inode, file);
}

static int ffs_aio_cancel(struct kiocb *kiocb)
{
	struct ffs_io_data *io_data = kiocb->private;
	struct ffs_epfile *epfile = kiocb->ki_filp->private_data;
	unsigned long flags;
	int value;

	spin_lock_irqsave(&epfile->ffs->eps_lock, flags);

	if (io_data && io_data->ep && io_data->req)
		value = usb_ep_dequeue(io_data->ep, io_data->req);
	else
		value = -EINVAL;

	spin_unlock_irqrestore(&epfile->ffs->eps_lock, flags);

	return value;
}

static ssize_t ffs_epfile_write_iter(struct kiocb *kiocb, struct iov_iter *from)
{
	struct ffs_io_data io_data, *p = &io_data;
	ssize_t res;

	if (!is_sync_kiocb(kiocb)) {
		p = kzalloc(sizeof(io_data), GFP_KERNEL);
		if (!p)
			return -ENOMEM;
		p->aio = true;
	} else {
		memset(p, 0, sizeof(*p));
		p->aio = false;
	}

	p->read = false;
	p->kiocb = kiocb;
	p->data = *from;
	p->mm = current->mm;

	kiocb->private = p;

	if (p->aio)
		kiocb_set_cancel_fn(kiocb, ffs_aio_cancel);

	res = ffs_epfile_io(kiocb->ki_filp, p);
	if (res == -EIOCBQUEUED)
		return res;
	if (p->aio)
		kfree(p);
	else
		*from = p->data;
	return res;
}

static ssize_t ffs_epfile_read_iter(struct kiocb *kiocb, struct iov_iter *to)
{
	struct ffs_io_data io_data, *p = &io_data;
	ssize_t res;

	if (!is_sync_kiocb(kiocb)) {
		p = kzalloc(sizeof(io_data), GFP_KERNEL);
		if (!p)
			return -ENOMEM;
		p->aio = true;
	} else {
		memset(p, 0, sizeof(*p));
		p->aio = false;
	}

	p->read = true;
	p->kiocb = kiocb;
	if (p->aio) {
		p->to_free = dup_iter(&p->data, to, GFP_KERNEL);
		if (!iter_is_ubuf(&p->data) && !p->to_free) {
			kfree(p);
			return -ENOMEM;
		}
	} else {
		p->data = *to;
		p->to_free = NULL;
	}
	p->mm = current->mm;

	kiocb->private = p;

	if (p->aio)
		kiocb_set_cancel_fn(kiocb, ffs_aio_cancel);

	res = ffs_epfile_io(kiocb->ki_filp, p);
	if (res == -EIOCBQUEUED)
		return res;

	if (p->aio) {
		kfree(p->to_free);
		kfree(p);
	} else {
		*to = p->data;
	}
	return res;
}

static int
ffs_epfile_release(struct inode *inode, struct file *file)
{
	struct ffs_epfile *epfile = inode->i_private;

	__ffs_epfile_read_buffer_free(epfile);
	ffs_data_closed(epfile->ffs);

	return 0;
}

static long ffs_epfile_ioctl(struct file *file, unsigned code,
			     unsigned long value)
{
	struct ffs_epfile *epfile = file->private_data;
	struct ffs_ep *ep;
	int ret;

	if (WARN_ON(epfile->ffs->state != FFS_ACTIVE))
		return -ENODEV;

	 
	ep = epfile->ep;
	if (!ep) {
		if (file->f_flags & O_NONBLOCK)
			return -EAGAIN;

		ret = wait_event_interruptible(
				epfile->ffs->wait, (ep = epfile->ep));
		if (ret)
			return -EINTR;
	}

	spin_lock_irq(&epfile->ffs->eps_lock);

	 
	if (epfile->ep != ep) {
		spin_unlock_irq(&epfile->ffs->eps_lock);
		return -ESHUTDOWN;
	}

	switch (code) {
	case FUNCTIONFS_FIFO_STATUS:
		ret = usb_ep_fifo_status(epfile->ep->ep);
		break;
	case FUNCTIONFS_FIFO_FLUSH:
		usb_ep_fifo_flush(epfile->ep->ep);
		ret = 0;
		break;
	case FUNCTIONFS_CLEAR_HALT:
		ret = usb_ep_clear_halt(epfile->ep->ep);
		break;
	case FUNCTIONFS_ENDPOINT_REVMAP:
		ret = epfile->ep->num;
		break;
	case FUNCTIONFS_ENDPOINT_DESC:
	{
		int desc_idx;
		struct usb_endpoint_descriptor desc1, *desc;

		switch (epfile->ffs->gadget->speed) {
		case USB_SPEED_SUPER:
		case USB_SPEED_SUPER_PLUS:
			desc_idx = 2;
			break;
		case USB_SPEED_HIGH:
			desc_idx = 1;
			break;
		default:
			desc_idx = 0;
		}

		desc = epfile->ep->descs[desc_idx];
		memcpy(&desc1, desc, desc->bLength);

		spin_unlock_irq(&epfile->ffs->eps_lock);
		ret = copy_to_user((void __user *)value, &desc1, desc1.bLength);
		if (ret)
			ret = -EFAULT;
		return ret;
	}
	default:
		ret = -ENOTTY;
	}
	spin_unlock_irq(&epfile->ffs->eps_lock);

	return ret;
}

static const struct file_operations ffs_epfile_operations = {
	.llseek =	no_llseek,

	.open =		ffs_epfile_open,
	.write_iter =	ffs_epfile_write_iter,
	.read_iter =	ffs_epfile_read_iter,
	.release =	ffs_epfile_release,
	.unlocked_ioctl =	ffs_epfile_ioctl,
	.compat_ioctl = compat_ptr_ioctl,
};


 

 

static struct inode *__must_check
ffs_sb_make_inode(struct super_block *sb, void *data,
		  const struct file_operations *fops,
		  const struct inode_operations *iops,
		  struct ffs_file_perms *perms)
{
	struct inode *inode;

	inode = new_inode(sb);

	if (inode) {
		struct timespec64 ts = inode_set_ctime_current(inode);

		inode->i_ino	 = get_next_ino();
		inode->i_mode    = perms->mode;
		inode->i_uid     = perms->uid;
		inode->i_gid     = perms->gid;
		inode->i_atime   = ts;
		inode->i_mtime   = ts;
		inode->i_private = data;
		if (fops)
			inode->i_fop = fops;
		if (iops)
			inode->i_op  = iops;
	}

	return inode;
}

 
static struct dentry *ffs_sb_create_file(struct super_block *sb,
					const char *name, void *data,
					const struct file_operations *fops)
{
	struct ffs_data	*ffs = sb->s_fs_info;
	struct dentry	*dentry;
	struct inode	*inode;

	dentry = d_alloc_name(sb->s_root, name);
	if (!dentry)
		return NULL;

	inode = ffs_sb_make_inode(sb, data, fops, NULL, &ffs->file_perms);
	if (!inode) {
		dput(dentry);
		return NULL;
	}

	d_add(dentry, inode);
	return dentry;
}

 
static const struct super_operations ffs_sb_operations = {
	.statfs =	simple_statfs,
	.drop_inode =	generic_delete_inode,
};

struct ffs_sb_fill_data {
	struct ffs_file_perms perms;
	umode_t root_mode;
	const char *dev_name;
	bool no_disconnect;
	struct ffs_data *ffs_data;
};

static int ffs_sb_fill(struct super_block *sb, struct fs_context *fc)
{
	struct ffs_sb_fill_data *data = fc->fs_private;
	struct inode	*inode;
	struct ffs_data	*ffs = data->ffs_data;

	ffs->sb              = sb;
	data->ffs_data       = NULL;
	sb->s_fs_info        = ffs;
	sb->s_blocksize      = PAGE_SIZE;
	sb->s_blocksize_bits = PAGE_SHIFT;
	sb->s_magic          = FUNCTIONFS_MAGIC;
	sb->s_op             = &ffs_sb_operations;
	sb->s_time_gran      = 1;

	 
	data->perms.mode = data->root_mode;
	inode = ffs_sb_make_inode(sb, NULL,
				  &simple_dir_operations,
				  &simple_dir_inode_operations,
				  &data->perms);
	sb->s_root = d_make_root(inode);
	if (!sb->s_root)
		return -ENOMEM;

	 
	if (!ffs_sb_create_file(sb, "ep0", ffs, &ffs_ep0_operations))
		return -ENOMEM;

	return 0;
}

enum {
	Opt_no_disconnect,
	Opt_rmode,
	Opt_fmode,
	Opt_mode,
	Opt_uid,
	Opt_gid,
};

static const struct fs_parameter_spec ffs_fs_fs_parameters[] = {
	fsparam_bool	("no_disconnect",	Opt_no_disconnect),
	fsparam_u32	("rmode",		Opt_rmode),
	fsparam_u32	("fmode",		Opt_fmode),
	fsparam_u32	("mode",		Opt_mode),
	fsparam_u32	("uid",			Opt_uid),
	fsparam_u32	("gid",			Opt_gid),
	{}
};

static int ffs_fs_parse_param(struct fs_context *fc, struct fs_parameter *param)
{
	struct ffs_sb_fill_data *data = fc->fs_private;
	struct fs_parse_result result;
	int opt;

	opt = fs_parse(fc, ffs_fs_fs_parameters, param, &result);
	if (opt < 0)
		return opt;

	switch (opt) {
	case Opt_no_disconnect:
		data->no_disconnect = result.boolean;
		break;
	case Opt_rmode:
		data->root_mode  = (result.uint_32 & 0555) | S_IFDIR;
		break;
	case Opt_fmode:
		data->perms.mode = (result.uint_32 & 0666) | S_IFREG;
		break;
	case Opt_mode:
		data->root_mode  = (result.uint_32 & 0555) | S_IFDIR;
		data->perms.mode = (result.uint_32 & 0666) | S_IFREG;
		break;

	case Opt_uid:
		data->perms.uid = make_kuid(current_user_ns(), result.uint_32);
		if (!uid_valid(data->perms.uid))
			goto unmapped_value;
		break;
	case Opt_gid:
		data->perms.gid = make_kgid(current_user_ns(), result.uint_32);
		if (!gid_valid(data->perms.gid))
			goto unmapped_value;
		break;

	default:
		return -ENOPARAM;
	}

	return 0;

unmapped_value:
	return invalf(fc, "%s: unmapped value: %u", param->key, result.uint_32);
}

 
static int ffs_fs_get_tree(struct fs_context *fc)
{
	struct ffs_sb_fill_data *ctx = fc->fs_private;
	struct ffs_data	*ffs;
	int ret;

	if (!fc->source)
		return invalf(fc, "No source specified");

	ffs = ffs_data_new(fc->source);
	if (!ffs)
		return -ENOMEM;
	ffs->file_perms = ctx->perms;
	ffs->no_disconnect = ctx->no_disconnect;

	ffs->dev_name = kstrdup(fc->source, GFP_KERNEL);
	if (!ffs->dev_name) {
		ffs_data_put(ffs);
		return -ENOMEM;
	}

	ret = ffs_acquire_dev(ffs->dev_name, ffs);
	if (ret) {
		ffs_data_put(ffs);
		return ret;
	}

	ctx->ffs_data = ffs;
	return get_tree_nodev(fc, ffs_sb_fill);
}

static void ffs_fs_free_fc(struct fs_context *fc)
{
	struct ffs_sb_fill_data *ctx = fc->fs_private;

	if (ctx) {
		if (ctx->ffs_data) {
			ffs_data_put(ctx->ffs_data);
		}

		kfree(ctx);
	}
}

static const struct fs_context_operations ffs_fs_context_ops = {
	.free		= ffs_fs_free_fc,
	.parse_param	= ffs_fs_parse_param,
	.get_tree	= ffs_fs_get_tree,
};

static int ffs_fs_init_fs_context(struct fs_context *fc)
{
	struct ffs_sb_fill_data *ctx;

	ctx = kzalloc(sizeof(struct ffs_sb_fill_data), GFP_KERNEL);
	if (!ctx)
		return -ENOMEM;

	ctx->perms.mode = S_IFREG | 0600;
	ctx->perms.uid = GLOBAL_ROOT_UID;
	ctx->perms.gid = GLOBAL_ROOT_GID;
	ctx->root_mode = S_IFDIR | 0500;
	ctx->no_disconnect = false;

	fc->fs_private = ctx;
	fc->ops = &ffs_fs_context_ops;
	return 0;
}

static void
ffs_fs_kill_sb(struct super_block *sb)
{
	kill_litter_super(sb);
	if (sb->s_fs_info)
		ffs_data_closed(sb->s_fs_info);
}

static struct file_system_type ffs_fs_type = {
	.owner		= THIS_MODULE,
	.name		= "functionfs",
	.init_fs_context = ffs_fs_init_fs_context,
	.parameters	= ffs_fs_fs_parameters,
	.kill_sb	= ffs_fs_kill_sb,
};
MODULE_ALIAS_FS("functionfs");


 

static int functionfs_init(void)
{
	int ret;

	ret = register_filesystem(&ffs_fs_type);
	if (!ret)
		pr_info("file system registered\n");
	else
		pr_err("failed registering file system (%d)\n", ret);

	return ret;
}

static void functionfs_cleanup(void)
{
	pr_info("unloading\n");
	unregister_filesystem(&ffs_fs_type);
}


 

static void ffs_data_clear(struct ffs_data *ffs);
static void ffs_data_reset(struct ffs_data *ffs);

static void ffs_data_get(struct ffs_data *ffs)
{
	refcount_inc(&ffs->ref);
}

static void ffs_data_opened(struct ffs_data *ffs)
{
	refcount_inc(&ffs->ref);
	if (atomic_add_return(1, &ffs->opened) == 1 &&
			ffs->state == FFS_DEACTIVATED) {
		ffs->state = FFS_CLOSING;
		ffs_data_reset(ffs);
	}
}

static void ffs_data_put(struct ffs_data *ffs)
{
	if (refcount_dec_and_test(&ffs->ref)) {
		pr_info("%s(): freeing\n", __func__);
		ffs_data_clear(ffs);
		ffs_release_dev(ffs->private_data);
		BUG_ON(waitqueue_active(&ffs->ev.waitq) ||
		       swait_active(&ffs->ep0req_completion.wait) ||
		       waitqueue_active(&ffs->wait));
		destroy_workqueue(ffs->io_completion_wq);
		kfree(ffs->dev_name);
		kfree(ffs);
	}
}

static void ffs_data_closed(struct ffs_data *ffs)
{
	struct ffs_epfile *epfiles;
	unsigned long flags;

	if (atomic_dec_and_test(&ffs->opened)) {
		if (ffs->no_disconnect) {
			ffs->state = FFS_DEACTIVATED;
			spin_lock_irqsave(&ffs->eps_lock, flags);
			epfiles = ffs->epfiles;
			ffs->epfiles = NULL;
			spin_unlock_irqrestore(&ffs->eps_lock,
							flags);

			if (epfiles)
				ffs_epfiles_destroy(epfiles,
						 ffs->eps_count);

			if (ffs->setup_state == FFS_SETUP_PENDING)
				__ffs_ep0_stall(ffs);
		} else {
			ffs->state = FFS_CLOSING;
			ffs_data_reset(ffs);
		}
	}
	if (atomic_read(&ffs->opened) < 0) {
		ffs->state = FFS_CLOSING;
		ffs_data_reset(ffs);
	}

	ffs_data_put(ffs);
}

static struct ffs_data *ffs_data_new(const char *dev_name)
{
	struct ffs_data *ffs = kzalloc(sizeof *ffs, GFP_KERNEL);
	if (!ffs)
		return NULL;

	ffs->io_completion_wq = alloc_ordered_workqueue("%s", 0, dev_name);
	if (!ffs->io_completion_wq) {
		kfree(ffs);
		return NULL;
	}

	refcount_set(&ffs->ref, 1);
	atomic_set(&ffs->opened, 0);
	ffs->state = FFS_READ_DESCRIPTORS;
	mutex_init(&ffs->mutex);
	spin_lock_init(&ffs->eps_lock);
	init_waitqueue_head(&ffs->ev.waitq);
	init_waitqueue_head(&ffs->wait);
	init_completion(&ffs->ep0req_completion);

	 
	ffs->ev.can_stall = 1;

	return ffs;
}

static void ffs_data_clear(struct ffs_data *ffs)
{
	struct ffs_epfile *epfiles;
	unsigned long flags;

	ffs_closed(ffs);

	BUG_ON(ffs->gadget);

	spin_lock_irqsave(&ffs->eps_lock, flags);
	epfiles = ffs->epfiles;
	ffs->epfiles = NULL;
	spin_unlock_irqrestore(&ffs->eps_lock, flags);

	 
	if (epfiles) {
		ffs_epfiles_destroy(epfiles, ffs->eps_count);
		ffs->epfiles = NULL;
	}

	if (ffs->ffs_eventfd) {
		eventfd_ctx_put(ffs->ffs_eventfd);
		ffs->ffs_eventfd = NULL;
	}

	kfree(ffs->raw_descs_data);
	kfree(ffs->raw_strings);
	kfree(ffs->stringtabs);
}

static void ffs_data_reset(struct ffs_data *ffs)
{
	ffs_data_clear(ffs);

	ffs->raw_descs_data = NULL;
	ffs->raw_descs = NULL;
	ffs->raw_strings = NULL;
	ffs->stringtabs = NULL;

	ffs->raw_descs_length = 0;
	ffs->fs_descs_count = 0;
	ffs->hs_descs_count = 0;
	ffs->ss_descs_count = 0;

	ffs->strings_count = 0;
	ffs->interfaces_count = 0;
	ffs->eps_count = 0;

	ffs->ev.count = 0;

	ffs->state = FFS_READ_DESCRIPTORS;
	ffs->setup_state = FFS_NO_SETUP;
	ffs->flags = 0;

	ffs->ms_os_descs_ext_prop_count = 0;
	ffs->ms_os_descs_ext_prop_name_len = 0;
	ffs->ms_os_descs_ext_prop_data_len = 0;
}


static int functionfs_bind(struct ffs_data *ffs, struct usb_composite_dev *cdev)
{
	struct usb_gadget_strings **lang;
	int first_id;

	if (WARN_ON(ffs->state != FFS_ACTIVE
		 || test_and_set_bit(FFS_FL_BOUND, &ffs->flags)))
		return -EBADFD;

	first_id = usb_string_ids_n(cdev, ffs->strings_count);
	if (first_id < 0)
		return first_id;

	ffs->ep0req = usb_ep_alloc_request(cdev->gadget->ep0, GFP_KERNEL);
	if (!ffs->ep0req)
		return -ENOMEM;
	ffs->ep0req->complete = ffs_ep0_complete;
	ffs->ep0req->context = ffs;

	lang = ffs->stringtabs;
	if (lang) {
		for (; *lang; ++lang) {
			struct usb_string *str = (*lang)->strings;
			int id = first_id;
			for (; str->s; ++id, ++str)
				str->id = id;
		}
	}

	ffs->gadget = cdev->gadget;
	ffs_data_get(ffs);
	return 0;
}

static void functionfs_unbind(struct ffs_data *ffs)
{
	if (!WARN_ON(!ffs->gadget)) {
		 
		usb_ep_dequeue(ffs->gadget->ep0, ffs->ep0req);
		mutex_lock(&ffs->mutex);
		usb_ep_free_request(ffs->gadget->ep0, ffs->ep0req);
		ffs->ep0req = NULL;
		ffs->gadget = NULL;
		clear_bit(FFS_FL_BOUND, &ffs->flags);
		mutex_unlock(&ffs->mutex);
		ffs_data_put(ffs);
	}
}

static int ffs_epfiles_create(struct ffs_data *ffs)
{
	struct ffs_epfile *epfile, *epfiles;
	unsigned i, count;

	count = ffs->eps_count;
	epfiles = kcalloc(count, sizeof(*epfiles), GFP_KERNEL);
	if (!epfiles)
		return -ENOMEM;

	epfile = epfiles;
	for (i = 1; i <= count; ++i, ++epfile) {
		epfile->ffs = ffs;
		mutex_init(&epfile->mutex);
		if (ffs->user_flags & FUNCTIONFS_VIRTUAL_ADDR)
			sprintf(epfile->name, "ep%02x", ffs->eps_addrmap[i]);
		else
			sprintf(epfile->name, "ep%u", i);
		epfile->dentry = ffs_sb_create_file(ffs->sb, epfile->name,
						 epfile,
						 &ffs_epfile_operations);
		if (!epfile->dentry) {
			ffs_epfiles_destroy(epfiles, i - 1);
			return -ENOMEM;
		}
	}

	ffs->epfiles = epfiles;
	return 0;
}

static void ffs_epfiles_destroy(struct ffs_epfile *epfiles, unsigned count)
{
	struct ffs_epfile *epfile = epfiles;

	for (; count; --count, ++epfile) {
		BUG_ON(mutex_is_locked(&epfile->mutex));
		if (epfile->dentry) {
			d_delete(epfile->dentry);
			dput(epfile->dentry);
			epfile->dentry = NULL;
		}
	}

	kfree(epfiles);
}

static void ffs_func_eps_disable(struct ffs_function *func)
{
	struct ffs_ep *ep;
	struct ffs_epfile *epfile;
	unsigned short count;
	unsigned long flags;

	spin_lock_irqsave(&func->ffs->eps_lock, flags);
	count = func->ffs->eps_count;
	epfile = func->ffs->epfiles;
	ep = func->eps;
	while (count--) {
		 
		if (ep->ep)
			usb_ep_disable(ep->ep);
		++ep;

		if (epfile) {
			epfile->ep = NULL;
			__ffs_epfile_read_buffer_free(epfile);
			++epfile;
		}
	}
	spin_unlock_irqrestore(&func->ffs->eps_lock, flags);
}

static int ffs_func_eps_enable(struct ffs_function *func)
{
	struct ffs_data *ffs;
	struct ffs_ep *ep;
	struct ffs_epfile *epfile;
	unsigned short count;
	unsigned long flags;
	int ret = 0;

	spin_lock_irqsave(&func->ffs->eps_lock, flags);
	ffs = func->ffs;
	ep = func->eps;
	epfile = ffs->epfiles;
	count = ffs->eps_count;
	while(count--) {
		ep->ep->driver_data = ep;

		ret = config_ep_by_speed(func->gadget, &func->function, ep->ep);
		if (ret) {
			pr_err("%s: config_ep_by_speed(%s) returned %d\n",
					__func__, ep->ep->name, ret);
			break;
		}

		ret = usb_ep_enable(ep->ep);
		if (!ret) {
			epfile->ep = ep;
			epfile->in = usb_endpoint_dir_in(ep->ep->desc);
			epfile->isoc = usb_endpoint_xfer_isoc(ep->ep->desc);
		} else {
			break;
		}

		++ep;
		++epfile;
	}

	wake_up_interruptible(&ffs->wait);
	spin_unlock_irqrestore(&func->ffs->eps_lock, flags);

	return ret;
}


 

 

enum ffs_entity_type {
	FFS_DESCRIPTOR, FFS_INTERFACE, FFS_STRING, FFS_ENDPOINT
};

enum ffs_os_desc_type {
	FFS_OS_DESC, FFS_OS_DESC_EXT_COMPAT, FFS_OS_DESC_EXT_PROP
};

typedef int (*ffs_entity_callback)(enum ffs_entity_type entity,
				   u8 *valuep,
				   struct usb_descriptor_header *desc,
				   void *priv);

typedef int (*ffs_os_desc_callback)(enum ffs_os_desc_type entity,
				    struct usb_os_desc_header *h, void *data,
				    unsigned len, void *priv);

static int __must_check ffs_do_single_desc(char *data, unsigned len,
					   ffs_entity_callback entity,
					   void *priv, int *current_class)
{
	struct usb_descriptor_header *_ds = (void *)data;
	u8 length;
	int ret;

	 
	if (len < 2) {
		pr_vdebug("descriptor too short\n");
		return -EINVAL;
	}

	 
	length = _ds->bLength;
	if (len < length) {
		pr_vdebug("descriptor longer then available data\n");
		return -EINVAL;
	}

#define __entity_check_INTERFACE(val)  1
#define __entity_check_STRING(val)     (val)
#define __entity_check_ENDPOINT(val)   ((val) & USB_ENDPOINT_NUMBER_MASK)
#define __entity(type, val) do {					\
		pr_vdebug("entity " #type "(%02x)\n", (val));		\
		if (!__entity_check_ ##type(val)) {			\
			pr_vdebug("invalid entity's value\n");		\
			return -EINVAL;					\
		}							\
		ret = entity(FFS_ ##type, &val, _ds, priv);		\
		if (ret < 0) {						\
			pr_debug("entity " #type "(%02x); ret = %d\n",	\
				 (val), ret);				\
			return ret;					\
		}							\
	} while (0)

	 
	switch (_ds->bDescriptorType) {
	case USB_DT_DEVICE:
	case USB_DT_CONFIG:
	case USB_DT_STRING:
	case USB_DT_DEVICE_QUALIFIER:
		 
		pr_vdebug("descriptor reserved for gadget: %d\n",
		      _ds->bDescriptorType);
		return -EINVAL;

	case USB_DT_INTERFACE: {
		struct usb_interface_descriptor *ds = (void *)_ds;
		pr_vdebug("interface descriptor\n");
		if (length != sizeof *ds)
			goto inv_length;

		__entity(INTERFACE, ds->bInterfaceNumber);
		if (ds->iInterface)
			__entity(STRING, ds->iInterface);
		*current_class = ds->bInterfaceClass;
	}
		break;

	case USB_DT_ENDPOINT: {
		struct usb_endpoint_descriptor *ds = (void *)_ds;
		pr_vdebug("endpoint descriptor\n");
		if (length != USB_DT_ENDPOINT_SIZE &&
		    length != USB_DT_ENDPOINT_AUDIO_SIZE)
			goto inv_length;
		__entity(ENDPOINT, ds->bEndpointAddress);
	}
		break;

	case USB_TYPE_CLASS | 0x01:
		if (*current_class == USB_INTERFACE_CLASS_HID) {
			pr_vdebug("hid descriptor\n");
			if (length != sizeof(struct hid_descriptor))
				goto inv_length;
			break;
		} else if (*current_class == USB_INTERFACE_CLASS_CCID) {
			pr_vdebug("ccid descriptor\n");
			if (length != sizeof(struct ccid_descriptor))
				goto inv_length;
			break;
		} else {
			pr_vdebug("unknown descriptor: %d for class %d\n",
			      _ds->bDescriptorType, *current_class);
			return -EINVAL;
		}

	case USB_DT_OTG:
		if (length != sizeof(struct usb_otg_descriptor))
			goto inv_length;
		break;

	case USB_DT_INTERFACE_ASSOCIATION: {
		struct usb_interface_assoc_descriptor *ds = (void *)_ds;
		pr_vdebug("interface association descriptor\n");
		if (length != sizeof *ds)
			goto inv_length;
		if (ds->iFunction)
			__entity(STRING, ds->iFunction);
	}
		break;

	case USB_DT_SS_ENDPOINT_COMP:
		pr_vdebug("EP SS companion descriptor\n");
		if (length != sizeof(struct usb_ss_ep_comp_descriptor))
			goto inv_length;
		break;

	case USB_DT_OTHER_SPEED_CONFIG:
	case USB_DT_INTERFACE_POWER:
	case USB_DT_DEBUG:
	case USB_DT_SECURITY:
	case USB_DT_CS_RADIO_CONTROL:
		 
		pr_vdebug("unimplemented descriptor: %d\n", _ds->bDescriptorType);
		return -EINVAL;

	default:
		 
		pr_vdebug("unknown descriptor: %d\n", _ds->bDescriptorType);
		return -EINVAL;

inv_length:
		pr_vdebug("invalid length: %d (descriptor %d)\n",
			  _ds->bLength, _ds->bDescriptorType);
		return -EINVAL;
	}

#undef __entity
#undef __entity_check_DESCRIPTOR
#undef __entity_check_INTERFACE
#undef __entity_check_STRING
#undef __entity_check_ENDPOINT

	return length;
}

static int __must_check ffs_do_descs(unsigned count, char *data, unsigned len,
				     ffs_entity_callback entity, void *priv)
{
	const unsigned _len = len;
	unsigned long num = 0;
	int current_class = -1;

	for (;;) {
		int ret;

		if (num == count)
			data = NULL;

		 
		ret = entity(FFS_DESCRIPTOR, (u8 *)num, (void *)data, priv);
		if (ret < 0) {
			pr_debug("entity DESCRIPTOR(%02lx); ret = %d\n",
				 num, ret);
			return ret;
		}

		if (!data)
			return _len - len;

		ret = ffs_do_single_desc(data, len, entity, priv,
			&current_class);
		if (ret < 0) {
			pr_debug("%s returns %d\n", __func__, ret);
			return ret;
		}

		len -= ret;
		data += ret;
		++num;
	}
}

static int __ffs_data_do_entity(enum ffs_entity_type type,
				u8 *valuep, struct usb_descriptor_header *desc,
				void *priv)
{
	struct ffs_desc_helper *helper = priv;
	struct usb_endpoint_descriptor *d;

	switch (type) {
	case FFS_DESCRIPTOR:
		break;

	case FFS_INTERFACE:
		 
		if (*valuep >= helper->interfaces_count)
			helper->interfaces_count = *valuep + 1;
		break;

	case FFS_STRING:
		 
		if (*valuep > helper->ffs->strings_count)
			helper->ffs->strings_count = *valuep;
		break;

	case FFS_ENDPOINT:
		d = (void *)desc;
		helper->eps_count++;
		if (helper->eps_count >= FFS_MAX_EPS_COUNT)
			return -EINVAL;
		 
		if (!helper->ffs->eps_count && !helper->ffs->interfaces_count)
			helper->ffs->eps_addrmap[helper->eps_count] =
				d->bEndpointAddress;
		else if (helper->ffs->eps_addrmap[helper->eps_count] !=
				d->bEndpointAddress)
			return -EINVAL;
		break;
	}

	return 0;
}

static int __ffs_do_os_desc_header(enum ffs_os_desc_type *next_type,
				   struct usb_os_desc_header *desc)
{
	u16 bcd_version = le16_to_cpu(desc->bcdVersion);
	u16 w_index = le16_to_cpu(desc->wIndex);

	if (bcd_version == 0x1) {
		pr_warn("bcdVersion must be 0x0100, stored in Little Endian order. "
			"Userspace driver should be fixed, accepting 0x0001 for compatibility.\n");
	} else if (bcd_version != 0x100) {
		pr_vdebug("unsupported os descriptors version: 0x%x\n",
			  bcd_version);
		return -EINVAL;
	}
	switch (w_index) {
	case 0x4:
		*next_type = FFS_OS_DESC_EXT_COMPAT;
		break;
	case 0x5:
		*next_type = FFS_OS_DESC_EXT_PROP;
		break;
	default:
		pr_vdebug("unsupported os descriptor type: %d", w_index);
		return -EINVAL;
	}

	return sizeof(*desc);
}

 
static int __must_check ffs_do_single_os_desc(char *data, unsigned len,
					      enum ffs_os_desc_type type,
					      u16 feature_count,
					      ffs_os_desc_callback entity,
					      void *priv,
					      struct usb_os_desc_header *h)
{
	int ret;
	const unsigned _len = len;

	 
	while (feature_count--) {
		ret = entity(type, h, data, len, priv);
		if (ret < 0) {
			pr_debug("bad OS descriptor, type: %d\n", type);
			return ret;
		}
		data += ret;
		len -= ret;
	}
	return _len - len;
}

 
static int __must_check ffs_do_os_descs(unsigned count,
					char *data, unsigned len,
					ffs_os_desc_callback entity, void *priv)
{
	const unsigned _len = len;
	unsigned long num = 0;

	for (num = 0; num < count; ++num) {
		int ret;
		enum ffs_os_desc_type type;
		u16 feature_count;
		struct usb_os_desc_header *desc = (void *)data;

		if (len < sizeof(*desc))
			return -EINVAL;

		 
		if (le32_to_cpu(desc->dwLength) > len)
			return -EINVAL;

		ret = __ffs_do_os_desc_header(&type, desc);
		if (ret < 0) {
			pr_debug("entity OS_DESCRIPTOR(%02lx); ret = %d\n",
				 num, ret);
			return ret;
		}
		 
		feature_count = le16_to_cpu(desc->wCount);
		if (type == FFS_OS_DESC_EXT_COMPAT &&
		    (feature_count > 255 || desc->Reserved))
				return -EINVAL;
		len -= ret;
		data += ret;

		 
		ret = ffs_do_single_os_desc(data, len, type,
					    feature_count, entity, priv, desc);
		if (ret < 0) {
			pr_debug("%s returns %d\n", __func__, ret);
			return ret;
		}

		len -= ret;
		data += ret;
	}
	return _len - len;
}

 
static int __ffs_data_do_os_desc(enum ffs_os_desc_type type,
				 struct usb_os_desc_header *h, void *data,
				 unsigned len, void *priv)
{
	struct ffs_data *ffs = priv;
	u8 length;

	switch (type) {
	case FFS_OS_DESC_EXT_COMPAT: {
		struct usb_ext_compat_desc *d = data;
		int i;

		if (len < sizeof(*d) ||
		    d->bFirstInterfaceNumber >= ffs->interfaces_count)
			return -EINVAL;
		if (d->Reserved1 != 1) {
			 
			pr_debug("usb_ext_compat_desc::Reserved1 forced to 1\n");
			d->Reserved1 = 1;
		}
		for (i = 0; i < ARRAY_SIZE(d->Reserved2); ++i)
			if (d->Reserved2[i])
				return -EINVAL;

		length = sizeof(struct usb_ext_compat_desc);
	}
		break;
	case FFS_OS_DESC_EXT_PROP: {
		struct usb_ext_prop_desc *d = data;
		u32 type, pdl;
		u16 pnl;

		if (len < sizeof(*d) || h->interface >= ffs->interfaces_count)
			return -EINVAL;
		length = le32_to_cpu(d->dwSize);
		if (len < length)
			return -EINVAL;
		type = le32_to_cpu(d->dwPropertyDataType);
		if (type < USB_EXT_PROP_UNICODE ||
		    type > USB_EXT_PROP_UNICODE_MULTI) {
			pr_vdebug("unsupported os descriptor property type: %d",
				  type);
			return -EINVAL;
		}
		pnl = le16_to_cpu(d->wPropertyNameLength);
		if (length < 14 + pnl) {
			pr_vdebug("invalid os descriptor length: %d pnl:%d (descriptor %d)\n",
				  length, pnl, type);
			return -EINVAL;
		}
		pdl = le32_to_cpu(*(__le32 *)((u8 *)data + 10 + pnl));
		if (length != 14 + pnl + pdl) {
			pr_vdebug("invalid os descriptor length: %d pnl:%d pdl:%d (descriptor %d)\n",
				  length, pnl, pdl, type);
			return -EINVAL;
		}
		++ffs->ms_os_descs_ext_prop_count;
		 
		ffs->ms_os_descs_ext_prop_name_len += pnl * 2;
		ffs->ms_os_descs_ext_prop_data_len += pdl;
	}
		break;
	default:
		pr_vdebug("unknown descriptor: %d\n", type);
		return -EINVAL;
	}
	return length;
}

static int __ffs_data_got_descs(struct ffs_data *ffs,
				char *const _data, size_t len)
{
	char *data = _data, *raw_descs;
	unsigned os_descs_count = 0, counts[3], flags;
	int ret = -EINVAL, i;
	struct ffs_desc_helper helper;

	if (get_unaligned_le32(data + 4) != len)
		goto error;

	switch (get_unaligned_le32(data)) {
	case FUNCTIONFS_DESCRIPTORS_MAGIC:
		flags = FUNCTIONFS_HAS_FS_DESC | FUNCTIONFS_HAS_HS_DESC;
		data += 8;
		len  -= 8;
		break;
	case FUNCTIONFS_DESCRIPTORS_MAGIC_V2:
		flags = get_unaligned_le32(data + 8);
		ffs->user_flags = flags;
		if (flags & ~(FUNCTIONFS_HAS_FS_DESC |
			      FUNCTIONFS_HAS_HS_DESC |
			      FUNCTIONFS_HAS_SS_DESC |
			      FUNCTIONFS_HAS_MS_OS_DESC |
			      FUNCTIONFS_VIRTUAL_ADDR |
			      FUNCTIONFS_EVENTFD |
			      FUNCTIONFS_ALL_CTRL_RECIP |
			      FUNCTIONFS_CONFIG0_SETUP)) {
			ret = -ENOSYS;
			goto error;
		}
		data += 12;
		len  -= 12;
		break;
	default:
		goto error;
	}

	if (flags & FUNCTIONFS_EVENTFD) {
		if (len < 4)
			goto error;
		ffs->ffs_eventfd =
			eventfd_ctx_fdget((int)get_unaligned_le32(data));
		if (IS_ERR(ffs->ffs_eventfd)) {
			ret = PTR_ERR(ffs->ffs_eventfd);
			ffs->ffs_eventfd = NULL;
			goto error;
		}
		data += 4;
		len  -= 4;
	}

	 
	for (i = 0; i < 3; ++i) {
		if (!(flags & (1 << i))) {
			counts[i] = 0;
		} else if (len < 4) {
			goto error;
		} else {
			counts[i] = get_unaligned_le32(data);
			data += 4;
			len  -= 4;
		}
	}
	if (flags & (1 << i)) {
		if (len < 4) {
			goto error;
		}
		os_descs_count = get_unaligned_le32(data);
		data += 4;
		len -= 4;
	}

	 
	raw_descs = data;
	helper.ffs = ffs;
	for (i = 0; i < 3; ++i) {
		if (!counts[i])
			continue;
		helper.interfaces_count = 0;
		helper.eps_count = 0;
		ret = ffs_do_descs(counts[i], data, len,
				   __ffs_data_do_entity, &helper);
		if (ret < 0)
			goto error;
		if (!ffs->eps_count && !ffs->interfaces_count) {
			ffs->eps_count = helper.eps_count;
			ffs->interfaces_count = helper.interfaces_count;
		} else {
			if (ffs->eps_count != helper.eps_count) {
				ret = -EINVAL;
				goto error;
			}
			if (ffs->interfaces_count != helper.interfaces_count) {
				ret = -EINVAL;
				goto error;
			}
		}
		data += ret;
		len  -= ret;
	}
	if (os_descs_count) {
		ret = ffs_do_os_descs(os_descs_count, data, len,
				      __ffs_data_do_os_desc, ffs);
		if (ret < 0)
			goto error;
		data += ret;
		len -= ret;
	}

	if (raw_descs == data || len) {
		ret = -EINVAL;
		goto error;
	}

	ffs->raw_descs_data	= _data;
	ffs->raw_descs		= raw_descs;
	ffs->raw_descs_length	= data - raw_descs;
	ffs->fs_descs_count	= counts[0];
	ffs->hs_descs_count	= counts[1];
	ffs->ss_descs_count	= counts[2];
	ffs->ms_os_descs_count	= os_descs_count;

	return 0;

error:
	kfree(_data);
	return ret;
}

static int __ffs_data_got_strings(struct ffs_data *ffs,
				  char *const _data, size_t len)
{
	u32 str_count, needed_count, lang_count;
	struct usb_gadget_strings **stringtabs, *t;
	const char *data = _data;
	struct usb_string *s;

	if (len < 16 ||
	    get_unaligned_le32(data) != FUNCTIONFS_STRINGS_MAGIC ||
	    get_unaligned_le32(data + 4) != len)
		goto error;
	str_count  = get_unaligned_le32(data + 8);
	lang_count = get_unaligned_le32(data + 12);

	 
	if (!str_count != !lang_count)
		goto error;

	 
	needed_count = ffs->strings_count;
	if (str_count < needed_count)
		goto error;

	 
	if (!needed_count) {
		kfree(_data);
		return 0;
	}

	 
	{
		unsigned i = 0;
		vla_group(d);
		vla_item(d, struct usb_gadget_strings *, stringtabs,
			size_add(lang_count, 1));
		vla_item(d, struct usb_gadget_strings, stringtab, lang_count);
		vla_item(d, struct usb_string, strings,
			size_mul(lang_count, (needed_count + 1)));

		char *vlabuf = kmalloc(vla_group_size(d), GFP_KERNEL);

		if (!vlabuf) {
			kfree(_data);
			return -ENOMEM;
		}

		 
		stringtabs = vla_ptr(vlabuf, d, stringtabs);
		t = vla_ptr(vlabuf, d, stringtab);
		i = lang_count;
		do {
			*stringtabs++ = t++;
		} while (--i);
		*stringtabs = NULL;

		 
		stringtabs = vla_ptr(vlabuf, d, stringtabs);
		t = vla_ptr(vlabuf, d, stringtab);
		s = vla_ptr(vlabuf, d, strings);
	}

	 
	data += 16;
	len -= 16;

	do {  
		unsigned needed = needed_count;
		u32 str_per_lang = str_count;

		if (len < 3)
			goto error_free;
		t->language = get_unaligned_le16(data);
		t->strings  = s;
		++t;

		data += 2;
		len -= 2;

		 
		do {  
			size_t length = strnlen(data, len);

			if (length == len)
				goto error_free;

			 
			if (needed) {
				 
				s->s = data;
				--needed;
				++s;
			}

			data += length + 1;
			len -= length + 1;
		} while (--str_per_lang);

		s->id = 0;    
		s->s = NULL;
		++s;

	} while (--lang_count);

	 
	if (len)
		goto error_free;

	 
	ffs->stringtabs = stringtabs;
	ffs->raw_strings = _data;

	return 0;

error_free:
	kfree(stringtabs);
error:
	kfree(_data);
	return -EINVAL;
}


 

static void __ffs_event_add(struct ffs_data *ffs,
			    enum usb_functionfs_event_type type)
{
	enum usb_functionfs_event_type rem_type1, rem_type2 = type;
	int neg = 0;

	 
	if (ffs->setup_state == FFS_SETUP_PENDING)
		ffs->setup_state = FFS_SETUP_CANCELLED;

	 
	switch (type) {
	case FUNCTIONFS_RESUME:
		rem_type2 = FUNCTIONFS_SUSPEND;
		fallthrough;
	case FUNCTIONFS_SUSPEND:
	case FUNCTIONFS_SETUP:
		rem_type1 = type;
		 
		break;

	case FUNCTIONFS_BIND:
	case FUNCTIONFS_UNBIND:
	case FUNCTIONFS_DISABLE:
	case FUNCTIONFS_ENABLE:
		 
		rem_type1 = FUNCTIONFS_SUSPEND;
		rem_type2 = FUNCTIONFS_RESUME;
		neg = 1;
		break;

	default:
		WARN(1, "%d: unknown event, this should not happen\n", type);
		return;
	}

	{
		u8 *ev  = ffs->ev.types, *out = ev;
		unsigned n = ffs->ev.count;
		for (; n; --n, ++ev)
			if ((*ev == rem_type1 || *ev == rem_type2) == neg)
				*out++ = *ev;
			else
				pr_vdebug("purging event %d\n", *ev);
		ffs->ev.count = out - ffs->ev.types;
	}

	pr_vdebug("adding event %d\n", type);
	ffs->ev.types[ffs->ev.count++] = type;
	wake_up_locked(&ffs->ev.waitq);
	if (ffs->ffs_eventfd)
		eventfd_signal(ffs->ffs_eventfd, 1);
}

static void ffs_event_add(struct ffs_data *ffs,
			  enum usb_functionfs_event_type type)
{
	unsigned long flags;
	spin_lock_irqsave(&ffs->ev.waitq.lock, flags);
	__ffs_event_add(ffs, type);
	spin_unlock_irqrestore(&ffs->ev.waitq.lock, flags);
}

 

static int ffs_ep_addr2idx(struct ffs_data *ffs, u8 endpoint_address)
{
	int i;

	for (i = 1; i < ARRAY_SIZE(ffs->eps_addrmap); ++i)
		if (ffs->eps_addrmap[i] == endpoint_address)
			return i;
	return -ENOENT;
}

static int __ffs_func_bind_do_descs(enum ffs_entity_type type, u8 *valuep,
				    struct usb_descriptor_header *desc,
				    void *priv)
{
	struct usb_endpoint_descriptor *ds = (void *)desc;
	struct ffs_function *func = priv;
	struct ffs_ep *ffs_ep;
	unsigned ep_desc_id;
	int idx;
	static const char *speed_names[] = { "full", "high", "super" };

	if (type != FFS_DESCRIPTOR)
		return 0;

	 
	if (func->function.ss_descriptors) {
		ep_desc_id = 2;
		func->function.ss_descriptors[(long)valuep] = desc;
	} else if (func->function.hs_descriptors) {
		ep_desc_id = 1;
		func->function.hs_descriptors[(long)valuep] = desc;
	} else {
		ep_desc_id = 0;
		func->function.fs_descriptors[(long)valuep]    = desc;
	}

	if (!desc || desc->bDescriptorType != USB_DT_ENDPOINT)
		return 0;

	idx = ffs_ep_addr2idx(func->ffs, ds->bEndpointAddress) - 1;
	if (idx < 0)
		return idx;

	ffs_ep = func->eps + idx;

	if (ffs_ep->descs[ep_desc_id]) {
		pr_err("two %sspeed descriptors for EP %d\n",
			  speed_names[ep_desc_id],
			  ds->bEndpointAddress & USB_ENDPOINT_NUMBER_MASK);
		return -EINVAL;
	}
	ffs_ep->descs[ep_desc_id] = ds;

	ffs_dump_mem(": Original  ep desc", ds, ds->bLength);
	if (ffs_ep->ep) {
		ds->bEndpointAddress = ffs_ep->descs[0]->bEndpointAddress;
		if (!ds->wMaxPacketSize)
			ds->wMaxPacketSize = ffs_ep->descs[0]->wMaxPacketSize;
	} else {
		struct usb_request *req;
		struct usb_ep *ep;
		u8 bEndpointAddress;
		u16 wMaxPacketSize;

		 
		bEndpointAddress = ds->bEndpointAddress;
		 
		wMaxPacketSize = ds->wMaxPacketSize;
		pr_vdebug("autoconfig\n");
		ep = usb_ep_autoconfig(func->gadget, ds);
		if (!ep)
			return -ENOTSUPP;
		ep->driver_data = func->eps + idx;

		req = usb_ep_alloc_request(ep, GFP_KERNEL);
		if (!req)
			return -ENOMEM;

		ffs_ep->ep  = ep;
		ffs_ep->req = req;
		func->eps_revmap[ds->bEndpointAddress &
				 USB_ENDPOINT_NUMBER_MASK] = idx + 1;
		 
		if (func->ffs->user_flags & FUNCTIONFS_VIRTUAL_ADDR)
			ds->bEndpointAddress = bEndpointAddress;
		 
		ds->wMaxPacketSize = wMaxPacketSize;
	}
	ffs_dump_mem(": Rewritten ep desc", ds, ds->bLength);

	return 0;
}

static int __ffs_func_bind_do_nums(enum ffs_entity_type type, u8 *valuep,
				   struct usb_descriptor_header *desc,
				   void *priv)
{
	struct ffs_function *func = priv;
	unsigned idx;
	u8 newValue;

	switch (type) {
	default:
	case FFS_DESCRIPTOR:
		 
		return 0;

	case FFS_INTERFACE:
		idx = *valuep;
		if (func->interfaces_nums[idx] < 0) {
			int id = usb_interface_id(func->conf, &func->function);
			if (id < 0)
				return id;
			func->interfaces_nums[idx] = id;
		}
		newValue = func->interfaces_nums[idx];
		break;

	case FFS_STRING:
		 
		newValue = func->ffs->stringtabs[0]->strings[*valuep - 1].id;
		break;

	case FFS_ENDPOINT:
		 
		if (desc->bDescriptorType == USB_DT_ENDPOINT)
			return 0;

		idx = (*valuep & USB_ENDPOINT_NUMBER_MASK) - 1;
		if (!func->eps[idx].ep)
			return -EINVAL;

		{
			struct usb_endpoint_descriptor **descs;
			descs = func->eps[idx].descs;
			newValue = descs[descs[0] ? 0 : 1]->bEndpointAddress;
		}
		break;
	}

	pr_vdebug("%02x -> %02x\n", *valuep, newValue);
	*valuep = newValue;
	return 0;
}

static int __ffs_func_bind_do_os_desc(enum ffs_os_desc_type type,
				      struct usb_os_desc_header *h, void *data,
				      unsigned len, void *priv)
{
	struct ffs_function *func = priv;
	u8 length = 0;

	switch (type) {
	case FFS_OS_DESC_EXT_COMPAT: {
		struct usb_ext_compat_desc *desc = data;
		struct usb_os_desc_table *t;

		t = &func->function.os_desc_table[desc->bFirstInterfaceNumber];
		t->if_id = func->interfaces_nums[desc->bFirstInterfaceNumber];
		memcpy(t->os_desc->ext_compat_id, &desc->CompatibleID,
		       ARRAY_SIZE(desc->CompatibleID) +
		       ARRAY_SIZE(desc->SubCompatibleID));
		length = sizeof(*desc);
	}
		break;
	case FFS_OS_DESC_EXT_PROP: {
		struct usb_ext_prop_desc *desc = data;
		struct usb_os_desc_table *t;
		struct usb_os_desc_ext_prop *ext_prop;
		char *ext_prop_name;
		char *ext_prop_data;

		t = &func->function.os_desc_table[h->interface];
		t->if_id = func->interfaces_nums[h->interface];

		ext_prop = func->ffs->ms_os_descs_ext_prop_avail;
		func->ffs->ms_os_descs_ext_prop_avail += sizeof(*ext_prop);

		ext_prop->type = le32_to_cpu(desc->dwPropertyDataType);
		ext_prop->name_len = le16_to_cpu(desc->wPropertyNameLength);
		ext_prop->data_len = le32_to_cpu(*(__le32 *)
			usb_ext_prop_data_len_ptr(data, ext_prop->name_len));
		length = ext_prop->name_len + ext_prop->data_len + 14;

		ext_prop_name = func->ffs->ms_os_descs_ext_prop_name_avail;
		func->ffs->ms_os_descs_ext_prop_name_avail +=
			ext_prop->name_len;

		ext_prop_data = func->ffs->ms_os_descs_ext_prop_data_avail;
		func->ffs->ms_os_descs_ext_prop_data_avail +=
			ext_prop->data_len;
		memcpy(ext_prop_data,
		       usb_ext_prop_data_ptr(data, ext_prop->name_len),
		       ext_prop->data_len);
		 
		switch (ext_prop->type) {
		case USB_EXT_PROP_UNICODE:
		case USB_EXT_PROP_UNICODE_ENV:
		case USB_EXT_PROP_UNICODE_LINK:
		case USB_EXT_PROP_UNICODE_MULTI:
			ext_prop->data_len *= 2;
			break;
		}
		ext_prop->data = ext_prop_data;

		memcpy(ext_prop_name, usb_ext_prop_name_ptr(data),
		       ext_prop->name_len);
		 
		ext_prop->name_len *= 2;
		ext_prop->name = ext_prop_name;

		t->os_desc->ext_prop_len +=
			ext_prop->name_len + ext_prop->data_len + 14;
		++t->os_desc->ext_prop_count;
		list_add_tail(&ext_prop->entry, &t->os_desc->ext_prop);
	}
		break;
	default:
		pr_vdebug("unknown descriptor: %d\n", type);
	}

	return length;
}

static inline struct f_fs_opts *ffs_do_functionfs_bind(struct usb_function *f,
						struct usb_configuration *c)
{
	struct ffs_function *func = ffs_func_from_usb(f);
	struct f_fs_opts *ffs_opts =
		container_of(f->fi, struct f_fs_opts, func_inst);
	struct ffs_data *ffs_data;
	int ret;

	 
	if (!ffs_opts->no_configfs)
		ffs_dev_lock();
	ret = ffs_opts->dev->desc_ready ? 0 : -ENODEV;
	ffs_data = ffs_opts->dev->ffs_data;
	if (!ffs_opts->no_configfs)
		ffs_dev_unlock();
	if (ret)
		return ERR_PTR(ret);

	func->ffs = ffs_data;
	func->conf = c;
	func->gadget = c->cdev->gadget;

	 
	if (!ffs_opts->refcnt) {
		ret = functionfs_bind(func->ffs, c->cdev);
		if (ret)
			return ERR_PTR(ret);
	}
	ffs_opts->refcnt++;
	func->function.strings = func->ffs->stringtabs;

	return ffs_opts;
}

static int _ffs_func_bind(struct usb_configuration *c,
			  struct usb_function *f)
{
	struct ffs_function *func = ffs_func_from_usb(f);
	struct ffs_data *ffs = func->ffs;

	const int full = !!func->ffs->fs_descs_count;
	const int high = !!func->ffs->hs_descs_count;
	const int super = !!func->ffs->ss_descs_count;

	int fs_len, hs_len, ss_len, ret, i;
	struct ffs_ep *eps_ptr;

	 
	vla_group(d);
	vla_item_with_sz(d, struct ffs_ep, eps, ffs->eps_count);
	vla_item_with_sz(d, struct usb_descriptor_header *, fs_descs,
		full ? ffs->fs_descs_count + 1 : 0);
	vla_item_with_sz(d, struct usb_descriptor_header *, hs_descs,
		high ? ffs->hs_descs_count + 1 : 0);
	vla_item_with_sz(d, struct usb_descriptor_header *, ss_descs,
		super ? ffs->ss_descs_count + 1 : 0);
	vla_item_with_sz(d, short, inums, ffs->interfaces_count);
	vla_item_with_sz(d, struct usb_os_desc_table, os_desc_table,
			 c->cdev->use_os_string ? ffs->interfaces_count : 0);
	vla_item_with_sz(d, char[16], ext_compat,
			 c->cdev->use_os_string ? ffs->interfaces_count : 0);
	vla_item_with_sz(d, struct usb_os_desc, os_desc,
			 c->cdev->use_os_string ? ffs->interfaces_count : 0);
	vla_item_with_sz(d, struct usb_os_desc_ext_prop, ext_prop,
			 ffs->ms_os_descs_ext_prop_count);
	vla_item_with_sz(d, char, ext_prop_name,
			 ffs->ms_os_descs_ext_prop_name_len);
	vla_item_with_sz(d, char, ext_prop_data,
			 ffs->ms_os_descs_ext_prop_data_len);
	vla_item_with_sz(d, char, raw_descs, ffs->raw_descs_length);
	char *vlabuf;

	 
	if (!(full | high | super))
		return -ENOTSUPP;

	 
	vlabuf = kzalloc(vla_group_size(d), GFP_KERNEL);
	if (!vlabuf)
		return -ENOMEM;

	ffs->ms_os_descs_ext_prop_avail = vla_ptr(vlabuf, d, ext_prop);
	ffs->ms_os_descs_ext_prop_name_avail =
		vla_ptr(vlabuf, d, ext_prop_name);
	ffs->ms_os_descs_ext_prop_data_avail =
		vla_ptr(vlabuf, d, ext_prop_data);

	 
	memcpy(vla_ptr(vlabuf, d, raw_descs), ffs->raw_descs,
	       ffs->raw_descs_length);

	memset(vla_ptr(vlabuf, d, inums), 0xff, d_inums__sz);
	eps_ptr = vla_ptr(vlabuf, d, eps);
	for (i = 0; i < ffs->eps_count; i++)
		eps_ptr[i].num = -1;

	 
	func->eps             = vla_ptr(vlabuf, d, eps);
	func->interfaces_nums = vla_ptr(vlabuf, d, inums);

	 
	if (full) {
		func->function.fs_descriptors = vla_ptr(vlabuf, d, fs_descs);
		fs_len = ffs_do_descs(ffs->fs_descs_count,
				      vla_ptr(vlabuf, d, raw_descs),
				      d_raw_descs__sz,
				      __ffs_func_bind_do_descs, func);
		if (fs_len < 0) {
			ret = fs_len;
			goto error;
		}
	} else {
		fs_len = 0;
	}

	if (high) {
		func->function.hs_descriptors = vla_ptr(vlabuf, d, hs_descs);
		hs_len = ffs_do_descs(ffs->hs_descs_count,
				      vla_ptr(vlabuf, d, raw_descs) + fs_len,
				      d_raw_descs__sz - fs_len,
				      __ffs_func_bind_do_descs, func);
		if (hs_len < 0) {
			ret = hs_len;
			goto error;
		}
	} else {
		hs_len = 0;
	}

	if (super) {
		func->function.ss_descriptors = func->function.ssp_descriptors =
			vla_ptr(vlabuf, d, ss_descs);
		ss_len = ffs_do_descs(ffs->ss_descs_count,
				vla_ptr(vlabuf, d, raw_descs) + fs_len + hs_len,
				d_raw_descs__sz - fs_len - hs_len,
				__ffs_func_bind_do_descs, func);
		if (ss_len < 0) {
			ret = ss_len;
			goto error;
		}
	} else {
		ss_len = 0;
	}

	 
	ret = ffs_do_descs(ffs->fs_descs_count +
			   (high ? ffs->hs_descs_count : 0) +
			   (super ? ffs->ss_descs_count : 0),
			   vla_ptr(vlabuf, d, raw_descs), d_raw_descs__sz,
			   __ffs_func_bind_do_nums, func);
	if (ret < 0)
		goto error;

	func->function.os_desc_table = vla_ptr(vlabuf, d, os_desc_table);
	if (c->cdev->use_os_string) {
		for (i = 0; i < ffs->interfaces_count; ++i) {
			struct usb_os_desc *desc;

			desc = func->function.os_desc_table[i].os_desc =
				vla_ptr(vlabuf, d, os_desc) +
				i * sizeof(struct usb_os_desc);
			desc->ext_compat_id =
				vla_ptr(vlabuf, d, ext_compat) + i * 16;
			INIT_LIST_HEAD(&desc->ext_prop);
		}
		ret = ffs_do_os_descs(ffs->ms_os_descs_count,
				      vla_ptr(vlabuf, d, raw_descs) +
				      fs_len + hs_len + ss_len,
				      d_raw_descs__sz - fs_len - hs_len -
				      ss_len,
				      __ffs_func_bind_do_os_desc, func);
		if (ret < 0)
			goto error;
	}
	func->function.os_desc_n =
		c->cdev->use_os_string ? ffs->interfaces_count : 0;

	 
	ffs_event_add(ffs, FUNCTIONFS_BIND);
	return 0;

error:
	 
	return ret;
}

static int ffs_func_bind(struct usb_configuration *c,
			 struct usb_function *f)
{
	struct f_fs_opts *ffs_opts = ffs_do_functionfs_bind(f, c);
	struct ffs_function *func = ffs_func_from_usb(f);
	int ret;

	if (IS_ERR(ffs_opts))
		return PTR_ERR(ffs_opts);

	ret = _ffs_func_bind(c, f);
	if (ret && !--ffs_opts->refcnt)
		functionfs_unbind(func->ffs);

	return ret;
}


 

static void ffs_reset_work(struct work_struct *work)
{
	struct ffs_data *ffs = container_of(work,
		struct ffs_data, reset_work);
	ffs_data_reset(ffs);
}

static int ffs_func_set_alt(struct usb_function *f,
			    unsigned interface, unsigned alt)
{
	struct ffs_function *func = ffs_func_from_usb(f);
	struct ffs_data *ffs = func->ffs;
	int ret = 0, intf;

	if (alt != (unsigned)-1) {
		intf = ffs_func_revmap_intf(func, interface);
		if (intf < 0)
			return intf;
	}

	if (ffs->func)
		ffs_func_eps_disable(ffs->func);

	if (ffs->state == FFS_DEACTIVATED) {
		ffs->state = FFS_CLOSING;
		INIT_WORK(&ffs->reset_work, ffs_reset_work);
		schedule_work(&ffs->reset_work);
		return -ENODEV;
	}

	if (ffs->state != FFS_ACTIVE)
		return -ENODEV;

	if (alt == (unsigned)-1) {
		ffs->func = NULL;
		ffs_event_add(ffs, FUNCTIONFS_DISABLE);
		return 0;
	}

	ffs->func = func;
	ret = ffs_func_eps_enable(func);
	if (ret >= 0)
		ffs_event_add(ffs, FUNCTIONFS_ENABLE);
	return ret;
}

static void ffs_func_disable(struct usb_function *f)
{
	ffs_func_set_alt(f, 0, (unsigned)-1);
}

static int ffs_func_setup(struct usb_function *f,
			  const struct usb_ctrlrequest *creq)
{
	struct ffs_function *func = ffs_func_from_usb(f);
	struct ffs_data *ffs = func->ffs;
	unsigned long flags;
	int ret;

	pr_vdebug("creq->bRequestType = %02x\n", creq->bRequestType);
	pr_vdebug("creq->bRequest     = %02x\n", creq->bRequest);
	pr_vdebug("creq->wValue       = %04x\n", le16_to_cpu(creq->wValue));
	pr_vdebug("creq->wIndex       = %04x\n", le16_to_cpu(creq->wIndex));
	pr_vdebug("creq->wLength      = %04x\n", le16_to_cpu(creq->wLength));

	 
	if (ffs->state != FFS_ACTIVE)
		return -ENODEV;

	switch (creq->bRequestType & USB_RECIP_MASK) {
	case USB_RECIP_INTERFACE:
		ret = ffs_func_revmap_intf(func, le16_to_cpu(creq->wIndex));
		if (ret < 0)
			return ret;
		break;

	case USB_RECIP_ENDPOINT:
		ret = ffs_func_revmap_ep(func, le16_to_cpu(creq->wIndex));
		if (ret < 0)
			return ret;
		if (func->ffs->user_flags & FUNCTIONFS_VIRTUAL_ADDR)
			ret = func->ffs->eps_addrmap[ret];
		break;

	default:
		if (func->ffs->user_flags & FUNCTIONFS_ALL_CTRL_RECIP)
			ret = le16_to_cpu(creq->wIndex);
		else
			return -EOPNOTSUPP;
	}

	spin_lock_irqsave(&ffs->ev.waitq.lock, flags);
	ffs->ev.setup = *creq;
	ffs->ev.setup.wIndex = cpu_to_le16(ret);
	__ffs_event_add(ffs, FUNCTIONFS_SETUP);
	spin_unlock_irqrestore(&ffs->ev.waitq.lock, flags);

	return creq->wLength == 0 ? USB_GADGET_DELAYED_STATUS : 0;
}

static bool ffs_func_req_match(struct usb_function *f,
			       const struct usb_ctrlrequest *creq,
			       bool config0)
{
	struct ffs_function *func = ffs_func_from_usb(f);

	if (config0 && !(func->ffs->user_flags & FUNCTIONFS_CONFIG0_SETUP))
		return false;

	switch (creq->bRequestType & USB_RECIP_MASK) {
	case USB_RECIP_INTERFACE:
		return (ffs_func_revmap_intf(func,
					     le16_to_cpu(creq->wIndex)) >= 0);
	case USB_RECIP_ENDPOINT:
		return (ffs_func_revmap_ep(func,
					   le16_to_cpu(creq->wIndex)) >= 0);
	default:
		return (bool) (func->ffs->user_flags &
			       FUNCTIONFS_ALL_CTRL_RECIP);
	}
}

static void ffs_func_suspend(struct usb_function *f)
{
	ffs_event_add(ffs_func_from_usb(f)->ffs, FUNCTIONFS_SUSPEND);
}

static void ffs_func_resume(struct usb_function *f)
{
	ffs_event_add(ffs_func_from_usb(f)->ffs, FUNCTIONFS_RESUME);
}


 

static int ffs_func_revmap_ep(struct ffs_function *func, u8 num)
{
	num = func->eps_revmap[num & USB_ENDPOINT_NUMBER_MASK];
	return num ? num : -EDOM;
}

static int ffs_func_revmap_intf(struct ffs_function *func, u8 intf)
{
	short *nums = func->interfaces_nums;
	unsigned count = func->ffs->interfaces_count;

	for (; count; --count, ++nums) {
		if (*nums >= 0 && *nums == intf)
			return nums - func->interfaces_nums;
	}

	return -EDOM;
}


 

static LIST_HEAD(ffs_devices);

static struct ffs_dev *_ffs_do_find_dev(const char *name)
{
	struct ffs_dev *dev;

	if (!name)
		return NULL;

	list_for_each_entry(dev, &ffs_devices, entry) {
		if (strcmp(dev->name, name) == 0)
			return dev;
	}

	return NULL;
}

 
static struct ffs_dev *_ffs_get_single_dev(void)
{
	struct ffs_dev *dev;

	if (list_is_singular(&ffs_devices)) {
		dev = list_first_entry(&ffs_devices, struct ffs_dev, entry);
		if (dev->single)
			return dev;
	}

	return NULL;
}

 
static struct ffs_dev *_ffs_find_dev(const char *name)
{
	struct ffs_dev *dev;

	dev = _ffs_get_single_dev();
	if (dev)
		return dev;

	return _ffs_do_find_dev(name);
}

 

static inline struct f_fs_opts *to_ffs_opts(struct config_item *item)
{
	return container_of(to_config_group(item), struct f_fs_opts,
			    func_inst.group);
}

static void ffs_attr_release(struct config_item *item)
{
	struct f_fs_opts *opts = to_ffs_opts(item);

	usb_put_function_instance(&opts->func_inst);
}

static struct configfs_item_operations ffs_item_ops = {
	.release	= ffs_attr_release,
};

static const struct config_item_type ffs_func_type = {
	.ct_item_ops	= &ffs_item_ops,
	.ct_owner	= THIS_MODULE,
};


 

static void ffs_free_inst(struct usb_function_instance *f)
{
	struct f_fs_opts *opts;

	opts = to_f_fs_opts(f);
	ffs_release_dev(opts->dev);
	ffs_dev_lock();
	_ffs_free_dev(opts->dev);
	ffs_dev_unlock();
	kfree(opts);
}

static int ffs_set_inst_name(struct usb_function_instance *fi, const char *name)
{
	if (strlen(name) >= sizeof_field(struct ffs_dev, name))
		return -ENAMETOOLONG;
	return ffs_name_dev(to_f_fs_opts(fi)->dev, name);
}

static struct usb_function_instance *ffs_alloc_inst(void)
{
	struct f_fs_opts *opts;
	struct ffs_dev *dev;

	opts = kzalloc(sizeof(*opts), GFP_KERNEL);
	if (!opts)
		return ERR_PTR(-ENOMEM);

	opts->func_inst.set_inst_name = ffs_set_inst_name;
	opts->func_inst.free_func_inst = ffs_free_inst;
	ffs_dev_lock();
	dev = _ffs_alloc_dev();
	ffs_dev_unlock();
	if (IS_ERR(dev)) {
		kfree(opts);
		return ERR_CAST(dev);
	}
	opts->dev = dev;
	dev->opts = opts;

	config_group_init_type_name(&opts->func_inst.group, "",
				    &ffs_func_type);
	return &opts->func_inst;
}

static void ffs_free(struct usb_function *f)
{
	kfree(ffs_func_from_usb(f));
}

static void ffs_func_unbind(struct usb_configuration *c,
			    struct usb_function *f)
{
	struct ffs_function *func = ffs_func_from_usb(f);
	struct ffs_data *ffs = func->ffs;
	struct f_fs_opts *opts =
		container_of(f->fi, struct f_fs_opts, func_inst);
	struct ffs_ep *ep = func->eps;
	unsigned count = ffs->eps_count;
	unsigned long flags;

	if (ffs->func == func) {
		ffs_func_eps_disable(func);
		ffs->func = NULL;
	}

	 
	drain_workqueue(ffs->io_completion_wq);

	ffs_event_add(ffs, FUNCTIONFS_UNBIND);
	if (!--opts->refcnt)
		functionfs_unbind(ffs);

	 
	spin_lock_irqsave(&func->ffs->eps_lock, flags);
	while (count--) {
		if (ep->ep && ep->req)
			usb_ep_free_request(ep->ep, ep->req);
		ep->req = NULL;
		++ep;
	}
	spin_unlock_irqrestore(&func->ffs->eps_lock, flags);
	kfree(func->eps);
	func->eps = NULL;
	 
	func->function.fs_descriptors = NULL;
	func->function.hs_descriptors = NULL;
	func->function.ss_descriptors = NULL;
	func->function.ssp_descriptors = NULL;
	func->interfaces_nums = NULL;

}

static struct usb_function *ffs_alloc(struct usb_function_instance *fi)
{
	struct ffs_function *func;

	func = kzalloc(sizeof(*func), GFP_KERNEL);
	if (!func)
		return ERR_PTR(-ENOMEM);

	func->function.name    = "Function FS Gadget";

	func->function.bind    = ffs_func_bind;
	func->function.unbind  = ffs_func_unbind;
	func->function.set_alt = ffs_func_set_alt;
	func->function.disable = ffs_func_disable;
	func->function.setup   = ffs_func_setup;
	func->function.req_match = ffs_func_req_match;
	func->function.suspend = ffs_func_suspend;
	func->function.resume  = ffs_func_resume;
	func->function.free_func = ffs_free;

	return &func->function;
}

 
static struct ffs_dev *_ffs_alloc_dev(void)
{
	struct ffs_dev *dev;
	int ret;

	if (_ffs_get_single_dev())
			return ERR_PTR(-EBUSY);

	dev = kzalloc(sizeof(*dev), GFP_KERNEL);
	if (!dev)
		return ERR_PTR(-ENOMEM);

	if (list_empty(&ffs_devices)) {
		ret = functionfs_init();
		if (ret) {
			kfree(dev);
			return ERR_PTR(ret);
		}
	}

	list_add(&dev->entry, &ffs_devices);

	return dev;
}

int ffs_name_dev(struct ffs_dev *dev, const char *name)
{
	struct ffs_dev *existing;
	int ret = 0;

	ffs_dev_lock();

	existing = _ffs_do_find_dev(name);
	if (!existing)
		strscpy(dev->name, name, ARRAY_SIZE(dev->name));
	else if (existing != dev)
		ret = -EBUSY;

	ffs_dev_unlock();

	return ret;
}
EXPORT_SYMBOL_GPL(ffs_name_dev);

int ffs_single_dev(struct ffs_dev *dev)
{
	int ret;

	ret = 0;
	ffs_dev_lock();

	if (!list_is_singular(&ffs_devices))
		ret = -EBUSY;
	else
		dev->single = true;

	ffs_dev_unlock();
	return ret;
}
EXPORT_SYMBOL_GPL(ffs_single_dev);

 
static void _ffs_free_dev(struct ffs_dev *dev)
{
	list_del(&dev->entry);

	kfree(dev);
	if (list_empty(&ffs_devices))
		functionfs_cleanup();
}

static int ffs_acquire_dev(const char *dev_name, struct ffs_data *ffs_data)
{
	int ret = 0;
	struct ffs_dev *ffs_dev;

	ffs_dev_lock();

	ffs_dev = _ffs_find_dev(dev_name);
	if (!ffs_dev) {
		ret = -ENOENT;
	} else if (ffs_dev->mounted) {
		ret = -EBUSY;
	} else if (ffs_dev->ffs_acquire_dev_callback &&
		   ffs_dev->ffs_acquire_dev_callback(ffs_dev)) {
		ret = -ENOENT;
	} else {
		ffs_dev->mounted = true;
		ffs_dev->ffs_data = ffs_data;
		ffs_data->private_data = ffs_dev;
	}

	ffs_dev_unlock();
	return ret;
}

static void ffs_release_dev(struct ffs_dev *ffs_dev)
{
	ffs_dev_lock();

	if (ffs_dev && ffs_dev->mounted) {
		ffs_dev->mounted = false;
		if (ffs_dev->ffs_data) {
			ffs_dev->ffs_data->private_data = NULL;
			ffs_dev->ffs_data = NULL;
		}

		if (ffs_dev->ffs_release_dev_callback)
			ffs_dev->ffs_release_dev_callback(ffs_dev);
	}

	ffs_dev_unlock();
}

static int ffs_ready(struct ffs_data *ffs)
{
	struct ffs_dev *ffs_obj;
	int ret = 0;

	ffs_dev_lock();

	ffs_obj = ffs->private_data;
	if (!ffs_obj) {
		ret = -EINVAL;
		goto done;
	}
	if (WARN_ON(ffs_obj->desc_ready)) {
		ret = -EBUSY;
		goto done;
	}

	ffs_obj->desc_ready = true;

	if (ffs_obj->ffs_ready_callback) {
		ret = ffs_obj->ffs_ready_callback(ffs);
		if (ret)
			goto done;
	}

	set_bit(FFS_FL_CALL_CLOSED_CALLBACK, &ffs->flags);
done:
	ffs_dev_unlock();
	return ret;
}

static void ffs_closed(struct ffs_data *ffs)
{
	struct ffs_dev *ffs_obj;
	struct f_fs_opts *opts;
	struct config_item *ci;

	ffs_dev_lock();

	ffs_obj = ffs->private_data;
	if (!ffs_obj)
		goto done;

	ffs_obj->desc_ready = false;

	if (test_and_clear_bit(FFS_FL_CALL_CLOSED_CALLBACK, &ffs->flags) &&
	    ffs_obj->ffs_closed_callback)
		ffs_obj->ffs_closed_callback(ffs);

	if (ffs_obj->opts)
		opts = ffs_obj->opts;
	else
		goto done;

	if (opts->no_configfs || !opts->func_inst.group.cg_item.ci_parent
	    || !kref_read(&opts->func_inst.group.cg_item.ci_kref))
		goto done;

	ci = opts->func_inst.group.cg_item.ci_parent->ci_parent;
	ffs_dev_unlock();

	if (test_bit(FFS_FL_BOUND, &ffs->flags))
		unregister_gadget_item(ci);
	return;
done:
	ffs_dev_unlock();
}

 

static int ffs_mutex_lock(struct mutex *mutex, unsigned nonblock)
{
	return nonblock
		? mutex_trylock(mutex) ? 0 : -EAGAIN
		: mutex_lock_interruptible(mutex);
}

static char *ffs_prepare_buffer(const char __user *buf, size_t len)
{
	char *data;

	if (!len)
		return NULL;

	data = memdup_user(buf, len);
	if (IS_ERR(data))
		return data;

	pr_vdebug("Buffer from user space:\n");
	ffs_dump_mem("", data, len);

	return data;
}

DECLARE_USB_FUNCTION_INIT(ffs, ffs_alloc_inst, ffs_alloc);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Michal Nazarewicz");
