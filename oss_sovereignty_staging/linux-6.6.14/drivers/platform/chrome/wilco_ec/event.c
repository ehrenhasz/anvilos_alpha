
 

#include <linux/acpi.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/idr.h>
#include <linux/io.h>
#include <linux/list.h>
#include <linux/module.h>
#include <linux/poll.h>
#include <linux/spinlock.h>
#include <linux/uaccess.h>
#include <linux/wait.h>

 
#define EC_ACPI_NOTIFY_EVENT		0x90
 
#define EC_ACPI_GET_EVENT		"QSET"
 
#define EC_ACPI_MAX_EVENT_WORDS		6
#define EC_ACPI_MAX_EVENT_SIZE \
	(sizeof(struct ec_event) + (EC_ACPI_MAX_EVENT_WORDS) * sizeof(u16))

 
#define EVENT_DEV_NAME		"wilco_event"
#define EVENT_CLASS_NAME	EVENT_DEV_NAME
#define DRV_NAME		EVENT_DEV_NAME
#define EVENT_DEV_NAME_FMT	(EVENT_DEV_NAME "%d")
static struct class event_class = {
	.name	= EVENT_CLASS_NAME,
};

 
#define EVENT_MAX_DEV 128
static int event_major;
static DEFINE_IDA(event_ida);

 
#define MAX_NUM_EVENTS 64

 
struct ec_event {
	u16 size;
	u16 type;
	u16 event[];
} __packed;

#define ec_event_num_words(ev) (ev->size - 1)
#define ec_event_size(ev) (sizeof(*ev) + (ec_event_num_words(ev) * sizeof(u16)))

 
struct ec_event_queue {
	int capacity;
	int head;
	int tail;
	struct ec_event *entries[];
};

 
static int queue_size = 64;
module_param(queue_size, int, 0644);

static struct ec_event_queue *event_queue_new(int capacity)
{
	struct ec_event_queue *q;

	q = kzalloc(struct_size(q, entries, capacity), GFP_KERNEL);
	if (!q)
		return NULL;

	q->capacity = capacity;

	return q;
}

static inline bool event_queue_empty(struct ec_event_queue *q)
{
	 
	return q->head == q->tail && !q->entries[q->head];
}

static inline bool event_queue_full(struct ec_event_queue *q)
{
	 
	return q->head == q->tail && q->entries[q->head];
}

static struct ec_event *event_queue_pop(struct ec_event_queue *q)
{
	struct ec_event *ev;

	if (event_queue_empty(q))
		return NULL;

	ev = q->entries[q->tail];
	q->entries[q->tail] = NULL;
	q->tail = (q->tail + 1) % q->capacity;

	return ev;
}

 
static struct ec_event *event_queue_push(struct ec_event_queue *q,
					 struct ec_event *ev)
{
	struct ec_event *popped = NULL;

	if (event_queue_full(q))
		popped = event_queue_pop(q);
	q->entries[q->head] = ev;
	q->head = (q->head + 1) % q->capacity;

	return popped;
}

