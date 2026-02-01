
 

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/module.h>
#include <linux/of.h>
#include <linux/rtc.h>
#include <linux/kdev_t.h>
#include <linux/idr.h>
#include <linux/slab.h>
#include <linux/workqueue.h>

#include "rtc-core.h"

static DEFINE_IDA(rtc_ida);
struct class *rtc_class;

static void rtc_device_release(struct device *dev)
{
	struct rtc_device *rtc = to_rtc_device(dev);
	struct timerqueue_head *head = &rtc->timerqueue;
	struct timerqueue_node *node;

	mutex_lock(&rtc->ops_lock);
	while ((node = timerqueue_getnext(head)))
		timerqueue_del(head, node);
	mutex_unlock(&rtc->ops_lock);

	cancel_work_sync(&rtc->irqwork);

	ida_free(&rtc_ida, rtc->id);
	mutex_destroy(&rtc->ops_lock);
	kfree(rtc);
}

#ifdef CONFIG_RTC_HCTOSYS_DEVICE
 
int rtc_hctosys_ret = -ENODEV;

 

static void rtc_hctosys(struct rtc_device *rtc)
{
	int err;
	struct rtc_time tm;
	struct timespec64 tv64 = {
		.tv_nsec = NSEC_PER_SEC >> 1,
	};

	err = rtc_read_time(rtc, &tm);
	if (err) {
		dev_err(rtc->dev.parent,
			"hctosys: unable to read the hardware clock\n");
		goto err_read;
	}

	tv64.tv_sec = rtc_tm_to_time64(&tm);

#if BITS_PER_LONG == 32
	if (tv64.tv_sec > INT_MAX) {
		err = -ERANGE;
		goto err_read;
	}
#endif

	err = do_settimeofday64(&tv64);

	dev_info(rtc->dev.parent, "setting system clock to %ptR UTC (%lld)\n",
		 &tm, (long long)tv64.tv_sec);

err_read:
	rtc_hctosys_ret = err;
}
#endif

#if defined(CONFIG_PM_SLEEP) && defined(CONFIG_RTC_HCTOSYS_DEVICE)
 

static struct timespec64 old_rtc, old_system, old_delta;

static int rtc_suspend(struct device *dev)
{
	struct rtc_device	*rtc = to_rtc_device(dev);
	struct rtc_time		tm;
	struct timespec64	delta, delta_delta;
	int err;

	if (timekeeping_rtc_skipsuspend())
		return 0;

	if (strcmp(dev_name(&rtc->dev), CONFIG_RTC_HCTOSYS_DEVICE) != 0)
		return 0;

	 
	err = rtc_read_time(rtc, &tm);
	if (err < 0) {
		pr_debug("%s:  fail to read rtc time\n", dev_name(&rtc->dev));
		return 0;
	}

	ktime_get_real_ts64(&old_system);
	old_rtc.tv_sec = rtc_tm_to_time64(&tm);

	 
	delta = timespec64_sub(old_system, old_rtc);
	delta_delta = timespec64_sub(delta, old_delta);
	if (delta_delta.tv_sec < -2 || delta_delta.tv_sec >= 2) {
		 
		old_delta = delta;
	} else {
		 
		old_system = timespec64_sub(old_system, delta_delta);
	}

	return 0;
}

static int rtc_resume(struct device *dev)
{
	struct rtc_device	*rtc = to_rtc_device(dev);
	struct rtc_time		tm;
	struct timespec64	new_system, new_rtc;
	struct timespec64	sleep_time;
	int err;

	if (timekeeping_rtc_skipresume())
		return 0;

	rtc_hctosys_ret = -ENODEV;
	if (strcmp(dev_name(&rtc->dev), CONFIG_RTC_HCTOSYS_DEVICE) != 0)
		return 0;

	 
	ktime_get_real_ts64(&new_system);
	err = rtc_read_time(rtc, &tm);
	if (err < 0) {
		pr_debug("%s:  fail to read rtc time\n", dev_name(&rtc->dev));
		return 0;
	}

	new_rtc.tv_sec = rtc_tm_to_time64(&tm);
	new_rtc.tv_nsec = 0;

	if (new_rtc.tv_sec < old_rtc.tv_sec) {
		pr_debug("%s:  time travel!\n", dev_name(&rtc->dev));
		return 0;
	}

	 
	sleep_time = timespec64_sub(new_rtc, old_rtc);

	 
	sleep_time = timespec64_sub(sleep_time,
				    timespec64_sub(new_system, old_system));

	if (sleep_time.tv_sec >= 0)
		timekeeping_inject_sleeptime64(&sleep_time);
	rtc_hctosys_ret = 0;
	return 0;
}

