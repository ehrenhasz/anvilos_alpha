
 

 

#include <linux/module.h>
#include <linux/mod_devicetable.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/spinlock.h>
#include <linux/miscdevice.h>
#include <linux/platform_device.h>
#include <linux/poll.h>
#include <linux/sched.h>
#include <linux/bitops.h>
#include <linux/slab.h>
#include <linux/io.h>
#include <linux/dma-mapping.h>
#include <linux/mm.h>
#include <linux/acpi.h>
#include <linux/bug.h>
#include "goldfish_pipe_qemu.h"

 
enum {
	PIPE_DRIVER_VERSION = 2,
	PIPE_CURRENT_DEVICE_VERSION = 2
};

enum {
	MAX_BUFFERS_PER_COMMAND = 336,
	MAX_SIGNALLED_PIPES = 64,
	INITIAL_PIPES_CAPACITY = 64
};

struct goldfish_pipe_dev;

 
struct goldfish_pipe_command {
	s32 cmd;	 
	s32 id;		 
	s32 status;	 
	s32 reserved;	 
	union {
		 
		struct {
			 
			u32 buffers_count;
			 
			s32 consumed_size;
			 
			u64 ptrs[MAX_BUFFERS_PER_COMMAND];
			 
			u32 sizes[MAX_BUFFERS_PER_COMMAND];
		} rw_params;
	};
};

 
struct signalled_pipe_buffer {
	u32 id;
	u32 flags;
};

 
struct open_command_param {
	u64 command_buffer_ptr;
	u32 rw_params_max_count;
};

 
struct goldfish_pipe_dev_buffers {
	struct open_command_param open_command_params;
	struct signalled_pipe_buffer
		signalled_pipe_buffers[MAX_SIGNALLED_PIPES];
};

 
struct goldfish_pipe {
	 
	u32 id;

	 
	unsigned long flags;

	 
	unsigned long signalled_flags;

	 
	struct goldfish_pipe_command *command_buffer;

	 
	struct goldfish_pipe *prev_signalled;
	struct goldfish_pipe *next_signalled;

	 
	struct mutex lock;

	 
	wait_queue_head_t wake_queue;

	 
	struct goldfish_pipe_dev *dev;

	 
	struct page *pages[MAX_BUFFERS_PER_COMMAND];
};

 
struct goldfish_pipe_dev {
	 
	void *magic;

	 
	spinlock_t lock;

	 
	struct goldfish_pipe **pipes;
	u32 pipes_capacity;

	 
	struct goldfish_pipe_dev_buffers *buffers;

	 
	struct goldfish_pipe *first_signalled_pipe;

	 
	struct device *pdev_dev;

	 
	int irq;
	int version;
	unsigned char __iomem *base;

	struct miscdevice miscdev;
};

static int goldfish_pipe_cmd_locked(struct goldfish_pipe *pipe,
				    enum PipeCmdCode cmd)
{
	pipe->command_buffer->cmd = cmd;
	 
	pipe->command_buffer->status = PIPE_ERROR_INVAL;
	writel(pipe->id, pipe->dev->base + PIPE_REG_CMD);
	return pipe->command_buffer->status;
}

static int goldfish_pipe_cmd(struct goldfish_pipe *pipe, enum PipeCmdCode cmd)
{
	int status;

	if (mutex_lock_interruptible(&pipe->lock))
		return PIPE_ERROR_IO;
	status = goldfish_pipe_cmd_locked(pipe, cmd);
	mutex_unlock(&pipe->lock);
	return status;
}

 
static int goldfish_pipe_error_convert(int status)
{
	switch (status) {
	case PIPE_ERROR_AGAIN:
		return -EAGAIN;
	case PIPE_ERROR_NOMEM:
		return -ENOMEM;
	case PIPE_ERROR_IO:
		return -EIO;
	default:
		return -EINVAL;
	}
}