static void event_queue_free(struct ec_event_queue *q)
{
	struct ec_event *event;

	while ((event = event_queue_pop(q)) != NULL)
		kfree(event);

	kfree(q);
}

 
struct event_device_data {
	struct ec_event_queue *events;
	spinlock_t queue_lock;
	wait_queue_head_t wq;
	struct device dev;
	struct cdev cdev;
	bool exist;
	atomic_t available;
};

 
static int enqueue_events(struct acpi_device *adev, const u8 *buf, u32 length)
{
	struct event_device_data *dev_data = adev->driver_data;
	struct ec_event *event, *queue_event, *old_event;
	size_t num_words, event_size;
	u32 offset = 0;

	while (offset < length) {
		event = (struct ec_event *)(buf + offset);

		num_words = ec_event_num_words(event);
		event_size = ec_event_size(event);
		if (num_words > EC_ACPI_MAX_EVENT_WORDS) {
			dev_err(&adev->dev, "Too many event words: %zu > %d\n",
				num_words, EC_ACPI_MAX_EVENT_WORDS);
			return -EOVERFLOW;
		}

		 
		if ((offset + event_size) > length) {
			dev_err(&adev->dev, "Event exceeds buffer: %zu > %d\n",
				offset + event_size, length);
			return -EOVERFLOW;
		}

		 
		offset += event_size;

		 
		queue_event = kmemdup(event, event_size, GFP_KERNEL);
		if (!queue_event)
			return -ENOMEM;
		spin_lock(&dev_data->queue_lock);
		old_event = event_queue_push(dev_data->events, queue_event);
		spin_unlock(&dev_data->queue_lock);
		kfree(old_event);
		wake_up_interruptible(&dev_data->wq);
	}

	return 0;
}

 
static void event_device_notify(struct acpi_device *adev, u32 value)
{
	struct acpi_buffer event_buffer = { ACPI_ALLOCATE_BUFFER, NULL };
	union acpi_object *obj;
	acpi_status status;

	if (value != EC_ACPI_NOTIFY_EVENT) {
		dev_err(&adev->dev, "Invalid event: 0x%08x\n", value);
		return;
	}

	 
	status = acpi_evaluate_object(adev->handle, EC_ACPI_GET_EVENT,
				      NULL, &event_buffer);
	if (ACPI_FAILURE(status)) {
		dev_err(&adev->dev, "Error executing ACPI method %s()\n",
			EC_ACPI_GET_EVENT);
		return;
	}

	obj = (union acpi_object *)event_buffer.pointer;
	if (!obj) {
		dev_err(&adev->dev, "Nothing returned from %s()\n",
			EC_ACPI_GET_EVENT);
		return;
	}
	if (obj->type != ACPI_TYPE_BUFFER) {
		dev_err(&adev->dev, "Invalid object returned from %s()\n",
			EC_ACPI_GET_EVENT);
		kfree(obj);
		return;
	}
	if (obj->buffer.length < sizeof(struct ec_event)) {
		dev_err(&adev->dev, "Invalid buffer length %d from %s()\n",
			obj->buffer.length, EC_ACPI_GET_EVENT);
		kfree(obj);
		return;
	}

	enqueue_events(adev, obj->buffer.pointer, obj->buffer.length);
	kfree(obj);
}

static int event_open(struct inode *inode, struct file *filp)
{
	struct event_device_data *dev_data;

	dev_data = container_of(inode->i_cdev, struct event_device_data, cdev);
	if (!dev_data->exist)
		return -ENODEV;

	if (atomic_cmpxchg(&dev_data->available, 1, 0) == 0)
		return -EBUSY;

	 
	get_device(&dev_data->dev);
	stream_open(inode, filp);
	filp->private_data = dev_data;

	return 0;
}

static __poll_t event_poll(struct file *filp, poll_table *wait)
{
	struct event_device_data *dev_data = filp->private_data;
	__poll_t mask = 0;

	poll_wait(filp, &dev_data->wq, wait);
	if (!dev_data->exist)
		return EPOLLHUP;
	if (!event_queue_empty(dev_data->events))
		mask |= EPOLLIN | EPOLLRDNORM | EPOLLPRI;
	return mask;
}

 
static ssize_t event_read(struct file *filp, char __user *buf, size_t count,
			  loff_t *pos)
{
	struct event_device_data *dev_data = filp->private_data;
	struct ec_event *event;
	ssize_t n_bytes_written = 0;
	int err;

	 
	if (count != 0 && count < EC_ACPI_MAX_EVENT_SIZE)
		return -EINVAL;

	spin_lock(&dev_data->queue_lock);
	while (event_queue_empty(dev_data->events)) {
		spin_unlock(&dev_data->queue_lock);
		if (filp->f_flags & O_NONBLOCK)
			return -EAGAIN;

		err = wait_event_interruptible(dev_data->wq,
					!event_queue_empty(dev_data->events) ||
					!dev_data->exist);
		if (err)
			return err;

		 
		if (!dev_data->exist)
			return -ENODEV;
		spin_lock(&dev_data->queue_lock);
	}
	event = event_queue_pop(dev_data->events);
	spin_unlock(&dev_data->queue_lock);
	n_bytes_written = ec_event_size(event);
	if (copy_to_user(buf, event, n_bytes_written))
		n_bytes_written = -EFAULT;
	kfree(event);

	return n_bytes_written;
}

static int event_release(struct inode *inode, struct file *filp)
{
	struct event_device_data *dev_data = filp->private_data;

	atomic_set(&dev_data->available, 1);
	put_device(&dev_data->dev);

	return 0;
}

