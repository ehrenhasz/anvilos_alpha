
 
#include <linux/sched/mm.h>
#include <linux/ktime.h>
#include <linux/hrtimer.h>
#include <linux/export.h>
#include <linux/pm_runtime.h>
#include <linux/pm_wakeirq.h>
#include <trace/events/rpm.h>

#include "../base.h"
#include "power.h"

typedef int (*pm_callback_t)(struct device *);

static pm_callback_t __rpm_get_callback(struct device *dev, size_t cb_offset)
{
	pm_callback_t cb;
	const struct dev_pm_ops *ops;

	if (dev->pm_domain)
		ops = &dev->pm_domain->ops;
	else if (dev->type && dev->type->pm)
		ops = dev->type->pm;
	else if (dev->class && dev->class->pm)
		ops = dev->class->pm;
	else if (dev->bus && dev->bus->pm)
		ops = dev->bus->pm;
	else
		ops = NULL;

	if (ops)
		cb = *(pm_callback_t *)((void *)ops + cb_offset);
	else
		cb = NULL;

	if (!cb && dev->driver && dev->driver->pm)
		cb = *(pm_callback_t *)((void *)dev->driver->pm + cb_offset);

	return cb;
}

#define RPM_GET_CALLBACK(dev, callback) \
		__rpm_get_callback(dev, offsetof(struct dev_pm_ops, callback))

static int rpm_resume(struct device *dev, int rpmflags);
static int rpm_suspend(struct device *dev, int rpmflags);

 
static void update_pm_runtime_accounting(struct device *dev)
{
	u64 now, last, delta;

	if (dev->power.disable_depth > 0)
		return;

	last = dev->power.accounting_timestamp;

	now = ktime_get_mono_fast_ns();
	dev->power.accounting_timestamp = now;

	 
	if (now < last)
		return;

	delta = now - last;

	if (dev->power.runtime_status == RPM_SUSPENDED)
		dev->power.suspended_time += delta;
	else
		dev->power.active_time += delta;
}

static void __update_runtime_status(struct device *dev, enum rpm_status status)
{
	update_pm_runtime_accounting(dev);
	dev->power.runtime_status = status;
}

static u64 rpm_get_accounted_time(struct device *dev, bool suspended)
{
	u64 time;
	unsigned long flags;

	spin_lock_irqsave(&dev->power.lock, flags);

	update_pm_runtime_accounting(dev);
	time = suspended ? dev->power.suspended_time : dev->power.active_time;

	spin_unlock_irqrestore(&dev->power.lock, flags);

	return time;
}

u64 pm_runtime_active_time(struct device *dev)
{
	return rpm_get_accounted_time(dev, false);
}

u64 pm_runtime_suspended_time(struct device *dev)
{
	return rpm_get_accounted_time(dev, true);
}
EXPORT_SYMBOL_GPL(pm_runtime_suspended_time);

 
static void pm_runtime_deactivate_timer(struct device *dev)
{
	if (dev->power.timer_expires > 0) {
		hrtimer_try_to_cancel(&dev->power.suspend_timer);
		dev->power.timer_expires = 0;
	}
}

 
static void pm_runtime_cancel_pending(struct device *dev)
{
	pm_runtime_deactivate_timer(dev);
	 
	dev->power.request = RPM_REQ_NONE;
}

 
u64 pm_runtime_autosuspend_expiration(struct device *dev)
{
	int autosuspend_delay;
	u64 expires;

	if (!dev->power.use_autosuspend)
		return 0;

	autosuspend_delay = READ_ONCE(dev->power.autosuspend_delay);
	if (autosuspend_delay < 0)
		return 0;

	expires  = READ_ONCE(dev->power.last_busy);
	expires += (u64)autosuspend_delay * NSEC_PER_MSEC;
	if (expires > ktime_get_mono_fast_ns())
		return expires;	 

	return 0;
}
EXPORT_SYMBOL_GPL(pm_runtime_autosuspend_expiration);

static int dev_memalloc_noio(struct device *dev, void *data)
{
	return dev->power.memalloc_noio;
}

 
void pm_runtime_set_memalloc_noio(struct device *dev, bool enable)
{
	static DEFINE_MUTEX(dev_hotplug_mutex);

	mutex_lock(&dev_hotplug_mutex);
	for (;;) {
		bool enabled;

		 
		spin_lock_irq(&dev->power.lock);
		enabled = dev->power.memalloc_noio;
		dev->power.memalloc_noio = enable;
		spin_unlock_irq(&dev->power.lock);

		 
		if (enabled && enable)
			break;

		dev = dev->parent;

		 
		if (!dev || (!enable &&
		    device_for_each_child(dev, NULL, dev_memalloc_noio)))
			break;
	}
	mutex_unlock(&dev_hotplug_mutex);
}
EXPORT_SYMBOL_GPL(pm_runtime_set_memalloc_noio);

 
static int rpm_check_suspend_allowed(struct device *dev)
{
	int retval = 0;

	if (dev->power.runtime_error)
		retval = -EINVAL;
	else if (dev->power.disable_depth > 0)
		retval = -EACCES;
	else if (atomic_read(&dev->power.usage_count))
		retval = -EAGAIN;
	else if (!dev->power.ignore_children && atomic_read(&dev->power.child_count))
		retval = -EBUSY;

	 
	else if ((dev->power.deferred_resume &&
	    dev->power.runtime_status == RPM_SUSPENDING) ||
	    (dev->power.request_pending && dev->power.request == RPM_REQ_RESUME))
		retval = -EAGAIN;
	else if (__dev_pm_qos_resume_latency(dev) == 0)
		retval = -EPERM;
	else if (dev->power.runtime_status == RPM_SUSPENDED)
		retval = 1;

	return retval;
}

