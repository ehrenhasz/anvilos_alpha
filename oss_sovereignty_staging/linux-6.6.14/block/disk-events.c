
 
#include <linux/export.h>
#include <linux/moduleparam.h>
#include <linux/blkdev.h>
#include "blk.h"

struct disk_events {
	struct list_head	node;		 
	struct gendisk		*disk;		 
	spinlock_t		lock;

	struct mutex		block_mutex;	 
	int			block;		 
	unsigned int		pending;	 
	unsigned int		clearing;	 

	long			poll_msecs;	 
	struct delayed_work	dwork;
};

static const char *disk_events_strs[] = {
	[ilog2(DISK_EVENT_MEDIA_CHANGE)]	= "media_change",
	[ilog2(DISK_EVENT_EJECT_REQUEST)]	= "eject_request",
};

static char *disk_uevents[] = {
	[ilog2(DISK_EVENT_MEDIA_CHANGE)]	= "DISK_MEDIA_CHANGE=1",
	[ilog2(DISK_EVENT_EJECT_REQUEST)]	= "DISK_EJECT_REQUEST=1",
};

 
static DEFINE_MUTEX(disk_events_mutex);
static LIST_HEAD(disk_events);

 
static unsigned long disk_events_dfl_poll_msecs;

static unsigned long disk_events_poll_jiffies(struct gendisk *disk)
{
	struct disk_events *ev = disk->ev;
	long intv_msecs = 0;

	 
	if (ev->poll_msecs >= 0)
		intv_msecs = ev->poll_msecs;
	else if (disk->event_flags & DISK_EVENT_FLAG_POLL)
		intv_msecs = disk_events_dfl_poll_msecs;

	return msecs_to_jiffies(intv_msecs);
}

 
void disk_block_events(struct gendisk *disk)
{
	struct disk_events *ev = disk->ev;
	unsigned long flags;
	bool cancel;

	if (!ev)
		return;

	 
	mutex_lock(&ev->block_mutex);

	spin_lock_irqsave(&ev->lock, flags);
	cancel = !ev->block++;
	spin_unlock_irqrestore(&ev->lock, flags);

	if (cancel)
		cancel_delayed_work_sync(&disk->ev->dwork);

	mutex_unlock(&ev->block_mutex);
}

static void __disk_unblock_events(struct gendisk *disk, bool check_now)
{
	struct disk_events *ev = disk->ev;
	unsigned long intv;
	unsigned long flags;

	spin_lock_irqsave(&ev->lock, flags);

	if (WARN_ON_ONCE(ev->block <= 0))
		goto out_unlock;

	if (--ev->block)
		goto out_unlock;

	intv = disk_events_poll_jiffies(disk);
	if (check_now)
		queue_delayed_work(system_freezable_power_efficient_wq,
				&ev->dwork, 0);
	else if (intv)
		queue_delayed_work(system_freezable_power_efficient_wq,
				&ev->dwork, intv);
out_unlock:
	spin_unlock_irqrestore(&ev->lock, flags);
}

 
void disk_unblock_events(struct gendisk *disk)
{
	if (disk->ev)
		__disk_unblock_events(disk, false);
}

 
void disk_flush_events(struct gendisk *disk, unsigned int mask)
{
	struct disk_events *ev = disk->ev;

	if (!ev)
		return;

	spin_lock_irq(&ev->lock);
	ev->clearing |= mask;
	if (!ev->block)
		mod_delayed_work(system_freezable_power_efficient_wq,
				&ev->dwork, 0);
	spin_unlock_irq(&ev->lock);
}

 
static void disk_event_uevent(struct gendisk *disk, unsigned int events)
{
	char *envp[ARRAY_SIZE(disk_uevents) + 1] = { };
	int nr_events = 0, i;

	for (i = 0; i < ARRAY_SIZE(disk_uevents); i++)
		if (events & disk->events & (1 << i))
			envp[nr_events++] = disk_uevents[i];

	if (nr_events)
		kobject_uevent_env(&disk_to_dev(disk)->kobj, KOBJ_CHANGE, envp);
}