static const struct file_operations event_fops = {
	.open = event_open,
	.poll  = event_poll,
	.read = event_read,
	.release = event_release,
	.llseek = no_llseek,
	.owner = THIS_MODULE,
};

 
static void free_device_data(struct device *d)
{
	struct event_device_data *dev_data;

	dev_data = container_of(d, struct event_device_data, dev);
	event_queue_free(dev_data->events);
	kfree(dev_data);
}

static void hangup_device(struct event_device_data *dev_data)
{
	dev_data->exist = false;
	 
	wake_up_interruptible(&dev_data->wq);
	put_device(&dev_data->dev);
}

 
static int event_device_add(struct acpi_device *adev)
{
	struct event_device_data *dev_data;
	int error, minor;

	minor = ida_alloc_max(&event_ida, EVENT_MAX_DEV-1, GFP_KERNEL);
	if (minor < 0) {
		error = minor;
		dev_err(&adev->dev, "Failed to find minor number: %d\n", error);
		return error;
	}

	dev_data = kzalloc(sizeof(*dev_data), GFP_KERNEL);
	if (!dev_data) {
		error = -ENOMEM;
		goto free_minor;
	}

	 
	adev->driver_data = dev_data;
	dev_data->events = event_queue_new(queue_size);
	if (!dev_data->events) {
		kfree(dev_data);
		error = -ENOMEM;
		goto free_minor;
	}
	spin_lock_init(&dev_data->queue_lock);
	init_waitqueue_head(&dev_data->wq);
	dev_data->exist = true;
	atomic_set(&dev_data->available, 1);

	 
	dev_data->dev.devt = MKDEV(event_major, minor);
	dev_data->dev.class = &event_class;
	dev_data->dev.release = free_device_data;
	dev_set_name(&dev_data->dev, EVENT_DEV_NAME_FMT, minor);
	device_initialize(&dev_data->dev);

	 
	cdev_init(&dev_data->cdev, &event_fops);
	error = cdev_device_add(&dev_data->cdev, &dev_data->dev);
	if (error)
		goto free_dev_data;

	return 0;

free_dev_data:
	hangup_device(dev_data);
free_minor:
	ida_simple_remove(&event_ida, minor);
	return error;
}

static void event_device_remove(struct acpi_device *adev)
{
	struct event_device_data *dev_data = adev->driver_data;

	cdev_device_del(&dev_data->cdev, &dev_data->dev);
	ida_simple_remove(&event_ida, MINOR(dev_data->dev.devt));
	hangup_device(dev_data);
}

static const struct acpi_device_id event_acpi_ids[] = {
	{ "GOOG000D", 0 },
	{ }
};
MODULE_DEVICE_TABLE(acpi, event_acpi_ids);

static struct acpi_driver event_driver = {
	.name = DRV_NAME,
	.class = DRV_NAME,
	.ids = event_acpi_ids,
	.ops = {
		.add = event_device_add,
		.notify = event_device_notify,
		.remove = event_device_remove,
	},
	.owner = THIS_MODULE,
};

static int __init event_module_init(void)
{
	dev_t dev_num = 0;
	int ret;

	ret = class_register(&event_class);
	if (ret) {
		pr_err(DRV_NAME ": Failed registering class: %d\n", ret);
		return ret;
	}

	 
	ret = alloc_chrdev_region(&dev_num, 0, EVENT_MAX_DEV, EVENT_DEV_NAME);
	if (ret) {
		pr_err(DRV_NAME ": Failed allocating dev numbers: %d\n", ret);
		goto destroy_class;
	}
	event_major = MAJOR(dev_num);

	ret = acpi_bus_register_driver(&event_driver);
	if (ret < 0) {
		pr_err(DRV_NAME ": Failed registering driver: %d\n", ret);
		goto unregister_region;
	}

	return 0;

unregister_region:
	unregister_chrdev_region(MKDEV(event_major, 0), EVENT_MAX_DEV);
destroy_class:
	class_unregister(&event_class);
	ida_destroy(&event_ida);
	return ret;
}

static void __exit event_module_exit(void)
{
	acpi_bus_unregister_driver(&event_driver);
	unregister_chrdev_region(MKDEV(event_major, 0), EVENT_MAX_DEV);
	class_unregister(&event_class);
	ida_destroy(&event_ida);
}

module_init(event_module_init);
module_exit(event_module_exit);

MODULE_AUTHOR("Nick Crews <ncrews@chromium.org>");
MODULE_DESCRIPTION("Wilco EC ACPI event driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:" DRV_NAME);