static int goldfish_pin_pages(unsigned long first_page,
			      unsigned long last_page,
			      unsigned int last_page_size,
			      int is_write,
			      struct page *pages[MAX_BUFFERS_PER_COMMAND],
			      unsigned int *iter_last_page_size)
{
	int ret;
	int requested_pages = ((last_page - first_page) >> PAGE_SHIFT) + 1;

	if (requested_pages > MAX_BUFFERS_PER_COMMAND) {
		requested_pages = MAX_BUFFERS_PER_COMMAND;
		*iter_last_page_size = PAGE_SIZE;
	} else {
		*iter_last_page_size = last_page_size;
	}

	ret = pin_user_pages_fast(first_page, requested_pages,
				  !is_write ? FOLL_WRITE : 0,
				  pages);
	if (ret <= 0)
		return -EFAULT;
	if (ret < requested_pages)
		*iter_last_page_size = PAGE_SIZE;

	return ret;
}

 
static void populate_rw_params(struct page **pages,
			       int pages_count,
			       unsigned long address,
			       unsigned long address_end,
			       unsigned long first_page,
			       unsigned long last_page,
			       unsigned int iter_last_page_size,
			       int is_write,
			       struct goldfish_pipe_command *command)
{
	 
	unsigned long xaddr = page_to_phys(pages[0]);
	unsigned long xaddr_prev = xaddr;
	int buffer_idx = 0;
	int i = 1;
	int size_on_page = first_page == last_page
			? (int)(address_end - address)
			: (PAGE_SIZE - (address & ~PAGE_MASK));
	command->rw_params.ptrs[0] = (u64)(xaddr | (address & ~PAGE_MASK));
	command->rw_params.sizes[0] = size_on_page;
	for (; i < pages_count; ++i) {
		xaddr = page_to_phys(pages[i]);
		size_on_page = (i == pages_count - 1) ?
			iter_last_page_size : PAGE_SIZE;
		if (xaddr == xaddr_prev + PAGE_SIZE) {
			command->rw_params.sizes[buffer_idx] += size_on_page;
		} else {
			++buffer_idx;
			command->rw_params.ptrs[buffer_idx] = (u64)xaddr;
			command->rw_params.sizes[buffer_idx] = size_on_page;
		}
		xaddr_prev = xaddr;
	}
	command->rw_params.buffers_count = buffer_idx + 1;
}

static int transfer_max_buffers(struct goldfish_pipe *pipe,
				unsigned long address,
				unsigned long address_end,
				int is_write,
				unsigned long last_page,
				unsigned int last_page_size,
				s32 *consumed_size,
				int *status)
{
	unsigned long first_page = address & PAGE_MASK;
	unsigned int iter_last_page_size;
	int pages_count;

	 
	if (mutex_lock_interruptible(&pipe->lock))
		return -ERESTARTSYS;

	pages_count = goldfish_pin_pages(first_page, last_page,
					 last_page_size, is_write,
					 pipe->pages, &iter_last_page_size);
	if (pages_count < 0) {
		mutex_unlock(&pipe->lock);
		return pages_count;
	}

	populate_rw_params(pipe->pages, pages_count, address, address_end,
			   first_page, last_page, iter_last_page_size, is_write,
			   pipe->command_buffer);

	 
	*status = goldfish_pipe_cmd_locked(pipe,
				is_write ? PIPE_CMD_WRITE : PIPE_CMD_READ);

	*consumed_size = pipe->command_buffer->rw_params.consumed_size;

	unpin_user_pages_dirty_lock(pipe->pages, pages_count,
				    !is_write && *consumed_size > 0);

	mutex_unlock(&pipe->lock);
	return 0;
}

static int wait_for_host_signal(struct goldfish_pipe *pipe, int is_write)
{
	u32 wake_bit = is_write ? BIT_WAKE_ON_WRITE : BIT_WAKE_ON_READ;

	set_bit(wake_bit, &pipe->flags);

	 
	goldfish_pipe_cmd(pipe,
		is_write ? PIPE_CMD_WAKE_ON_WRITE : PIPE_CMD_WAKE_ON_READ);

	while (test_bit(wake_bit, &pipe->flags)) {
		if (wait_event_interruptible(pipe->wake_queue,
					     !test_bit(wake_bit, &pipe->flags)))
			return -ERESTARTSYS;

		if (test_bit(BIT_CLOSED_ON_HOST, &pipe->flags))
			return -EIO;
	}

	return 0;
}