static int rpm_get_suppliers(struct device *dev)
{
	struct device_link *link;

	list_for_each_entry_rcu(link, &dev->links.suppliers, c_node,
				device_links_read_lock_held()) {
		int retval;

		if (!(link->flags & DL_FLAG_PM_RUNTIME))
			continue;

		retval = pm_runtime_get_sync(link->supplier);
		 
		if (retval < 0 && retval != -EACCES) {
			pm_runtime_put_noidle(link->supplier);
			return retval;
		}
		refcount_inc(&link->rpm_active);
	}
	return 0;
}

 
void pm_runtime_release_supplier(struct device_link *link)
{
	struct device *supplier = link->supplier;

	 
	while (refcount_dec_not_one(&link->rpm_active) &&
	       atomic_read(&supplier->power.usage_count) > 0)
		pm_runtime_put_noidle(supplier);
}

static void __rpm_put_suppliers(struct device *dev, bool try_to_suspend)
{
	struct device_link *link;

	list_for_each_entry_rcu(link, &dev->links.suppliers, c_node,
				device_links_read_lock_held()) {
		pm_runtime_release_supplier(link);
		if (try_to_suspend)
			pm_request_idle(link->supplier);
	}
}

static void rpm_put_suppliers(struct device *dev)
{
	__rpm_put_suppliers(dev, true);
}

