
 

#include <linux/rtc.h>
#include <linux/sched.h>
#include <linux/module.h>
#include <linux/log2.h>
#include <linux/workqueue.h>

#define CREATE_TRACE_POINTS
#include <trace/events/rtc.h>

static int rtc_timer_enqueue(struct rtc_device *rtc, struct rtc_timer *timer);
static void rtc_timer_remove(struct rtc_device *rtc, struct rtc_timer *timer);

static void rtc_add_offset(struct rtc_device *rtc, struct rtc_time *tm)
{
	time64_t secs;

	if (!rtc->offset_secs)
		return;

	secs = rtc_tm_to_time64(tm);

	 
	if ((rtc->start_secs > rtc->range_min && secs >= rtc->start_secs) ||
	    (rtc->start_secs < rtc->range_min &&
	     secs <= (rtc->start_secs + rtc->range_max - rtc->range_min)))
		return;

	rtc_time64_to_tm(secs + rtc->offset_secs, tm);
}

static void rtc_subtract_offset(struct rtc_device *rtc, struct rtc_time *tm)
{
	time64_t secs;

	if (!rtc->offset_secs)
		return;

	secs = rtc_tm_to_time64(tm);

	 
	if (secs >= rtc->range_min && secs <= rtc->range_max)
		return;

	rtc_time64_to_tm(secs - rtc->offset_secs, tm);
}

static int rtc_valid_range(struct rtc_device *rtc, struct rtc_time *tm)
{
	if (rtc->range_min != rtc->range_max) {
		time64_t time = rtc_tm_to_time64(tm);
		time64_t range_min = rtc->set_start_time ? rtc->start_secs :
			rtc->range_min;
		timeu64_t range_max = rtc->set_start_time ?
			(rtc->start_secs + rtc->range_max - rtc->range_min) :
			rtc->range_max;

		if (time < range_min || time > range_max)
			return -ERANGE;
	}

	return 0;
}

static int __rtc_read_time(struct rtc_device *rtc, struct rtc_time *tm)
{
	int err;

	if (!rtc->ops) {
		err = -ENODEV;
	} else if (!rtc->ops->read_time) {
		err = -EINVAL;
	} else {
		memset(tm, 0, sizeof(struct rtc_time));
		err = rtc->ops->read_time(rtc->dev.parent, tm);
		if (err < 0) {
			dev_dbg(&rtc->dev, "read_time: fail to read: %d\n",
				err);
			return err;
		}

		rtc_add_offset(rtc, tm);

		err = rtc_valid_tm(tm);
		if (err < 0)
			dev_dbg(&rtc->dev, "read_time: rtc_time isn't valid\n");
	}
	return err;
}

int rtc_read_time(struct rtc_device *rtc, struct rtc_time *tm)
{
	int err;

	err = mutex_lock_interruptible(&rtc->ops_lock);
	if (err)
		return err;

	err = __rtc_read_time(rtc, tm);
	mutex_unlock(&rtc->ops_lock);

	trace_rtc_read_time(rtc_tm_to_time64(tm), err);
	return err;
}
EXPORT_SYMBOL_GPL(rtc_read_time);

int rtc_set_time(struct rtc_device *rtc, struct rtc_time *tm)
{
	int err, uie;

	err = rtc_valid_tm(tm);
	if (err != 0)
		return err;

	err = rtc_valid_range(rtc, tm);
	if (err)
		return err;

	rtc_subtract_offset(rtc, tm);

#ifdef CONFIG_RTC_INTF_DEV_UIE_EMUL
	uie = rtc->uie_rtctimer.enabled || rtc->uie_irq_active;
#else
	uie = rtc->uie_rtctimer.enabled;
#endif
	if (uie) {
		err = rtc_update_irq_enable(rtc, 0);
		if (err)
			return err;
	}

	err = mutex_lock_interruptible(&rtc->ops_lock);
	if (err)
		return err;

	if (!rtc->ops)
		err = -ENODEV;
	else if (rtc->ops->set_time)
		err = rtc->ops->set_time(rtc->dev.parent, tm);
	else
		err = -EINVAL;

	pm_stay_awake(rtc->dev.parent);
	mutex_unlock(&rtc->ops_lock);
	 
	schedule_work(&rtc->irqwork);

	if (uie) {
		err = rtc_update_irq_enable(rtc, 1);
		if (err)
			return err;
	}

	trace_rtc_set_time(rtc_tm_to_time64(tm), err);
	return err;
}
EXPORT_SYMBOL_GPL(rtc_set_time);