static ssize_t goldfish_pipe_read_write(struct file *filp,
					char __user *buffer,
					size_t bufflen,
					int is_write)
{
	struct goldfish_pipe *pipe = filp->private_data;
	int count = 0, ret = -EINVAL;
	unsigned long address, address_end, last_page;
	unsigned int last_page_size;

	 
	if (unlikely(test_bit(BIT_CLOSED_ON_HOST, &pipe->flags)))
		return -EIO;
	 
	if (unlikely(bufflen == 0))
		return 0;
	 
	if (unlikely(!access_ok(buffer, bufflen)))
		return -EFAULT;

	address = (unsigned long)buffer;
	address_end = address + bufflen;
	last_page = (address_end - 1) & PAGE_MASK;
	last_page_size = ((address_end - 1) & ~PAGE_MASK) + 1;

	while (address < address_end) {
		s32 consumed_size;
		int status;

		ret = transfer_max_buffers(pipe, address, address_end, is_write,
					   last_page, last_page_size,
					   &consumed_size, &status);
		if (ret < 0)
			break;

		if (consumed_size > 0) {
			 
			count += consumed_size;
			address += consumed_size;
		}
		if (status > 0)
			continue;
		if (status == 0) {
			 
			ret = 0;
			break;
		}
		if (count > 0) {
			 
			if (status != PIPE_ERROR_AGAIN)
				dev_err_ratelimited(pipe->dev->pdev_dev,
					"backend error %d on %s\n",
					status, is_write ? "write" : "read");
			break;
		}

		 
		if (status != PIPE_ERROR_AGAIN ||
			(filp->f_flags & O_NONBLOCK) != 0) {
			ret = goldfish_pipe_error_convert(status);
			break;
		}

		status = wait_for_host_signal(pipe, is_write);
		if (status < 0)
			return status;
	}

	if (count > 0)
		return count;
	return ret;
}

static ssize_t goldfish_pipe_read(struct file *filp, char __user *buffer,
				  size_t bufflen, loff_t *ppos)
{
	return goldfish_pipe_read_write(filp, buffer, bufflen,
					  0);
}

static ssize_t goldfish_pipe_write(struct file *filp,
				   const char __user *buffer, size_t bufflen,
				   loff_t *ppos)
{
	 
	char __user *no_const_buffer = (char __user *)buffer;

	return goldfish_pipe_read_write(filp, no_const_buffer, bufflen,
					  1);
}

static __poll_t goldfish_pipe_poll(struct file *filp, poll_table *wait)
{
	struct goldfish_pipe *pipe = filp->private_data;
	__poll_t mask = 0;
	int status;

	poll_wait(filp, &pipe->wake_queue, wait);

	status = goldfish_pipe_cmd(pipe, PIPE_CMD_POLL);
	if (status < 0)
		return -ERESTARTSYS;

	if (status & PIPE_POLL_IN)
		mask |= EPOLLIN | EPOLLRDNORM;
	if (status & PIPE_POLL_OUT)
		mask |= EPOLLOUT | EPOLLWRNORM;
	if (status & PIPE_POLL_HUP)
		mask |= EPOLLHUP;
	if (test_bit(BIT_CLOSED_ON_HOST, &pipe->flags))
		mask |= EPOLLERR;

	return mask;
}

static void signalled_pipes_add_locked(struct goldfish_pipe_dev *dev,
				       u32 id, u32 flags)
{
	struct goldfish_pipe *pipe;

	if (WARN_ON(id >= dev->pipes_capacity))
		return;

	pipe = dev->pipes[id];
	if (!pipe)
		return;
	pipe->signalled_flags |= flags;

	if (pipe->prev_signalled || pipe->next_signalled ||
		dev->first_signalled_pipe == pipe)
		return;	 
	pipe->next_signalled = dev->first_signalled_pipe;
	if (dev->first_signalled_pipe)
		dev->first_signalled_pipe->prev_signalled = pipe;
	dev->first_signalled_pipe = pipe;
}