static void disk_check_events(struct disk_events *ev,
			      unsigned int *clearing_ptr)
{
	struct gendisk *disk = ev->disk;
	unsigned int clearing = *clearing_ptr;
	unsigned int events;
	unsigned long intv;

	 
	events = disk->fops->check_events(disk, clearing);

	 
	spin_lock_irq(&ev->lock);

	events &= ~ev->pending;
	ev->pending |= events;
	*clearing_ptr &= ~clearing;

	intv = disk_events_poll_jiffies(disk);
	if (!ev->block && intv)
		queue_delayed_work(system_freezable_power_efficient_wq,
				&ev->dwork, intv);

	spin_unlock_irq(&ev->lock);

	if (events & DISK_EVENT_MEDIA_CHANGE)
		inc_diskseq(disk);

	if (disk->event_flags & DISK_EVENT_FLAG_UEVENT)
		disk_event_uevent(disk, events);
}

 
static unsigned int disk_clear_events(struct gendisk *disk, unsigned int mask)
{
	struct disk_events *ev = disk->ev;
	unsigned int pending;
	unsigned int clearing = mask;

	if (!ev)
		return 0;

	disk_block_events(disk);

	 
	spin_lock_irq(&ev->lock);
	clearing |= ev->clearing;
	ev->clearing = 0;
	spin_unlock_irq(&ev->lock);

	disk_check_events(ev, &clearing);
	 
	__disk_unblock_events(disk, ev->clearing ? true : false);

	 
	spin_lock_irq(&ev->lock);
	pending = ev->pending & mask;
	ev->pending &= ~mask;
	spin_unlock_irq(&ev->lock);
	WARN_ON_ONCE(clearing & mask);

	return pending;
}

 
bool disk_check_media_change(struct gendisk *disk)
{
	unsigned int events;

	events = disk_clear_events(disk, DISK_EVENT_MEDIA_CHANGE |
				   DISK_EVENT_EJECT_REQUEST);
	if (!(events & DISK_EVENT_MEDIA_CHANGE))
		return false;

	bdev_mark_dead(disk->part0, true);
	set_bit(GD_NEED_PART_SCAN, &disk->state);
	return true;
}
EXPORT_SYMBOL(disk_check_media_change);

 
void disk_force_media_change(struct gendisk *disk)
{
	disk_event_uevent(disk, DISK_EVENT_MEDIA_CHANGE);
	inc_diskseq(disk);
	bdev_mark_dead(disk->part0, true);
	set_bit(GD_NEED_PART_SCAN, &disk->state);
}
EXPORT_SYMBOL_GPL(disk_force_media_change);

 
static void disk_events_workfn(struct work_struct *work)
{
	struct delayed_work *dwork = to_delayed_work(work);
	struct disk_events *ev = container_of(dwork, struct disk_events, dwork);

	disk_check_events(ev, &ev->clearing);
}

 
static ssize_t __disk_events_show(unsigned int events, char *buf)
{
	const char *delim = "";
	ssize_t pos = 0;
	int i;

	for (i = 0; i < ARRAY_SIZE(disk_events_strs); i++)
		if (events & (1 << i)) {
			pos += sprintf(buf + pos, "%s%s",
				       delim, disk_events_strs[i]);
			delim = " ";
		}
	if (pos)
		pos += sprintf(buf + pos, "\n");
	return pos;
}

static ssize_t disk_events_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct gendisk *disk = dev_to_disk(dev);

	if (!(disk->event_flags & DISK_EVENT_FLAG_UEVENT))
		return 0;
	return __disk_events_show(disk->events, buf);
}

static ssize_t disk_events_async_show(struct device *dev,
				      struct device_attribute *attr, char *buf)
{
	return 0;
}

static ssize_t disk_events_poll_msecs_show(struct device *dev,
					   struct device_attribute *attr,
					   char *buf)
{
	struct gendisk *disk = dev_to_disk(dev);

	if (!disk->ev)
		return sprintf(buf, "-1\n");
	return sprintf(buf, "%ld\n", disk->ev->poll_msecs);
}

static ssize_t disk_events_poll_msecs_store(struct device *dev,
					    struct device_attribute *attr,
					    const char *buf, size_t count)
{
	struct gendisk *disk = dev_to_disk(dev);
	long intv;

	if (!count || !sscanf(buf, "%ld", &intv))
		return -EINVAL;

	if (intv < 0 && intv != -1)
		return -EINVAL;

	if (!disk->ev)
		return -ENODEV;

	disk_block_events(disk);
	disk->ev->poll_msecs = intv;
	__disk_unblock_events(disk, true);
	return count;
}

DEVICE_ATTR(events, 0444, disk_events_show, NULL);
DEVICE_ATTR(events_async, 0444, disk_events_async_show, NULL);
DEVICE_ATTR(events_poll_msecs, 0644, disk_events_poll_msecs_show,
	    disk_events_poll_msecs_store);

 
static int disk_events_set_dfl_poll_msecs(const char *val,
					  const struct kernel_param *kp)
{
	struct disk_events *ev;
	int ret;

	ret = param_set_ulong(val, kp);
	if (ret < 0)
		return ret;

	mutex_lock(&disk_events_mutex);
	list_for_each_entry(ev, &disk_events, node)
		disk_flush_events(ev->disk, 0);
	mutex_unlock(&disk_events_mutex);
	return 0;
}

static const struct kernel_param_ops disk_events_dfl_poll_msecs_param_ops = {
	.set	= disk_events_set_dfl_poll_msecs,
	.get	= param_get_ulong,
};

#undef MODULE_PARAM_PREFIX
#define MODULE_PARAM_PREFIX	"block."

module_param_cb(events_dfl_poll_msecs, &disk_events_dfl_poll_msecs_param_ops,
		&disk_events_dfl_poll_msecs, 0644);

 
int disk_alloc_events(struct gendisk *disk)
{
	struct disk_events *ev;

	if (!disk->fops->check_events || !disk->events)
		return 0;

	ev = kzalloc(sizeof(*ev), GFP_KERNEL);
	if (!ev) {
		pr_warn("%s: failed to initialize events\n", disk->disk_name);
		return -ENOMEM;
	}

	INIT_LIST_HEAD(&ev->node);
	ev->disk = disk;
	spin_lock_init(&ev->lock);
	mutex_init(&ev->block_mutex);
	ev->block = 1;
	ev->poll_msecs = -1;
	INIT_DELAYED_WORK(&ev->dwork, disk_events_workfn);

	disk->ev = ev;
	return 0;
}

void disk_add_events(struct gendisk *disk)
{
	if (!disk->ev)
		return;

	mutex_lock(&disk_events_mutex);
	list_add_tail(&disk->ev->node, &disk_events);
	mutex_unlock(&disk_events_mutex);

	 
	__disk_unblock_events(disk, true);
}

void disk_del_events(struct gendisk *disk)
{
	if (disk->ev) {
		disk_block_events(disk);

		mutex_lock(&disk_events_mutex);
		list_del_init(&disk->ev->node);
		mutex_unlock(&disk_events_mutex);
	}
}

void disk_release_events(struct gendisk *disk)
{
	 
	WARN_ON_ONCE(disk->ev && disk->ev->block != 1);
	kfree(disk->ev);
}