static int rtc_read_alarm_internal(struct rtc_device *rtc,
				   struct rtc_wkalrm *alarm)
{
	int err;

	err = mutex_lock_interruptible(&rtc->ops_lock);
	if (err)
		return err;

	if (!rtc->ops) {
		err = -ENODEV;
	} else if (!test_bit(RTC_FEATURE_ALARM, rtc->features) || !rtc->ops->read_alarm) {
		err = -EINVAL;
	} else {
		alarm->enabled = 0;
		alarm->pending = 0;
		alarm->time.tm_sec = -1;
		alarm->time.tm_min = -1;
		alarm->time.tm_hour = -1;
		alarm->time.tm_mday = -1;
		alarm->time.tm_mon = -1;
		alarm->time.tm_year = -1;
		alarm->time.tm_wday = -1;
		alarm->time.tm_yday = -1;
		alarm->time.tm_isdst = -1;
		err = rtc->ops->read_alarm(rtc->dev.parent, alarm);
	}

	mutex_unlock(&rtc->ops_lock);

	trace_rtc_read_alarm(rtc_tm_to_time64(&alarm->time), err);
	return err;
}

int __rtc_read_alarm(struct rtc_device *rtc, struct rtc_wkalrm *alarm)
{
	int err;
	struct rtc_time before, now;
	int first_time = 1;
	time64_t t_now, t_alm;
	enum { none, day, month, year } missing = none;
	unsigned int days;

	 

	 
	err = rtc_read_time(rtc, &before);
	if (err < 0)
		return err;
	do {
		if (!first_time)
			memcpy(&before, &now, sizeof(struct rtc_time));
		first_time = 0;

		 
		err = rtc_read_alarm_internal(rtc, alarm);
		if (err)
			return err;

		 
		if (rtc_valid_tm(&alarm->time) == 0) {
			rtc_add_offset(rtc, &alarm->time);
			return 0;
		}

		 
		err = rtc_read_time(rtc, &now);
		if (err < 0)
			return err;

		 
	} while (before.tm_min  != now.tm_min ||
		 before.tm_hour != now.tm_hour ||
		 before.tm_mon  != now.tm_mon ||
		 before.tm_year != now.tm_year);

	 
	if (alarm->time.tm_sec == -1)
		alarm->time.tm_sec = now.tm_sec;
	if (alarm->time.tm_min == -1)
		alarm->time.tm_min = now.tm_min;
	if (alarm->time.tm_hour == -1)
		alarm->time.tm_hour = now.tm_hour;

	 
	if (alarm->time.tm_mday < 1 || alarm->time.tm_mday > 31) {
		alarm->time.tm_mday = now.tm_mday;
		missing = day;
	}
	if ((unsigned int)alarm->time.tm_mon >= 12) {
		alarm->time.tm_mon = now.tm_mon;
		if (missing == none)
			missing = month;
	}
	if (alarm->time.tm_year == -1) {
		alarm->time.tm_year = now.tm_year;
		if (missing == none)
			missing = year;
	}

	 
	err = rtc_valid_tm(&alarm->time);
	if (err)
		goto done;

	 
	t_now = rtc_tm_to_time64(&now);
	t_alm = rtc_tm_to_time64(&alarm->time);
	if (t_now < t_alm)
		goto done;

	switch (missing) {
	 
	case day:
		dev_dbg(&rtc->dev, "alarm rollover: %s\n", "day");
		t_alm += 24 * 60 * 60;
		rtc_time64_to_tm(t_alm, &alarm->time);
		break;

	 
	case month:
		dev_dbg(&rtc->dev, "alarm rollover: %s\n", "month");
		do {
			if (alarm->time.tm_mon < 11) {
				alarm->time.tm_mon++;
			} else {
				alarm->time.tm_mon = 0;
				alarm->time.tm_year++;
			}
			days = rtc_month_days(alarm->time.tm_mon,
					      alarm->time.tm_year);
		} while (days < alarm->time.tm_mday);
		break;

	 
	case year:
		dev_dbg(&rtc->dev, "alarm rollover: %s\n", "year");
		do {
			alarm->time.tm_year++;
		} while (!is_leap_year(alarm->time.tm_year + 1900) &&
			 rtc_valid_tm(&alarm->time) != 0);
		break;

	default:
		dev_warn(&rtc->dev, "alarm rollover not handled\n");
	}

	err = rtc_valid_tm(&alarm->time);

done:
	if (err && alarm->enabled)
		dev_warn(&rtc->dev, "invalid alarm value: %ptR\n",
			 &alarm->time);

	return err;
}