static void signalled_pipes_remove_locked(struct goldfish_pipe_dev *dev,
					  struct goldfish_pipe *pipe)
{
	if (pipe->prev_signalled)
		pipe->prev_signalled->next_signalled = pipe->next_signalled;
	if (pipe->next_signalled)
		pipe->next_signalled->prev_signalled = pipe->prev_signalled;
	if (pipe == dev->first_signalled_pipe)
		dev->first_signalled_pipe = pipe->next_signalled;
	pipe->prev_signalled = NULL;
	pipe->next_signalled = NULL;
}

static struct goldfish_pipe *signalled_pipes_pop_front(
		struct goldfish_pipe_dev *dev, int *wakes)
{
	struct goldfish_pipe *pipe;
	unsigned long flags;

	spin_lock_irqsave(&dev->lock, flags);

	pipe = dev->first_signalled_pipe;
	if (pipe) {
		*wakes = pipe->signalled_flags;
		pipe->signalled_flags = 0;
		 
		dev->first_signalled_pipe = pipe->next_signalled;
		if (dev->first_signalled_pipe)
			dev->first_signalled_pipe->prev_signalled = NULL;
		pipe->next_signalled = NULL;
	}

	spin_unlock_irqrestore(&dev->lock, flags);
	return pipe;
}

static irqreturn_t goldfish_interrupt_task(int irq, void *dev_addr)
{
	 
	struct goldfish_pipe_dev *dev = dev_addr;
	struct goldfish_pipe *pipe;
	int wakes;

	while ((pipe = signalled_pipes_pop_front(dev, &wakes)) != NULL) {
		if (wakes & PIPE_WAKE_CLOSED) {
			pipe->flags = 1 << BIT_CLOSED_ON_HOST;
		} else {
			if (wakes & PIPE_WAKE_READ)
				clear_bit(BIT_WAKE_ON_READ, &pipe->flags);
			if (wakes & PIPE_WAKE_WRITE)
				clear_bit(BIT_WAKE_ON_WRITE, &pipe->flags);
		}
		 
		wake_up_interruptible(&pipe->wake_queue);
	}
	return IRQ_HANDLED;
}

static void goldfish_pipe_device_deinit(struct platform_device *pdev,
					struct goldfish_pipe_dev *dev);

 
static irqreturn_t goldfish_pipe_interrupt(int irq, void *dev_id)
{
	u32 count;
	u32 i;
	unsigned long flags;
	struct goldfish_pipe_dev *dev = dev_id;

	if (dev->magic != &goldfish_pipe_device_deinit)
		return IRQ_NONE;

	 
	spin_lock_irqsave(&dev->lock, flags);

	count = readl(dev->base + PIPE_REG_GET_SIGNALLED);
	if (count == 0) {
		spin_unlock_irqrestore(&dev->lock, flags);
		return IRQ_NONE;
	}
	if (count > MAX_SIGNALLED_PIPES)
		count = MAX_SIGNALLED_PIPES;

	for (i = 0; i < count; ++i)
		signalled_pipes_add_locked(dev,
			dev->buffers->signalled_pipe_buffers[i].id,
			dev->buffers->signalled_pipe_buffers[i].flags);

	spin_unlock_irqrestore(&dev->lock, flags);

	return IRQ_WAKE_THREAD;
}

static int get_free_pipe_id_locked(struct goldfish_pipe_dev *dev)
{
	int id;

	for (id = 0; id < dev->pipes_capacity; ++id)
		if (!dev->pipes[id])
			return id;

	{
		 
		u32 new_capacity = 2 * dev->pipes_capacity;
		struct goldfish_pipe **pipes =
			kcalloc(new_capacity, sizeof(*pipes), GFP_ATOMIC);
		if (!pipes)
			return -ENOMEM;
		memcpy(pipes, dev->pipes, sizeof(*pipes) * dev->pipes_capacity);
		kfree(dev->pipes);
		dev->pipes = pipes;
		id = dev->pipes_capacity;
		dev->pipes_capacity = new_capacity;
	}
	return id;
}

 
static struct goldfish_pipe_dev *to_goldfish_pipe_dev(struct file *file)
{
	struct miscdevice *miscdev = file->private_data;