static SIMPLE_DEV_PM_OPS(rtc_class_dev_pm_ops, rtc_suspend, rtc_resume);
#define RTC_CLASS_DEV_PM_OPS	(&rtc_class_dev_pm_ops)
#else
#define RTC_CLASS_DEV_PM_OPS	NULL
#endif

 
static struct rtc_device *rtc_allocate_device(void)
{
	struct rtc_device *rtc;

	rtc = kzalloc(sizeof(*rtc), GFP_KERNEL);
	if (!rtc)
		return NULL;

	device_initialize(&rtc->dev);

	 
	rtc->set_offset_nsec = NSEC_PER_SEC + 5 * NSEC_PER_MSEC;

	rtc->irq_freq = 1;
	rtc->max_user_freq = 64;
	rtc->dev.class = rtc_class;
	rtc->dev.groups = rtc_get_dev_attribute_groups();
	rtc->dev.release = rtc_device_release;

	mutex_init(&rtc->ops_lock);
	spin_lock_init(&rtc->irq_lock);
	init_waitqueue_head(&rtc->irq_queue);

	 
	timerqueue_init_head(&rtc->timerqueue);
	INIT_WORK(&rtc->irqwork, rtc_timer_do_work);
	 
	rtc_timer_init(&rtc->aie_timer, rtc_aie_update_irq, rtc);
	 
	rtc_timer_init(&rtc->uie_rtctimer, rtc_uie_update_irq, rtc);
	 
	hrtimer_init(&rtc->pie_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	rtc->pie_timer.function = rtc_pie_update_irq;
	rtc->pie_enabled = 0;

	set_bit(RTC_FEATURE_ALARM, rtc->features);
	set_bit(RTC_FEATURE_UPDATE_INTERRUPT, rtc->features);

	return rtc;
}

static int rtc_device_get_id(struct device *dev)
{
	int of_id = -1, id = -1;

	if (dev->of_node)
		of_id = of_alias_get_id(dev->of_node, "rtc");
	else if (dev->parent && dev->parent->of_node)
		of_id = of_alias_get_id(dev->parent->of_node, "rtc");

	if (of_id >= 0) {
		id = ida_simple_get(&rtc_ida, of_id, of_id + 1, GFP_KERNEL);
		if (id < 0)
			dev_warn(dev, "/aliases ID %d not available\n", of_id);
	}

	if (id < 0)
		id = ida_alloc(&rtc_ida, GFP_KERNEL);

	return id;
}

static void rtc_device_get_offset(struct rtc_device *rtc)
{
	time64_t range_secs;
	u32 start_year;
	int ret;

	 
	if (rtc->range_min == rtc->range_max)
		return;

	ret = device_property_read_u32(rtc->dev.parent, "start-year",
				       &start_year);
	if (!ret) {
		rtc->start_secs = mktime64(start_year, 1, 1, 0, 0, 0);
		rtc->set_start_time = true;
	}

	 
	if (!rtc->set_start_time)
		return;

	range_secs = rtc->range_max - rtc->range_min + 1;

	 
	if (rtc->start_secs > rtc->range_max ||
	    rtc->start_secs + range_secs - 1 < rtc->range_min)
		rtc->offset_secs = rtc->start_secs - rtc->range_min;
	else if (rtc->start_secs > rtc->range_min)
		rtc->offset_secs = range_secs;
	else if (rtc->start_secs < rtc->range_min)
		rtc->offset_secs = -range_secs;
	else
		rtc->offset_secs = 0;
}

static void devm_rtc_unregister_device(void *data)
{
	struct rtc_device *rtc = data;

	mutex_lock(&rtc->ops_lock);
	 
	rtc_proc_del_device(rtc);
	if (!test_bit(RTC_NO_CDEV, &rtc->flags))
		cdev_device_del(&rtc->char_dev, &rtc->dev);
	rtc->ops = NULL;
	mutex_unlock(&rtc->ops_lock);
}

static void devm_rtc_release_device(void *res)
{
	struct rtc_device *rtc = res;

	put_device(&rtc->dev);
}

struct rtc_device *devm_rtc_allocate_device(struct device *dev)
{
	struct rtc_device *rtc;
	int id, err;