int rtc_read_alarm(struct rtc_device *rtc, struct rtc_wkalrm *alarm)
{
	int err;

	err = mutex_lock_interruptible(&rtc->ops_lock);
	if (err)
		return err;
	if (!rtc->ops) {
		err = -ENODEV;
	} else if (!test_bit(RTC_FEATURE_ALARM, rtc->features)) {
		err = -EINVAL;
	} else {
		memset(alarm, 0, sizeof(struct rtc_wkalrm));
		alarm->enabled = rtc->aie_timer.enabled;
		alarm->time = rtc_ktime_to_tm(rtc->aie_timer.node.expires);
	}
	mutex_unlock(&rtc->ops_lock);

	trace_rtc_read_alarm(rtc_tm_to_time64(&alarm->time), err);
	return err;
}
EXPORT_SYMBOL_GPL(rtc_read_alarm);

static int __rtc_set_alarm(struct rtc_device *rtc, struct rtc_wkalrm *alarm)
{
	struct rtc_time tm;
	time64_t now, scheduled;
	int err;

	err = rtc_valid_tm(&alarm->time);
	if (err)
		return err;

	scheduled = rtc_tm_to_time64(&alarm->time);

	 
	err = __rtc_read_time(rtc, &tm);
	if (err)
		return err;
	now = rtc_tm_to_time64(&tm);

	if (scheduled <= now)
		return -ETIME;
	 

	rtc_subtract_offset(rtc, &alarm->time);

	if (!rtc->ops)
		err = -ENODEV;
	else if (!test_bit(RTC_FEATURE_ALARM, rtc->features))
		err = -EINVAL;
	else
		err = rtc->ops->set_alarm(rtc->dev.parent, alarm);

	trace_rtc_set_alarm(rtc_tm_to_time64(&alarm->time), err);
	return err;
}

int rtc_set_alarm(struct rtc_device *rtc, struct rtc_wkalrm *alarm)
{
	ktime_t alarm_time;
	int err;

	if (!rtc->ops)
		return -ENODEV;
	else if (!test_bit(RTC_FEATURE_ALARM, rtc->features))
		return -EINVAL;

	err = rtc_valid_tm(&alarm->time);
	if (err != 0)
		return err;

	err = rtc_valid_range(rtc, &alarm->time);
	if (err)
		return err;

	err = mutex_lock_interruptible(&rtc->ops_lock);
	if (err)
		return err;
	if (rtc->aie_timer.enabled)
		rtc_timer_remove(rtc, &rtc->aie_timer);

	alarm_time = rtc_tm_to_ktime(alarm->time);
	 
	if (test_bit(RTC_FEATURE_ALARM_RES_MINUTE, rtc->features))
		alarm_time = ktime_sub_ns(alarm_time, (u64)alarm->time.tm_sec * NSEC_PER_SEC);

	rtc->aie_timer.node.expires = alarm_time;
	rtc->aie_timer.period = 0;
	if (alarm->enabled)
		err = rtc_timer_enqueue(rtc, &rtc->aie_timer);

	mutex_unlock(&rtc->ops_lock);

	return err;
}
EXPORT_SYMBOL_GPL(rtc_set_alarm);

 
int rtc_initialize_alarm(struct rtc_device *rtc, struct rtc_wkalrm *alarm)
{
	int err;
	struct rtc_time now;

	err = rtc_valid_tm(&alarm->time);
	if (err != 0)
		return err;

	err = rtc_read_time(rtc, &now);
	if (err)
		return err;

	err = mutex_lock_interruptible(&rtc->ops_lock);
	if (err)
		return err;

	rtc->aie_timer.node.expires = rtc_tm_to_ktime(alarm->time);
	rtc->aie_timer.period = 0;

	 
	if (alarm->enabled && (rtc_tm_to_ktime(now) <
			 rtc->aie_timer.node.expires)) {
		rtc->aie_timer.enabled = 1;
		timerqueue_add(&rtc->timerqueue, &rtc->aie_timer.node);
		trace_rtc_timer_enqueue(&rtc->aie_timer);
	}
	mutex_unlock(&rtc->ops_lock);
	return err;
}
EXPORT_SYMBOL_GPL(rtc_initialize_alarm);