	return container_of(miscdev, struct goldfish_pipe_dev, miscdev);
}

 
static int goldfish_pipe_open(struct inode *inode, struct file *file)
{
	struct goldfish_pipe_dev *dev = to_goldfish_pipe_dev(file);
	unsigned long flags;
	int id;
	int status;

	 
	struct goldfish_pipe *pipe = kzalloc(sizeof(*pipe), GFP_KERNEL);

	if (!pipe)
		return -ENOMEM;

	pipe->dev = dev;
	mutex_init(&pipe->lock);
	init_waitqueue_head(&pipe->wake_queue);

	 
	BUILD_BUG_ON(sizeof(struct goldfish_pipe_command) > PAGE_SIZE);
	pipe->command_buffer =
		(struct goldfish_pipe_command *)__get_free_page(GFP_KERNEL);
	if (!pipe->command_buffer) {
		status = -ENOMEM;
		goto err_pipe;
	}

	spin_lock_irqsave(&dev->lock, flags);

	id = get_free_pipe_id_locked(dev);
	if (id < 0) {
		status = id;
		goto err_id_locked;
	}

	dev->pipes[id] = pipe;
	pipe->id = id;
	pipe->command_buffer->id = id;

	 
	dev->buffers->open_command_params.rw_params_max_count =
			MAX_BUFFERS_PER_COMMAND;
	dev->buffers->open_command_params.command_buffer_ptr =
			(u64)(unsigned long)__pa(pipe->command_buffer);
	status = goldfish_pipe_cmd_locked(pipe, PIPE_CMD_OPEN);
	spin_unlock_irqrestore(&dev->lock, flags);
	if (status < 0)
		goto err_cmd;
	 
	file->private_data = pipe;
	return 0;

err_cmd:
	spin_lock_irqsave(&dev->lock, flags);
	dev->pipes[id] = NULL;
err_id_locked:
	spin_unlock_irqrestore(&dev->lock, flags);
	free_page((unsigned long)pipe->command_buffer);
err_pipe:
	kfree(pipe);
	return status;
}

static int goldfish_pipe_release(struct inode *inode, struct file *filp)
{
	unsigned long flags;
	struct goldfish_pipe *pipe = filp->private_data;
	struct goldfish_pipe_dev *dev = pipe->dev;

	 
	goldfish_pipe_cmd(pipe, PIPE_CMD_CLOSE);

	spin_lock_irqsave(&dev->lock, flags);
	dev->pipes[pipe->id] = NULL;
	signalled_pipes_remove_locked(dev, pipe);
	spin_unlock_irqrestore(&dev->lock, flags);

	filp->private_data = NULL;
	free_page((unsigned long)pipe->command_buffer);
	kfree(pipe);
	return 0;
}

static const struct file_operations goldfish_pipe_fops = {
	.owner = THIS_MODULE,
	.read = goldfish_pipe_read,
	.write = goldfish_pipe_write,
	.poll = goldfish_pipe_poll,
	.open = goldfish_pipe_open,
	.release = goldfish_pipe_release,
};

static void init_miscdevice(struct miscdevice *miscdev)
{
	memset(miscdev, 0, sizeof(*miscdev));

	miscdev->minor = MISC_DYNAMIC_MINOR;
	miscdev->name = "goldfish_pipe";
	miscdev->fops = &goldfish_pipe_fops;
}

static void write_pa_addr(void *addr, void __iomem *portl, void __iomem *porth)
{
	const unsigned long paddr = __pa(addr);

	writel(upper_32_bits(paddr), porth);
	writel(lower_32_bits(paddr), portl);
}

static int goldfish_pipe_device_init(struct platform_device *pdev,
				     struct goldfish_pipe_dev *dev)
{
	int err;

	err = devm_request_threaded_irq(&pdev->dev, dev->irq,
					goldfish_pipe_interrupt,
					goldfish_interrupt_task,
					IRQF_SHARED, "goldfish_pipe", dev);
	if (err) {
		dev_err(&pdev->dev, "unable to allocate IRQ for v2\n");
		return err;
	}

	init_miscdevice(&dev->miscdev);
	err = misc_register(&dev->miscdev);
	if (err) {
		dev_err(&pdev->dev, "unable to register v2 device\n");
		return err;
	}