static void rpm_suspend_suppliers(struct device *dev)
{
	struct device_link *link;
	int idx = device_links_read_lock();

	list_for_each_entry_rcu(link, &dev->links.suppliers, c_node,
				device_links_read_lock_held())
		pm_request_idle(link->supplier);

	device_links_read_unlock(idx);
}

 
static int __rpm_callback(int (*cb)(struct device *), struct device *dev)
	__releases(&dev->power.lock) __acquires(&dev->power.lock)
{
	int retval = 0, idx;
	bool use_links = dev->power.links_count > 0;

	if (dev->power.irq_safe) {
		spin_unlock(&dev->power.lock);
	} else {
		spin_unlock_irq(&dev->power.lock);

		 
		if (use_links && dev->power.runtime_status == RPM_RESUMING) {
			idx = device_links_read_lock();

			retval = rpm_get_suppliers(dev);
			if (retval) {
				rpm_put_suppliers(dev);
				goto fail;
			}

			device_links_read_unlock(idx);
		}
	}

	if (cb)
		retval = cb(dev);

	if (dev->power.irq_safe) {
		spin_lock(&dev->power.lock);
	} else {
		 
		if (use_links &&
		    ((dev->power.runtime_status == RPM_SUSPENDING && !retval) ||
		    (dev->power.runtime_status == RPM_RESUMING && retval))) {
			idx = device_links_read_lock();

			__rpm_put_suppliers(dev, false);

fail:
			device_links_read_unlock(idx);
		}

		spin_lock_irq(&dev->power.lock);
	}

	return retval;
}

 
static int rpm_callback(int (*cb)(struct device *), struct device *dev)
{
	int retval;

	if (dev->power.memalloc_noio) {
		unsigned int noio_flag;

		 
		noio_flag = memalloc_noio_save();
		retval = __rpm_callback(cb, dev);
		memalloc_noio_restore(noio_flag);
	} else {
		retval = __rpm_callback(cb, dev);
	}

	dev->power.runtime_error = retval;
	return retval != -EACCES ? retval : -EIO;
}

 
static int rpm_idle(struct device *dev, int rpmflags)
{
	int (*callback)(struct device *);
	int retval;

	trace_rpm_idle(dev, rpmflags);
	retval = rpm_check_suspend_allowed(dev);
	if (retval < 0)
		;	 

	 
	else if (dev->power.runtime_status != RPM_ACTIVE)
		retval = -EAGAIN;

	 
	else if (dev->power.request_pending &&
	    dev->power.request > RPM_REQ_IDLE)
		retval = -EAGAIN;

	 
	else if (dev->power.idle_notification)
		retval = -EINPROGRESS;

	if (retval)
		goto out;

	 
	dev->power.request = RPM_REQ_NONE;

	callback = RPM_GET_CALLBACK(dev, runtime_idle);

	 
	if (!callback || dev->power.no_callbacks)
		goto out;

	 
	if (rpmflags & RPM_ASYNC) {
		dev->power.request = RPM_REQ_IDLE;
		if (!dev->power.request_pending) {
			dev->power.request_pending = true;
			queue_work(pm_wq, &dev->power.work);
		}
		trace_rpm_return_int(dev, _THIS_IP_, 0);
		return 0;
	}

	dev->power.idle_notification = true;

	if (dev->power.irq_safe)
		spin_unlock(&dev->power.lock);
	else
		spin_unlock_irq(&dev->power.lock);

	retval = callback(dev);

	if (dev->power.irq_safe)
		spin_lock(&dev->power.lock);
	else
		spin_lock_irq(&dev->power.lock);

	dev->power.idle_notification = false;
	wake_up_all(&dev->power.wait_queue);

 out:
	trace_rpm_return_int(dev, _THIS_IP_, retval);
	return retval ? retval : rpm_suspend(dev, rpmflags | RPM_AUTO);
}

 
static int rpm_suspend(struct device *dev, int rpmflags)
	__releases(&dev->power.lock) __acquires(&dev->power.lock)
{
	int (*callback)(struct device *);
	struct device *parent = NULL;
	int retval;

	trace_rpm_suspend(dev, rpmflags);

 repeat:
	retval = rpm_check_suspend_allowed(dev);
	if (retval < 0)
		goto out;	 

	 
	if (dev->power.runtime_status == RPM_RESUMING && !(rpmflags & RPM_ASYNC))
		retval = -EAGAIN;

	if (retval)
		goto out;

	 
	if ((rpmflags & RPM_AUTO) && dev->power.runtime_status != RPM_SUSPENDING) {
		u64 expires = pm_runtime_autosuspend_expiration(dev);

		if (expires != 0) {
			 
			dev->power.request = RPM_REQ_NONE;

			 
			if (!(dev->power.timer_expires &&
			    dev->power.timer_expires <= expires)) {
				 
				u64 slack = (u64)READ_ONCE(dev->power.autosuspend_delay) *
						    (NSEC_PER_MSEC >> 2);

				dev->power.timer_expires = expires;
				hrtimer_start_range_ns(&dev->power.suspend_timer,
						       ns_to_ktime(expires),
						       slack,
						       HRTIMER_MODE_ABS);
			}
			dev->power.timer_autosuspends = 1;
			goto out;
		}
	}

	 
	pm_runtime_cancel_pending(dev);

	if (dev->power.runtime_status == RPM_SUSPENDING) {
		DEFINE_WAIT(wait);

		if (rpmflags & (RPM_ASYNC | RPM_NOWAIT)) {
			retval = -EINPROGRESS;
			goto out;
		}

		if (dev->power.irq_safe) {
			spin_unlock(&dev->power.lock);

			cpu_relax();

			spin_lock(&dev->power.lock);
			goto repeat;
		}

		 
		for (;;) {
			prepare_to_wait(&dev->power.wait_queue, &wait,
					TASK_UNINTERRUPTIBLE);
			if (dev->power.runtime_status != RPM_SUSPENDING)
				break;

			spin_unlock_irq(&dev->power.lock);

			schedule();

			spin_lock_irq(&dev->power.lock);
		}
		finish_wait(&dev->power.wait_queue, &wait);
		goto repeat;
	}

	if (dev->power.no_callbacks)
		goto no_callback;	 

	 
	if (rpmflags & RPM_ASYNC) {
		dev->power.request = (rpmflags & RPM_AUTO) ?
		    RPM_REQ_AUTOSUSPEND : RPM_REQ_SUSPEND;
		if (!dev->power.request_pending) {
			dev->power.request_pending = true;
			queue_work(pm_wq, &dev->power.work);
		}
		goto out;
	}

	__update_runtime_status(dev, RPM_SUSPENDING);

	callback = RPM_GET_CALLBACK(dev, runtime_suspend);

	dev_pm_enable_wake_irq_check(dev, true);
	retval = rpm_callback(callback, dev);
	if (retval)
		goto fail;

	dev_pm_enable_wake_irq_complete(dev);

 no_callback:
	__update_runtime_status(dev, RPM_SUSPENDED);
	pm_runtime_deactivate_timer(dev);

	if (dev->parent) {
		parent = dev->parent;
		atomic_add_unless(&parent->power.child_count, -1, 0);
	}
	wake_up_all(&dev->power.wait_queue);

	if (dev->power.deferred_resume) {
		dev->power.deferred_resume = false;
		rpm_resume(dev, 0);
		retval = -EAGAIN;
		goto out;
	}

	if (dev->power.irq_safe)
		goto out;

	 
	if (parent && !parent->power.ignore_children) {
		spin_unlock(&dev->power.lock);

		spin_lock(&parent->power.lock);
		rpm_idle(parent, RPM_ASYNC);
		spin_unlock(&parent->power.lock);

		spin_lock(&dev->power.lock);
	}
	 
	if (dev->power.links_count > 0) {
		spin_unlock_irq(&dev->power.lock);

		rpm_suspend_suppliers(dev);

		spin_lock_irq(&dev->power.lock);
	}

 out:
	trace_rpm_return_int(dev, _THIS_IP_, retval);

	return retval;

 fail:
	dev_pm_disable_wake_irq_check(dev, true);
	__update_runtime_status(dev, RPM_ACTIVE);
	dev->power.deferred_resume = false;
	wake_up_all(&dev->power.wait_queue);

	if (retval == -EAGAIN || retval == -EBUSY) {
		dev->power.runtime_error = 0;

		 
		if ((rpmflags & RPM_AUTO) &&
		    pm_runtime_autosuspend_expiration(dev) != 0)
			goto repeat;
	} else {
		pm_runtime_cancel_pending(dev);
	}
	goto out;
}

 
static int rpm_resume(struct device *dev, int rpmflags)
	__releases(&dev->power.lock) __acquires(&dev->power.lock)
{
	int (*callback)(struct device *);
	struct device *parent = NULL;
	int retval = 0;

	trace_rpm_resume(dev, rpmflags);

 repeat:
	if (dev->power.runtime_error) {
		retval = -EINVAL;
	} else if (dev->power.disable_depth > 0) {
		if (dev->power.runtime_status == RPM_ACTIVE &&
		    dev->power.last_status == RPM_ACTIVE)
			retval = 1;
		else
			retval = -EACCES;
	}
	if (retval)
		goto out;

	 
	dev->power.request = RPM_REQ_NONE;
	if (!dev->power.timer_autosuspends)
		pm_runtime_deactivate_timer(dev);

	if (dev->power.runtime_status == RPM_ACTIVE) {
		retval = 1;
		goto out;
	}

	if (dev->power.runtime_status == RPM_RESUMING ||
	    dev->power.runtime_status == RPM_SUSPENDING) {
		DEFINE_WAIT(wait);

		if (rpmflags & (RPM_ASYNC | RPM_NOWAIT)) {
			if (dev->power.runtime_status == RPM_SUSPENDING) {
				dev->power.deferred_resume = true;
				if (rpmflags & RPM_NOWAIT)
					retval = -EINPROGRESS;
			} else {
				retval = -EINPROGRESS;
			}
			goto out;
		}

		if (dev->power.irq_safe) {
			spin_unlock(&dev->power.lock);

			cpu_relax();

			spin_lock(&dev->power.lock);
			goto repeat;
		}

		 
		for (;;) {
			prepare_to_wait(&dev->power.wait_queue, &wait,
					TASK_UNINTERRUPTIBLE);
			if (dev->power.runtime_status != RPM_RESUMING &&
			    dev->power.runtime_status != RPM_SUSPENDING)
				break;

			spin_unlock_irq(&dev->power.lock);

			schedule();

			spin_lock_irq(&dev->power.lock);
		}
		finish_wait(&dev->power.wait_queue, &wait);
		goto repeat;
	}

	 
	if (dev->power.no_callbacks && !parent && dev->parent) {
		spin_lock_nested(&dev->parent->power.lock, SINGLE_DEPTH_NESTING);
		if (dev->parent->power.disable_depth > 0 ||
		    dev->parent->power.ignore_children ||
		    dev->parent->power.runtime_status == RPM_ACTIVE) {
			atomic_inc(&dev->parent->power.child_count);
			spin_unlock(&dev->parent->power.lock);
			retval = 1;
			goto no_callback;	 
		}
		spin_unlock(&dev->parent->power.lock);
	}

	 
	if (rpmflags & RPM_ASYNC) {
		dev->power.request = RPM_REQ_RESUME;
		if (!dev->power.request_pending) {
			dev->power.request_pending = true;
			queue_work(pm_wq, &dev->power.work);
		}
		retval = 0;
		goto out;
	}

	if (!parent && dev->parent) {
		 
		parent = dev->parent;
		if (dev->power.irq_safe)
			goto skip_parent;

		spin_unlock(&dev->power.lock);

		pm_runtime_get_noresume(parent);

		spin_lock(&parent->power.lock);
		 
		if (!parent->power.disable_depth &&
		    !parent->power.ignore_children) {
			rpm_resume(parent, 0);
			if (parent->power.runtime_status != RPM_ACTIVE)
				retval = -EBUSY;
		}
		spin_unlock(&parent->power.lock);

		spin_lock(&dev->power.lock);
		if (retval)
			goto out;

		goto repeat;
	}
 skip_parent:

	if (dev->power.no_callbacks)
		goto no_callback;	 

	__update_runtime_status(dev, RPM_RESUMING);

	callback = RPM_GET_CALLBACK(dev, runtime_resume);

	dev_pm_disable_wake_irq_check(dev, false);
	retval = rpm_callback(callback, dev);
	if (retval) {
		__update_runtime_status(dev, RPM_SUSPENDED);
		pm_runtime_cancel_pending(dev);
		dev_pm_enable_wake_irq_check(dev, false);
	} else {
 no_callback:
		__update_runtime_status(dev, RPM_ACTIVE);
		pm_runtime_mark_last_busy(dev);
		if (parent)
			atomic_inc(&parent->power.child_count);
	}
	wake_up_all(&dev->power.wait_queue);

	if (retval >= 0)
		rpm_idle(dev, RPM_ASYNC);

 out:
	if (parent && !dev->power.irq_safe) {
		spin_unlock_irq(&dev->power.lock);

		pm_runtime_put(parent);

		spin_lock_irq(&dev->power.lock);
	}

	trace_rpm_return_int(dev, _THIS_IP_, retval);

	return retval;
}

 
static void pm_runtime_work(struct work_struct *work)
{
	struct device *dev = container_of(work, struct device, power.work);
	enum rpm_request req;

	spin_lock_irq(&dev->power.lock);

	if (!dev->power.request_pending)
		goto out;

	req = dev->power.request;
	dev->power.request = RPM_REQ_NONE;
	dev->power.request_pending = false;

	switch (req) {
	case RPM_REQ_NONE:
		break;
	case RPM_REQ_IDLE:
		rpm_idle(dev, RPM_NOWAIT);
		break;
	case RPM_REQ_SUSPEND:
		rpm_suspend(dev, RPM_NOWAIT);
		break;
	case RPM_REQ_AUTOSUSPEND:
		rpm_suspend(dev, RPM_NOWAIT | RPM_AUTO);
		break;
	case RPM_REQ_RESUME:
		rpm_resume(dev, RPM_NOWAIT);
		break;
	}

 out:
	spin_unlock_irq(&dev->power.lock);
}

 
static enum hrtimer_restart  pm_suspend_timer_fn(struct hrtimer *timer)
{
	struct device *dev = container_of(timer, struct device, power.suspend_timer);
	unsigned long flags;
	u64 expires;