int rtc_alarm_irq_enable(struct rtc_device *rtc, unsigned int enabled)
{
	int err;

	err = mutex_lock_interruptible(&rtc->ops_lock);
	if (err)
		return err;

	if (rtc->aie_timer.enabled != enabled) {
		if (enabled)
			err = rtc_timer_enqueue(rtc, &rtc->aie_timer);
		else
			rtc_timer_remove(rtc, &rtc->aie_timer);
	}

	if (err)
		 ;
	else if (!rtc->ops)
		err = -ENODEV;
	else if (!test_bit(RTC_FEATURE_ALARM, rtc->features) || !rtc->ops->alarm_irq_enable)
		err = -EINVAL;
	else
		err = rtc->ops->alarm_irq_enable(rtc->dev.parent, enabled);

	mutex_unlock(&rtc->ops_lock);

	trace_rtc_alarm_irq_enable(enabled, err);
	return err;
}
EXPORT_SYMBOL_GPL(rtc_alarm_irq_enable);

int rtc_update_irq_enable(struct rtc_device *rtc, unsigned int enabled)
{
	int err;

	err = mutex_lock_interruptible(&rtc->ops_lock);
	if (err)
		return err;

#ifdef CONFIG_RTC_INTF_DEV_UIE_EMUL
	if (enabled == 0 && rtc->uie_irq_active) {
		mutex_unlock(&rtc->ops_lock);
		return rtc_dev_update_irq_enable_emul(rtc, 0);
	}
#endif
	 
	if (rtc->uie_rtctimer.enabled == enabled)
		goto out;

	if (!test_bit(RTC_FEATURE_UPDATE_INTERRUPT, rtc->features) ||
	    !test_bit(RTC_FEATURE_ALARM, rtc->features)) {
		mutex_unlock(&rtc->ops_lock);
#ifdef CONFIG_RTC_INTF_DEV_UIE_EMUL
		return rtc_dev_update_irq_enable_emul(rtc, enabled);
#else
		return -EINVAL;
#endif
	}

	if (enabled) {
		struct rtc_time tm;
		ktime_t now, onesec;

		err = __rtc_read_time(rtc, &tm);
		if (err)
			goto out;
		onesec = ktime_set(1, 0);
		now = rtc_tm_to_ktime(tm);
		rtc->uie_rtctimer.node.expires = ktime_add(now, onesec);
		rtc->uie_rtctimer.period = ktime_set(1, 0);
		err = rtc_timer_enqueue(rtc, &rtc->uie_rtctimer);
	} else {
		rtc_timer_remove(rtc, &rtc->uie_rtctimer);
	}

out:
	mutex_unlock(&rtc->ops_lock);

	return err;
}
EXPORT_SYMBOL_GPL(rtc_update_irq_enable);

 
void rtc_handle_legacy_irq(struct rtc_device *rtc, int num, int mode)
{
	unsigned long flags;

	 
	spin_lock_irqsave(&rtc->irq_lock, flags);
	rtc->irq_data = (rtc->irq_data + (num << 8)) | (RTC_IRQF | mode);
	spin_unlock_irqrestore(&rtc->irq_lock, flags);

	wake_up_interruptible(&rtc->irq_queue);
	kill_fasync(&rtc->async_queue, SIGIO, POLL_IN);
}

 
void rtc_aie_update_irq(struct rtc_device *rtc)
{
	rtc_handle_legacy_irq(rtc, 1, RTC_AF);
}

 
void rtc_uie_update_irq(struct rtc_device *rtc)
{
	rtc_handle_legacy_irq(rtc, 1,  RTC_UF);
}

 
enum hrtimer_restart rtc_pie_update_irq(struct hrtimer *timer)
{
	struct rtc_device *rtc;
	ktime_t period;
	u64 count;

	rtc = container_of(timer, struct rtc_device, pie_timer);

	period = NSEC_PER_SEC / rtc->irq_freq;
	count = hrtimer_forward_now(timer, period);

	rtc_handle_legacy_irq(rtc, count, RTC_PF);