	dev->pdev_dev = &pdev->dev;
	dev->first_signalled_pipe = NULL;
	dev->pipes_capacity = INITIAL_PIPES_CAPACITY;
	dev->pipes = kcalloc(dev->pipes_capacity, sizeof(*dev->pipes),
			     GFP_KERNEL);
	if (!dev->pipes) {
		misc_deregister(&dev->miscdev);
		return -ENOMEM;
	}

	 
	BUILD_BUG_ON(sizeof(struct goldfish_pipe_dev_buffers) > PAGE_SIZE);
	dev->buffers = (struct goldfish_pipe_dev_buffers *)
		__get_free_page(GFP_KERNEL);
	if (!dev->buffers) {
		kfree(dev->pipes);
		misc_deregister(&dev->miscdev);
		return -ENOMEM;
	}

	 
	write_pa_addr(&dev->buffers->signalled_pipe_buffers,
		      dev->base + PIPE_REG_SIGNAL_BUFFER,
		      dev->base + PIPE_REG_SIGNAL_BUFFER_HIGH);

	writel(MAX_SIGNALLED_PIPES,
	       dev->base + PIPE_REG_SIGNAL_BUFFER_COUNT);

	write_pa_addr(&dev->buffers->open_command_params,
		      dev->base + PIPE_REG_OPEN_BUFFER,
		      dev->base + PIPE_REG_OPEN_BUFFER_HIGH);

	platform_set_drvdata(pdev, dev);
	return 0;
}

static void goldfish_pipe_device_deinit(struct platform_device *pdev,
					struct goldfish_pipe_dev *dev)
{
	misc_deregister(&dev->miscdev);
	kfree(dev->pipes);
	free_page((unsigned long)dev->buffers);
}

static int goldfish_pipe_probe(struct platform_device *pdev)
{
	struct resource *r;
	struct goldfish_pipe_dev *dev;

	dev = devm_kzalloc(&pdev->dev, sizeof(*dev), GFP_KERNEL);
	if (!dev)
		return -ENOMEM;

	dev->magic = &goldfish_pipe_device_deinit;
	spin_lock_init(&dev->lock);

	r = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!r || resource_size(r) < PAGE_SIZE) {
		dev_err(&pdev->dev, "can't allocate i/o page\n");
		return -EINVAL;
	}
	dev->base = devm_ioremap(&pdev->dev, r->start, PAGE_SIZE);
	if (!dev->base) {
		dev_err(&pdev->dev, "ioremap failed\n");
		return -EINVAL;
	}

	dev->irq = platform_get_irq(pdev, 0);
	if (dev->irq < 0)
		return dev->irq;

	 
	writel(PIPE_DRIVER_VERSION, dev->base + PIPE_REG_VERSION);
	dev->version = readl(dev->base + PIPE_REG_VERSION);
	if (WARN_ON(dev->version < PIPE_CURRENT_DEVICE_VERSION))
		return -EINVAL;

	return goldfish_pipe_device_init(pdev, dev);
}

static int goldfish_pipe_remove(struct platform_device *pdev)
{
	struct goldfish_pipe_dev *dev = platform_get_drvdata(pdev);

	goldfish_pipe_device_deinit(pdev, dev);
	return 0;
}

static const struct acpi_device_id goldfish_pipe_acpi_match[] = {
	{ "GFSH0003", 0 },
	{ },
};
MODULE_DEVICE_TABLE(acpi, goldfish_pipe_acpi_match);

static const struct of_device_id goldfish_pipe_of_match[] = {
	{ .compatible = "google,android-pipe", },
	{},
};
MODULE_DEVICE_TABLE(of, goldfish_pipe_of_match);

static struct platform_driver goldfish_pipe_driver = {
	.probe = goldfish_pipe_probe,
	.remove = goldfish_pipe_remove,
	.driver = {
		.name = "goldfish_pipe",
		.of_match_table = goldfish_pipe_of_match,
		.acpi_match_table = ACPI_PTR(goldfish_pipe_acpi_match),
	}
};

module_platform_driver(goldfish_pipe_driver);
MODULE_AUTHOR("David Turner <digit@google.com>");
MODULE_LICENSE("GPL v2");