	spin_lock_irqsave(&dev->power.lock, flags);

	expires = dev->power.timer_expires;
	 
	if (expires > 0 && expires < ktime_get_mono_fast_ns()) {
		dev->power.timer_expires = 0;
		rpm_suspend(dev, dev->power.timer_autosuspends ?
		    (RPM_ASYNC | RPM_AUTO) : RPM_ASYNC);
	}

	spin_unlock_irqrestore(&dev->power.lock, flags);

	return HRTIMER_NORESTART;
}

 
int pm_schedule_suspend(struct device *dev, unsigned int delay)
{
	unsigned long flags;
	u64 expires;
	int retval;

	spin_lock_irqsave(&dev->power.lock, flags);

	if (!delay) {
		retval = rpm_suspend(dev, RPM_ASYNC);
		goto out;
	}

	retval = rpm_check_suspend_allowed(dev);
	if (retval)
		goto out;

	 
	pm_runtime_cancel_pending(dev);

	expires = ktime_get_mono_fast_ns() + (u64)delay * NSEC_PER_MSEC;
	dev->power.timer_expires = expires;
	dev->power.timer_autosuspends = 0;
	hrtimer_start(&dev->power.suspend_timer, expires, HRTIMER_MODE_ABS);

 out:
	spin_unlock_irqrestore(&dev->power.lock, flags);

	return retval;
}
EXPORT_SYMBOL_GPL(pm_schedule_suspend);