	return HRTIMER_RESTART;
}

 
void rtc_update_irq(struct rtc_device *rtc,
		    unsigned long num, unsigned long events)
{
	if (IS_ERR_OR_NULL(rtc))
		return;

	pm_stay_awake(rtc->dev.parent);
	schedule_work(&rtc->irqwork);
}
EXPORT_SYMBOL_GPL(rtc_update_irq);

struct rtc_device *rtc_class_open(const char *name)
{
	struct device *dev;
	struct rtc_device *rtc = NULL;

	dev = class_find_device_by_name(rtc_class, name);
	if (dev)
		rtc = to_rtc_device(dev);

	if (rtc) {
		if (!try_module_get(rtc->owner)) {
			put_device(dev);
			rtc = NULL;
		}
	}

	return rtc;
}
EXPORT_SYMBOL_GPL(rtc_class_open);

void rtc_class_close(struct rtc_device *rtc)
{
	module_put(rtc->owner);
	put_device(&rtc->dev);
}
EXPORT_SYMBOL_GPL(rtc_class_close);

static int rtc_update_hrtimer(struct rtc_device *rtc, int enabled)
{
	 
	if (hrtimer_try_to_cancel(&rtc->pie_timer) < 0)
		return -1;

	if (enabled) {
		ktime_t period = NSEC_PER_SEC / rtc->irq_freq;

		hrtimer_start(&rtc->pie_timer, period, HRTIMER_MODE_REL);
	}
	return 0;
}

 
int rtc_irq_set_state(struct rtc_device *rtc, int enabled)
{
	int err = 0;

	while (rtc_update_hrtimer(rtc, enabled) < 0)
		cpu_relax();

	rtc->pie_enabled = enabled;

	trace_rtc_irq_set_state(enabled, err);
	return err;
}

 
int rtc_irq_set_freq(struct rtc_device *rtc, int freq)
{
	int err = 0;

	if (freq <= 0 || freq > RTC_MAX_FREQ)
		return -EINVAL;

	rtc->irq_freq = freq;
	while (rtc->pie_enabled && rtc_update_hrtimer(rtc, 1) < 0)
		cpu_relax();

	trace_rtc_irq_set_freq(freq, err);
	return err;
}

 
static int rtc_timer_enqueue(struct rtc_device *rtc, struct rtc_timer *timer)
{
	struct timerqueue_node *next = timerqueue_getnext(&rtc->timerqueue);
	struct rtc_time tm;
	ktime_t now;
	int err;

	err = __rtc_read_time(rtc, &tm);
	if (err)
		return err;

	timer->enabled = 1;
	now = rtc_tm_to_ktime(tm);

	 
	while (next) {
		if (next->expires >= now)
			break;
		next = timerqueue_iterate_next(next);
	}

	timerqueue_add(&rtc->timerqueue, &timer->node);
	trace_rtc_timer_enqueue(timer);
	if (!next || ktime_before(timer->node.expires, next->expires)) {
		struct rtc_wkalrm alarm;

		alarm.time = rtc_ktime_to_tm(timer->node.expires);
		alarm.enabled = 1;
		err = __rtc_set_alarm(rtc, &alarm);
		if (err == -ETIME) {
			pm_stay_awake(rtc->dev.parent);
			schedule_work(&rtc->irqwork);
		} else if (err) {
			timerqueue_del(&rtc->timerqueue, &timer->node);
			trace_rtc_timer_dequeue(timer);
			timer->enabled = 0;
			return err;
		}
	}
	return 0;
}