	id = rtc_device_get_id(dev);
	if (id < 0)
		return ERR_PTR(id);

	rtc = rtc_allocate_device();
	if (!rtc) {
		ida_free(&rtc_ida, id);
		return ERR_PTR(-ENOMEM);
	}

	rtc->id = id;
	rtc->dev.parent = dev;
	err = devm_add_action_or_reset(dev, devm_rtc_release_device, rtc);
	if (err)
		return ERR_PTR(err);

	err = dev_set_name(&rtc->dev, "rtc%d", id);
	if (err)
		return ERR_PTR(err);

	return rtc;
}
EXPORT_SYMBOL_GPL(devm_rtc_allocate_device);

int __devm_rtc_register_device(struct module *owner, struct rtc_device *rtc)
{
	struct rtc_wkalrm alrm;
	int err;

	if (!rtc->ops) {
		dev_dbg(&rtc->dev, "no ops set\n");
		return -EINVAL;
	}

	if (!rtc->ops->set_alarm)
		clear_bit(RTC_FEATURE_ALARM, rtc->features);

	if (rtc->ops->set_offset)
		set_bit(RTC_FEATURE_CORRECTION, rtc->features);

	rtc->owner = owner;
	rtc_device_get_offset(rtc);

	 
	err = __rtc_read_alarm(rtc, &alrm);
	if (!err && !rtc_valid_tm(&alrm.time))
		rtc_initialize_alarm(rtc, &alrm);

	rtc_dev_prepare(rtc);

	err = cdev_device_add(&rtc->char_dev, &rtc->dev);
	if (err) {
		set_bit(RTC_NO_CDEV, &rtc->flags);
		dev_warn(rtc->dev.parent, "failed to add char device %d:%d\n",
			 MAJOR(rtc->dev.devt), rtc->id);
	} else {
		dev_dbg(rtc->dev.parent, "char device (%d:%d)\n",
			MAJOR(rtc->dev.devt), rtc->id);
	}

	rtc_proc_add_device(rtc);

	dev_info(rtc->dev.parent, "registered as %s\n",
		 dev_name(&rtc->dev));

#ifdef CONFIG_RTC_HCTOSYS_DEVICE
	if (!strcmp(dev_name(&rtc->dev), CONFIG_RTC_HCTOSYS_DEVICE))
		rtc_hctosys(rtc);
#endif

	return devm_add_action_or_reset(rtc->dev.parent,
					devm_rtc_unregister_device, rtc);
}
EXPORT_SYMBOL_GPL(__devm_rtc_register_device);

 
struct rtc_device *devm_rtc_device_register(struct device *dev,
					    const char *name,
					    const struct rtc_class_ops *ops,
					    struct module *owner)
{
	struct rtc_device *rtc;
	int err;

	rtc = devm_rtc_allocate_device(dev);
	if (IS_ERR(rtc))
		return rtc;

	rtc->ops = ops;

	err = __devm_rtc_register_device(owner, rtc);
	if (err)
		return ERR_PTR(err);

	return rtc;
}
EXPORT_SYMBOL_GPL(devm_rtc_device_register);

static int __init rtc_init(void)
{
	rtc_class = class_create("rtc");
	if (IS_ERR(rtc_class)) {
		pr_err("couldn't create class\n");
		return PTR_ERR(rtc_class);
	}
	rtc_class->pm = RTC_CLASS_DEV_PM_OPS;
	rtc_dev_init();
	return 0;
}
subsys_initcall(rtc_init);