static int rpm_drop_usage_count(struct device *dev)
{
	int ret;

	ret = atomic_sub_return(1, &dev->power.usage_count);
	if (ret >= 0)
		return ret;

	 
	atomic_inc(&dev->power.usage_count);
	dev_warn(dev, "Runtime PM usage count underflow!\n");
	return -EINVAL;
}

 
int __pm_runtime_idle(struct device *dev, int rpmflags)
{
	unsigned long flags;
	int retval;

	if (rpmflags & RPM_GET_PUT) {
		retval = rpm_drop_usage_count(dev);
		if (retval < 0) {
			return retval;
		} else if (retval > 0) {
			trace_rpm_usage(dev, rpmflags);
			return 0;
		}
	}

	might_sleep_if(!(rpmflags & RPM_ASYNC) && !dev->power.irq_safe);

	spin_lock_irqsave(&dev->power.lock, flags);
	retval = rpm_idle(dev, rpmflags);
	spin_unlock_irqrestore(&dev->power.lock, flags);

	return retval;
}
EXPORT_SYMBOL_GPL(__pm_runtime_idle);

 
int __pm_runtime_suspend(struct device *dev, int rpmflags)
{
	unsigned long flags;
	int retval;

	if (rpmflags & RPM_GET_PUT) {
		retval = rpm_drop_usage_count(dev);
		if (retval < 0) {
			return retval;
		} else if (retval > 0) {
			trace_rpm_usage(dev, rpmflags);
			return 0;
		}
	}

	might_sleep_if(!(rpmflags & RPM_ASYNC) && !dev->power.irq_safe);

	spin_lock_irqsave(&dev->power.lock, flags);
	retval = rpm_suspend(dev, rpmflags);
	spin_unlock_irqrestore(&dev->power.lock, flags);

	return retval;
}
EXPORT_SYMBOL_GPL(__pm_runtime_suspend);

 
int __pm_runtime_resume(struct device *dev, int rpmflags)
{
	unsigned long flags;
	int retval;

	might_sleep_if(!(rpmflags & RPM_ASYNC) && !dev->power.irq_safe &&
			dev->power.runtime_status != RPM_ACTIVE);

	if (rpmflags & RPM_GET_PUT)
		atomic_inc(&dev->power.usage_count);

	spin_lock_irqsave(&dev->power.lock, flags);
	retval = rpm_resume(dev, rpmflags);
	spin_unlock_irqrestore(&dev->power.lock, flags);

	return retval;
}
EXPORT_SYMBOL_GPL(__pm_runtime_resume);

 
int pm_runtime_get_if_active(struct device *dev, bool ign_usage_count)
{
	unsigned long flags;
	int retval;

	spin_lock_irqsave(&dev->power.lock, flags);
	if (dev->power.disable_depth > 0) {
		retval = -EINVAL;
	} else if (dev->power.runtime_status != RPM_ACTIVE) {
		retval = 0;
	} else if (ign_usage_count) {
		retval = 1;
		atomic_inc(&dev->power.usage_count);
	} else {
		retval = atomic_inc_not_zero(&dev->power.usage_count);
	}
	trace_rpm_usage(dev, 0);
	spin_unlock_irqrestore(&dev->power.lock, flags);

	return retval;
}
EXPORT_SYMBOL_GPL(pm_runtime_get_if_active);

 
int __pm_runtime_set_status(struct device *dev, unsigned int status)
{
	struct device *parent = dev->parent;
	bool notify_parent = false;
	unsigned long flags;
	int error = 0;

	if (status != RPM_ACTIVE && status != RPM_SUSPENDED)
		return -EINVAL;

	spin_lock_irqsave(&dev->power.lock, flags);

	 
	if (dev->power.runtime_error || dev->power.disable_depth)
		dev->power.disable_depth++;
	else
		error = -EAGAIN;

	spin_unlock_irqrestore(&dev->power.lock, flags);

	if (error)
		return error;

	 
	if (status == RPM_ACTIVE) {
		int idx = device_links_read_lock();

		error = rpm_get_suppliers(dev);
		if (error)
			status = RPM_SUSPENDED;

		device_links_read_unlock(idx);
	}

	spin_lock_irqsave(&dev->power.lock, flags);

	if (dev->power.runtime_status == status || !parent)
		goto out_set;

	if (status == RPM_SUSPENDED) {
		atomic_add_unless(&parent->power.child_count, -1, 0);
		notify_parent = !parent->power.ignore_children;
	} else {
		spin_lock_nested(&parent->power.lock, SINGLE_DEPTH_NESTING);

		 
		if (!parent->power.disable_depth &&
		    !parent->power.ignore_children &&
		    parent->power.runtime_status != RPM_ACTIVE) {
			dev_err(dev, "runtime PM trying to activate child device %s but parent (%s) is not active\n",
				dev_name(dev),
				dev_name(parent));
			error = -EBUSY;
		} else if (dev->power.runtime_status == RPM_SUSPENDED) {
			atomic_inc(&parent->power.child_count);
		}

		spin_unlock(&parent->power.lock);

		if (error) {
			status = RPM_SUSPENDED;
			goto out;
		}
	}

 out_set:
	__update_runtime_status(dev, status);
	if (!error)
		dev->power.runtime_error = 0;

 out:
	spin_unlock_irqrestore(&dev->power.lock, flags);

	if (notify_parent)
		pm_request_idle(parent);

	if (status == RPM_SUSPENDED) {
		int idx = device_links_read_lock();

		rpm_put_suppliers(dev);

		device_links_read_unlock(idx);
	}

	pm_runtime_enable(dev);

	return error;
}
EXPORT_SYMBOL_GPL(__pm_runtime_set_status);

 
static void __pm_runtime_barrier(struct device *dev)
{
	pm_runtime_deactivate_timer(dev);

	if (dev->power.request_pending) {
		dev->power.request = RPM_REQ_NONE;
		spin_unlock_irq(&dev->power.lock);

		cancel_work_sync(&dev->power.work);

		spin_lock_irq(&dev->power.lock);
		dev->power.request_pending = false;
	}

	if (dev->power.runtime_status == RPM_SUSPENDING ||
	    dev->power.runtime_status == RPM_RESUMING ||
	    dev->power.idle_notification) {
		DEFINE_WAIT(wait);

		 
		for (;;) {
			prepare_to_wait(&dev->power.wait_queue, &wait,
					TASK_UNINTERRUPTIBLE);
			if (dev->power.runtime_status != RPM_SUSPENDING
			    && dev->power.runtime_status != RPM_RESUMING
			    && !dev->power.idle_notification)
				break;
			spin_unlock_irq(&dev->power.lock);

			schedule();

			spin_lock_irq(&dev->power.lock);
		}
		finish_wait(&dev->power.wait_queue, &wait);
	}
}

 
int pm_runtime_barrier(struct device *dev)
{
	int retval = 0;

	pm_runtime_get_noresume(dev);
	spin_lock_irq(&dev->power.lock);

	if (dev->power.request_pending
	    && dev->power.request == RPM_REQ_RESUME) {
		rpm_resume(dev, 0);
		retval = 1;
	}

	__pm_runtime_barrier(dev);

	spin_unlock_irq(&dev->power.lock);
	pm_runtime_put_noidle(dev);

	return retval;
}
EXPORT_SYMBOL_GPL(pm_runtime_barrier);

 
void __pm_runtime_disable(struct device *dev, bool check_resume)
{
	spin_lock_irq(&dev->power.lock);

	if (dev->power.disable_depth > 0) {
		dev->power.disable_depth++;
		goto out;
	}

	 
	if (check_resume && dev->power.request_pending &&
	    dev->power.request == RPM_REQ_RESUME) {
		 
		pm_runtime_get_noresume(dev);

		rpm_resume(dev, 0);

		pm_runtime_put_noidle(dev);
	}

	 
	update_pm_runtime_accounting(dev);

	if (!dev->power.disable_depth++) {
		__pm_runtime_barrier(dev);
		dev->power.last_status = dev->power.runtime_status;
	}

 out:
	spin_unlock_irq(&dev->power.lock);
}
EXPORT_SYMBOL_GPL(__pm_runtime_disable);

 
void pm_runtime_enable(struct device *dev)
{
	unsigned long flags;

	spin_lock_irqsave(&dev->power.lock, flags);

	if (!dev->power.disable_depth) {
		dev_warn(dev, "Unbalanced %s!\n", __func__);
		goto out;
	}

	if (--dev->power.disable_depth > 0)
		goto out;

	dev->power.last_status = RPM_INVALID;
	dev->power.accounting_timestamp = ktime_get_mono_fast_ns();

	if (dev->power.runtime_status == RPM_SUSPENDED &&
	    !dev->power.ignore_children &&
	    atomic_read(&dev->power.child_count) > 0)
		dev_warn(dev, "Enabling runtime PM for inactive device with active children\n");

out:
	spin_unlock_irqrestore(&dev->power.lock, flags);
}
EXPORT_SYMBOL_GPL(pm_runtime_enable);