static void rtc_alarm_disable(struct rtc_device *rtc)
{
	if (!rtc->ops || !test_bit(RTC_FEATURE_ALARM, rtc->features) || !rtc->ops->alarm_irq_enable)
		return;

	rtc->ops->alarm_irq_enable(rtc->dev.parent, false);
	trace_rtc_alarm_irq_enable(0, 0);
}

 
static void rtc_timer_remove(struct rtc_device *rtc, struct rtc_timer *timer)
{
	struct timerqueue_node *next = timerqueue_getnext(&rtc->timerqueue);

	timerqueue_del(&rtc->timerqueue, &timer->node);
	trace_rtc_timer_dequeue(timer);
	timer->enabled = 0;
	if (next == &timer->node) {
		struct rtc_wkalrm alarm;
		int err;

		next = timerqueue_getnext(&rtc->timerqueue);
		if (!next) {
			rtc_alarm_disable(rtc);
			return;
		}
		alarm.time = rtc_ktime_to_tm(next->expires);
		alarm.enabled = 1;
		err = __rtc_set_alarm(rtc, &alarm);
		if (err == -ETIME) {
			pm_stay_awake(rtc->dev.parent);
			schedule_work(&rtc->irqwork);
		}
	}
}

 
void rtc_timer_do_work(struct work_struct *work)
{
	struct rtc_timer *timer;
	struct timerqueue_node *next;
	ktime_t now;
	struct rtc_time tm;

	struct rtc_device *rtc =
		container_of(work, struct rtc_device, irqwork);

	mutex_lock(&rtc->ops_lock);
again:
	__rtc_read_time(rtc, &tm);
	now = rtc_tm_to_ktime(tm);
	while ((next = timerqueue_getnext(&rtc->timerqueue))) {
		if (next->expires > now)
			break;

		 
		timer = container_of(next, struct rtc_timer, node);
		timerqueue_del(&rtc->timerqueue, &timer->node);
		trace_rtc_timer_dequeue(timer);
		timer->enabled = 0;
		if (timer->func)
			timer->func(timer->rtc);

		trace_rtc_timer_fired(timer);
		 
		if (ktime_to_ns(timer->period)) {
			timer->node.expires = ktime_add(timer->node.expires,
							timer->period);
			timer->enabled = 1;
			timerqueue_add(&rtc->timerqueue, &timer->node);
			trace_rtc_timer_enqueue(timer);
		}
	}

	 
	if (next) {
		struct rtc_wkalrm alarm;
		int err;
		int retry = 3;

		alarm.time = rtc_ktime_to_tm(next->expires);
		alarm.enabled = 1;
reprogram:
		err = __rtc_set_alarm(rtc, &alarm);
		if (err == -ETIME) {
			goto again;
		} else if (err) {
			if (retry-- > 0)
				goto reprogram;

			timer = container_of(next, struct rtc_timer, node);
			timerqueue_del(&rtc->timerqueue, &timer->node);
			trace_rtc_timer_dequeue(timer);
			timer->enabled = 0;
			dev_err(&rtc->dev, "__rtc_set_alarm: err=%d\n", err);
			goto again;
		}
	} else {
		rtc_alarm_disable(rtc);
	}

	pm_relax(rtc->dev.parent);
	mutex_unlock(&rtc->ops_lock);
}

 
void rtc_timer_init(struct rtc_timer *timer, void (*f)(struct rtc_device *r),
		    struct rtc_device *rtc)
{
	timerqueue_init(&timer->node);
	timer->enabled = 0;
	timer->func = f;
	timer->rtc = rtc;
}

 
int rtc_timer_start(struct rtc_device *rtc, struct rtc_timer *timer,
		    ktime_t expires, ktime_t period)
{
	int ret = 0;

	mutex_lock(&rtc->ops_lock);
	if (timer->enabled)
		rtc_timer_remove(rtc, timer);

	timer->node.expires = expires;
	timer->period = period;

	ret = rtc_timer_enqueue(rtc, timer);

	mutex_unlock(&rtc->ops_lock);
	return ret;
}

 
void rtc_timer_cancel(struct rtc_device *rtc, struct rtc_timer *timer)
{
	mutex_lock(&rtc->ops_lock);
	if (timer->enabled)
		rtc_timer_remove(rtc, timer);
	mutex_unlock(&rtc->ops_lock);
}

 
int rtc_read_offset(struct rtc_device *rtc, long *offset)
{
	int ret;

	if (!rtc->ops)
		return -ENODEV;

	if (!rtc->ops->read_offset)
		return -EINVAL;

	mutex_lock(&rtc->ops_lock);
	ret = rtc->ops->read_offset(rtc->dev.parent, offset);
	mutex_unlock(&rtc->ops_lock);

	trace_rtc_read_offset(*offset, ret);
	return ret;
}

 
int rtc_set_offset(struct rtc_device *rtc, long offset)
{
	int ret;

	if (!rtc->ops)
		return -ENODEV;

	if (!rtc->ops->set_offset)
		return -EINVAL;

	mutex_lock(&rtc->ops_lock);
	ret = rtc->ops->set_offset(rtc->dev.parent, offset);
	mutex_unlock(&rtc->ops_lock);

	trace_rtc_set_offset(offset, ret);
	return ret;
}