static void pm_runtime_disable_action(void *data)
{
	pm_runtime_dont_use_autosuspend(data);
	pm_runtime_disable(data);
}

 
int devm_pm_runtime_enable(struct device *dev)
{
	pm_runtime_enable(dev);

	return devm_add_action_or_reset(dev, pm_runtime_disable_action, dev);
}
EXPORT_SYMBOL_GPL(devm_pm_runtime_enable);

 
void pm_runtime_forbid(struct device *dev)
{
	spin_lock_irq(&dev->power.lock);
	if (!dev->power.runtime_auto)
		goto out;

	dev->power.runtime_auto = false;
	atomic_inc(&dev->power.usage_count);
	rpm_resume(dev, 0);

 out:
	spin_unlock_irq(&dev->power.lock);
}
EXPORT_SYMBOL_GPL(pm_runtime_forbid);

 
void pm_runtime_allow(struct device *dev)
{
	int ret;

	spin_lock_irq(&dev->power.lock);
	if (dev->power.runtime_auto)
		goto out;

	dev->power.runtime_auto = true;
	ret = rpm_drop_usage_count(dev);
	if (ret == 0)
		rpm_idle(dev, RPM_AUTO | RPM_ASYNC);
	else if (ret > 0)
		trace_rpm_usage(dev, RPM_AUTO | RPM_ASYNC);

 out:
	spin_unlock_irq(&dev->power.lock);
}
EXPORT_SYMBOL_GPL(pm_runtime_allow);

 
void pm_runtime_no_callbacks(struct device *dev)
{
	spin_lock_irq(&dev->power.lock);
	dev->power.no_callbacks = 1;
	spin_unlock_irq(&dev->power.lock);
	if (device_is_registered(dev))
		rpm_sysfs_remove(dev);
}
EXPORT_SYMBOL_GPL(pm_runtime_no_callbacks);

 
void pm_runtime_irq_safe(struct device *dev)
{
	if (dev->parent)
		pm_runtime_get_sync(dev->parent);

	spin_lock_irq(&dev->power.lock);
	dev->power.irq_safe = 1;
	spin_unlock_irq(&dev->power.lock);
}
EXPORT_SYMBOL_GPL(pm_runtime_irq_safe);

 
static void update_autosuspend(struct device *dev, int old_delay, int old_use)
{
	int delay = dev->power.autosuspend_delay;

	 
	if (dev->power.use_autosuspend && delay < 0) {

		 
		if (!old_use || old_delay >= 0) {
			atomic_inc(&dev->power.usage_count);
			rpm_resume(dev, 0);
		} else {
			trace_rpm_usage(dev, 0);
		}
	}

	 
	else {

		 
		if (old_use && old_delay < 0)
			atomic_dec(&dev->power.usage_count);

		 
		rpm_idle(dev, RPM_AUTO);
	}
}

 
void pm_runtime_set_autosuspend_delay(struct device *dev, int delay)
{
	int old_delay, old_use;

	spin_lock_irq(&dev->power.lock);
	old_delay = dev->power.autosuspend_delay;
	old_use = dev->power.use_autosuspend;
	dev->power.autosuspend_delay = delay;
	update_autosuspend(dev, old_delay, old_use);
	spin_unlock_irq(&dev->power.lock);
}
EXPORT_SYMBOL_GPL(pm_runtime_set_autosuspend_delay);

 
void __pm_runtime_use_autosuspend(struct device *dev, bool use)
{
	int old_delay, old_use;

	spin_lock_irq(&dev->power.lock);
	old_delay = dev->power.autosuspend_delay;
	old_use = dev->power.use_autosuspend;
	dev->power.use_autosuspend = use;
	update_autosuspend(dev, old_delay, old_use);
	spin_unlock_irq(&dev->power.lock);
}
EXPORT_SYMBOL_GPL(__pm_runtime_use_autosuspend);

 
void pm_runtime_init(struct device *dev)
{
	dev->power.runtime_status = RPM_SUSPENDED;
	dev->power.last_status = RPM_INVALID;
	dev->power.idle_notification = false;

	dev->power.disable_depth = 1;
	atomic_set(&dev->power.usage_count, 0);

	dev->power.runtime_error = 0;

	atomic_set(&dev->power.child_count, 0);
	pm_suspend_ignore_children(dev, false);
	dev->power.runtime_auto = true;

	dev->power.request_pending = false;
	dev->power.request = RPM_REQ_NONE;
	dev->power.deferred_resume = false;
	dev->power.needs_force_resume = 0;
	INIT_WORK(&dev->power.work, pm_runtime_work);

	dev->power.timer_expires = 0;
	hrtimer_init(&dev->power.suspend_timer, CLOCK_MONOTONIC, HRTIMER_MODE_ABS);
	dev->power.suspend_timer.function = pm_suspend_timer_fn;

	init_waitqueue_head(&dev->power.wait_queue);
}

 
void pm_runtime_reinit(struct device *dev)
{
	if (!pm_runtime_enabled(dev)) {
		if (dev->power.runtime_status == RPM_ACTIVE)
			pm_runtime_set_suspended(dev);
		if (dev->power.irq_safe) {
			spin_lock_irq(&dev->power.lock);
			dev->power.irq_safe = 0;
			spin_unlock_irq(&dev->power.lock);
			if (dev->parent)
				pm_runtime_put(dev->parent);
		}
	}
}

 
void pm_runtime_remove(struct device *dev)
{
	__pm_runtime_disable(dev, false);
	pm_runtime_reinit(dev);
}

 
void pm_runtime_get_suppliers(struct device *dev)
{
	struct device_link *link;
	int idx;

	idx = device_links_read_lock();

	list_for_each_entry_rcu(link, &dev->links.suppliers, c_node,
				device_links_read_lock_held())
		if (link->flags & DL_FLAG_PM_RUNTIME) {
			link->supplier_preactivated = true;
			pm_runtime_get_sync(link->supplier);
		}

	device_links_read_unlock(idx);
}

 
void pm_runtime_put_suppliers(struct device *dev)
{
	struct device_link *link;
	int idx;

	idx = device_links_read_lock();

	list_for_each_entry_rcu(link, &dev->links.suppliers, c_node,
				device_links_read_lock_held())
		if (link->supplier_preactivated) {
			link->supplier_preactivated = false;
			pm_runtime_put(link->supplier);
		}

	device_links_read_unlock(idx);
}

void pm_runtime_new_link(struct device *dev)
{
	spin_lock_irq(&dev->power.lock);
	dev->power.links_count++;
	spin_unlock_irq(&dev->power.lock);
}

static void pm_runtime_drop_link_count(struct device *dev)
{
	spin_lock_irq(&dev->power.lock);
	WARN_ON(dev->power.links_count == 0);
	dev->power.links_count--;
	spin_unlock_irq(&dev->power.lock);
}

 
void pm_runtime_drop_link(struct device_link *link)
{
	if (!(link->flags & DL_FLAG_PM_RUNTIME))
		return;

	pm_runtime_drop_link_count(link->consumer);
	pm_runtime_release_supplier(link);
	pm_request_idle(link->supplier);
}

static bool pm_runtime_need_not_resume(struct device *dev)
{
	return atomic_read(&dev->power.usage_count) <= 1 &&
		(atomic_read(&dev->power.child_count) == 0 ||
		 dev->power.ignore_children);
}

 
int pm_runtime_force_suspend(struct device *dev)
{
	int (*callback)(struct device *);
	int ret;

	pm_runtime_disable(dev);
	if (pm_runtime_status_suspended(dev))
		return 0;

	callback = RPM_GET_CALLBACK(dev, runtime_suspend);

	dev_pm_enable_wake_irq_check(dev, true);
	ret = callback ? callback(dev) : 0;
	if (ret)
		goto err;

	dev_pm_enable_wake_irq_complete(dev);

	 
	if (pm_runtime_need_not_resume(dev)) {
		pm_runtime_set_suspended(dev);
	} else {
		__update_runtime_status(dev, RPM_SUSPENDED);
		dev->power.needs_force_resume = 1;
	}

	return 0;

err:
	dev_pm_disable_wake_irq_check(dev, true);
	pm_runtime_enable(dev);
	return ret;
}
EXPORT_SYMBOL_GPL(pm_runtime_force_suspend);

 
int pm_runtime_force_resume(struct device *dev)
{
	int (*callback)(struct device *);
	int ret = 0;

	if (!pm_runtime_status_suspended(dev) || !dev->power.needs_force_resume)
		goto out;

	 
	__update_runtime_status(dev, RPM_ACTIVE);

	callback = RPM_GET_CALLBACK(dev, runtime_resume);

	dev_pm_disable_wake_irq_check(dev, false);
	ret = callback ? callback(dev) : 0;
	if (ret) {
		pm_runtime_set_suspended(dev);
		dev_pm_enable_wake_irq_check(dev, false);
		goto out;
	}

	pm_runtime_mark_last_busy(dev);
out:
	dev->power.needs_force_resume = 0;
	pm_runtime_enable(dev);
	return ret;
}
EXPORT_SYMBOL_GPL(pm_runtime_force_resume);
