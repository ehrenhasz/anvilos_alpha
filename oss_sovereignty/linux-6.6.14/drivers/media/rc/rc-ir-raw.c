




#include <linux/export.h>
#include <linux/kthread.h>
#include <linux/mutex.h>
#include <linux/kmod.h>
#include <linux/sched.h>
#include "rc-core-priv.h"

 
static LIST_HEAD(ir_raw_client_list);

 
DEFINE_MUTEX(ir_raw_handler_lock);
static LIST_HEAD(ir_raw_handler_list);
static atomic64_t available_protocols = ATOMIC64_INIT(0);

static int ir_raw_event_thread(void *data)
{
	struct ir_raw_event ev;
	struct ir_raw_handler *handler;
	struct ir_raw_event_ctrl *raw = data;
	struct rc_dev *dev = raw->dev;

	while (1) {
		mutex_lock(&ir_raw_handler_lock);
		while (kfifo_out(&raw->kfifo, &ev, 1)) {
			if (is_timing_event(ev)) {
				if (ev.duration == 0)
					dev_warn_once(&dev->dev, "nonsensical timing event of duration 0");
				if (is_timing_event(raw->prev_ev) &&
				    !is_transition(&ev, &raw->prev_ev))
					dev_warn_once(&dev->dev, "two consecutive events of type %s",
						      TO_STR(ev.pulse));
			}
			list_for_each_entry(handler, &ir_raw_handler_list, list)
				if (dev->enabled_protocols &
				    handler->protocols || !handler->protocols)
					handler->decode(dev, ev);
			lirc_raw_event(dev, ev);
			raw->prev_ev = ev;
		}
		mutex_unlock(&ir_raw_handler_lock);

		set_current_state(TASK_INTERRUPTIBLE);

		if (kthread_should_stop()) {
			__set_current_state(TASK_RUNNING);
			break;
		} else if (!kfifo_is_empty(&raw->kfifo))
			set_current_state(TASK_RUNNING);

		schedule();
	}

	return 0;
}

 
int ir_raw_event_store(struct rc_dev *dev, struct ir_raw_event *ev)
{
	if (!dev->raw)
		return -EINVAL;

	dev_dbg(&dev->dev, "sample: (%05dus %s)\n",
		ev->duration, TO_STR(ev->pulse));

	if (!kfifo_put(&dev->raw->kfifo, *ev)) {
		dev_err(&dev->dev, "IR event FIFO is full!\n");
		return -ENOSPC;
	}

	return 0;
}
EXPORT_SYMBOL_GPL(ir_raw_event_store);

 
int ir_raw_event_store_edge(struct rc_dev *dev, bool pulse)
{
	ktime_t			now;
	struct ir_raw_event	ev = {};

	if (!dev->raw)
		return -EINVAL;

	now = ktime_get();
	ev.duration = ktime_to_us(ktime_sub(now, dev->raw->last_event));
	ev.pulse = !pulse;

	return ir_raw_event_store_with_timeout(dev, &ev);
}
EXPORT_SYMBOL_GPL(ir_raw_event_store_edge);

 
int ir_raw_event_store_with_timeout(struct rc_dev *dev, struct ir_raw_event *ev)
{
	ktime_t		now;
	int		rc = 0;

	if (!dev->raw)
		return -EINVAL;

	now = ktime_get();

	spin_lock(&dev->raw->edge_spinlock);
	rc = ir_raw_event_store(dev, ev);

	dev->raw->last_event = now;

	 
	if (!timer_pending(&dev->raw->edge_handle) ||
	    time_after(dev->raw->edge_handle.expires,
		       jiffies + msecs_to_jiffies(15))) {
		mod_timer(&dev->raw->edge_handle,
			  jiffies + msecs_to_jiffies(15));
	}
	spin_unlock(&dev->raw->edge_spinlock);

	return rc;
}
EXPORT_SYMBOL_GPL(ir_raw_event_store_with_timeout);

 
int ir_raw_event_store_with_filter(struct rc_dev *dev, struct ir_raw_event *ev)
{
	if (!dev->raw)
		return -EINVAL;

	 
	if (dev->idle && !ev->pulse)
		return 0;
	else if (dev->idle)
		ir_raw_event_set_idle(dev, false);

	if (!dev->raw->this_ev.duration)
		dev->raw->this_ev = *ev;
	else if (ev->pulse == dev->raw->this_ev.pulse)
		dev->raw->this_ev.duration += ev->duration;
	else {
		ir_raw_event_store(dev, &dev->raw->this_ev);
		dev->raw->this_ev = *ev;
	}

	 
	if (!ev->pulse && dev->timeout &&
	    dev->raw->this_ev.duration >= dev->timeout)
		ir_raw_event_set_idle(dev, true);

	return 1;
}
EXPORT_SYMBOL_GPL(ir_raw_event_store_with_filter);

 
void ir_raw_event_set_idle(struct rc_dev *dev, bool idle)
{
	if (!dev->raw)
		return;

	dev_dbg(&dev->dev, "%s idle mode\n", idle ? "enter" : "leave");

	if (idle) {
		dev->raw->this_ev.timeout = true;
		ir_raw_event_store(dev, &dev->raw->this_ev);
		dev->raw->this_ev = (struct ir_raw_event) {};
	}

	if (dev->s_idle)
		dev->s_idle(dev, idle);

	dev->idle = idle;
}
EXPORT_SYMBOL_GPL(ir_raw_event_set_idle);

 
void ir_raw_event_handle(struct rc_dev *dev)
{
	if (!dev->raw || !dev->raw->thread)
		return;

	wake_up_process(dev->raw->thread);
}
EXPORT_SYMBOL_GPL(ir_raw_event_handle);

 
u64
ir_raw_get_allowed_protocols(void)
{
	return atomic64_read(&available_protocols);
}

static int change_protocol(struct rc_dev *dev, u64 *rc_proto)
{
	struct ir_raw_handler *handler;
	u32 timeout = 0;

	mutex_lock(&ir_raw_handler_lock);
	list_for_each_entry(handler, &ir_raw_handler_list, list) {
		if (!(dev->enabled_protocols & handler->protocols) &&
		    (*rc_proto & handler->protocols) && handler->raw_register)
			handler->raw_register(dev);

		if ((dev->enabled_protocols & handler->protocols) &&
		    !(*rc_proto & handler->protocols) &&
		    handler->raw_unregister)
			handler->raw_unregister(dev);
	}
	mutex_unlock(&ir_raw_handler_lock);

	if (!dev->max_timeout)
		return 0;

	mutex_lock(&ir_raw_handler_lock);
	list_for_each_entry(handler, &ir_raw_handler_list, list) {
		if (handler->protocols & *rc_proto) {
			if (timeout < handler->min_timeout)
				timeout = handler->min_timeout;
		}
	}
	mutex_unlock(&ir_raw_handler_lock);

	if (timeout == 0)
		timeout = IR_DEFAULT_TIMEOUT;
	else
		timeout += MS_TO_US(10);

	if (timeout < dev->min_timeout)
		timeout = dev->min_timeout;
	else if (timeout > dev->max_timeout)
		timeout = dev->max_timeout;

	if (dev->s_timeout)
		dev->s_timeout(dev, timeout);
	else
		dev->timeout = timeout;

	return 0;
}

static void ir_raw_disable_protocols(struct rc_dev *dev, u64 protocols)
{
	mutex_lock(&dev->lock);
	dev->enabled_protocols &= ~protocols;
	mutex_unlock(&dev->lock);
}

 
int ir_raw_gen_manchester(struct ir_raw_event **ev, unsigned int max,
			  const struct ir_raw_timings_manchester *timings,
			  unsigned int n, u64 data)
{
	bool need_pulse;
	u64 i;
	int ret = -ENOBUFS;

	i = BIT_ULL(n - 1);

	if (timings->leader_pulse) {
		if (!max--)
			return ret;
		init_ir_raw_event_duration((*ev), 1, timings->leader_pulse);
		if (timings->leader_space) {
			if (!max--)
				return ret;
			init_ir_raw_event_duration(++(*ev), 0,
						   timings->leader_space);
		}
	} else {
		 
		--(*ev);
	}
	 

	while (n && i > 0) {
		need_pulse = !(data & i);
		if (timings->invert)
			need_pulse = !need_pulse;
		if (need_pulse == !!(*ev)->pulse) {
			(*ev)->duration += timings->clock;
		} else {
			if (!max--)
				goto nobufs;
			init_ir_raw_event_duration(++(*ev), need_pulse,
						   timings->clock);
		}

		if (!max--)
			goto nobufs;
		init_ir_raw_event_duration(++(*ev), !need_pulse,
					   timings->clock);
		i >>= 1;
	}

	if (timings->trailer_space) {
		if (!(*ev)->pulse)
			(*ev)->duration += timings->trailer_space;
		else if (!max--)
			goto nobufs;
		else
			init_ir_raw_event_duration(++(*ev), 0,
						   timings->trailer_space);
	}

	ret = 0;
nobufs:
	 
	++(*ev);
	return ret;
}
EXPORT_SYMBOL(ir_raw_gen_manchester);

 
int ir_raw_gen_pd(struct ir_raw_event **ev, unsigned int max,
		  const struct ir_raw_timings_pd *timings,
		  unsigned int n, u64 data)
{
	int i;
	int ret;
	unsigned int space;

	if (timings->header_pulse) {
		ret = ir_raw_gen_pulse_space(ev, &max, timings->header_pulse,
					     timings->header_space);
		if (ret)
			return ret;
	}

	if (timings->msb_first) {
		for (i = n - 1; i >= 0; --i) {
			space = timings->bit_space[(data >> i) & 1];
			ret = ir_raw_gen_pulse_space(ev, &max,
						     timings->bit_pulse,
						     space);
			if (ret)
				return ret;
		}
	} else {
		for (i = 0; i < n; ++i, data >>= 1) {
			space = timings->bit_space[data & 1];
			ret = ir_raw_gen_pulse_space(ev, &max,
						     timings->bit_pulse,
						     space);
			if (ret)
				return ret;
		}
	}

	ret = ir_raw_gen_pulse_space(ev, &max, timings->trailer_pulse,
				     timings->trailer_space);
	return ret;
}
EXPORT_SYMBOL(ir_raw_gen_pd);

 
int ir_raw_gen_pl(struct ir_raw_event **ev, unsigned int max,
		  const struct ir_raw_timings_pl *timings,
		  unsigned int n, u64 data)
{
	int i;
	int ret = -ENOBUFS;
	unsigned int pulse;

	if (!max--)
		return ret;

	init_ir_raw_event_duration((*ev)++, 1, timings->header_pulse);

	if (timings->msb_first) {
		for (i = n - 1; i >= 0; --i) {
			if (!max--)
				return ret;
			init_ir_raw_event_duration((*ev)++, 0,
						   timings->bit_space);
			if (!max--)
				return ret;
			pulse = timings->bit_pulse[(data >> i) & 1];
			init_ir_raw_event_duration((*ev)++, 1, pulse);
		}
	} else {
		for (i = 0; i < n; ++i, data >>= 1) {
			if (!max--)
				return ret;
			init_ir_raw_event_duration((*ev)++, 0,
						   timings->bit_space);
			if (!max--)
				return ret;
			pulse = timings->bit_pulse[data & 1];
			init_ir_raw_event_duration((*ev)++, 1, pulse);
		}
	}

	if (!max--)
		return ret;

	init_ir_raw_event_duration((*ev)++, 0, timings->trailer_space);

	return 0;
}
EXPORT_SYMBOL(ir_raw_gen_pl);

 
int ir_raw_encode_scancode(enum rc_proto protocol, u32 scancode,
			   struct ir_raw_event *events, unsigned int max)
{
	struct ir_raw_handler *handler;
	int ret = -EINVAL;
	u64 mask = 1ULL << protocol;

	ir_raw_load_modules(&mask);

	mutex_lock(&ir_raw_handler_lock);
	list_for_each_entry(handler, &ir_raw_handler_list, list) {
		if (handler->protocols & mask && handler->encode) {
			ret = handler->encode(protocol, scancode, events, max);
			if (ret >= 0 || ret == -ENOBUFS)
				break;
		}
	}
	mutex_unlock(&ir_raw_handler_lock);

	return ret;
}
EXPORT_SYMBOL(ir_raw_encode_scancode);

 
static void ir_raw_edge_handle(struct timer_list *t)
{
	struct ir_raw_event_ctrl *raw = from_timer(raw, t, edge_handle);
	struct rc_dev *dev = raw->dev;
	unsigned long flags;
	ktime_t interval;

	spin_lock_irqsave(&dev->raw->edge_spinlock, flags);
	interval = ktime_sub(ktime_get(), dev->raw->last_event);
	if (ktime_to_us(interval) >= dev->timeout) {
		struct ir_raw_event ev = {
			.timeout = true,
			.duration = ktime_to_us(interval)
		};

		ir_raw_event_store(dev, &ev);
	} else {
		mod_timer(&dev->raw->edge_handle,
			  jiffies + usecs_to_jiffies(dev->timeout -
						     ktime_to_us(interval)));
	}
	spin_unlock_irqrestore(&dev->raw->edge_spinlock, flags);

	ir_raw_event_handle(dev);
}

 
int ir_raw_encode_carrier(enum rc_proto protocol)
{
	struct ir_raw_handler *handler;
	int ret = -EINVAL;
	u64 mask = BIT_ULL(protocol);

	mutex_lock(&ir_raw_handler_lock);
	list_for_each_entry(handler, &ir_raw_handler_list, list) {
		if (handler->protocols & mask && handler->encode) {
			ret = handler->carrier;
			break;
		}
	}
	mutex_unlock(&ir_raw_handler_lock);

	return ret;
}
EXPORT_SYMBOL(ir_raw_encode_carrier);

 
int ir_raw_event_prepare(struct rc_dev *dev)
{
	if (!dev)
		return -EINVAL;

	dev->raw = kzalloc(sizeof(*dev->raw), GFP_KERNEL);
	if (!dev->raw)
		return -ENOMEM;

	dev->raw->dev = dev;
	dev->change_protocol = change_protocol;
	dev->idle = true;
	spin_lock_init(&dev->raw->edge_spinlock);
	timer_setup(&dev->raw->edge_handle, ir_raw_edge_handle, 0);
	INIT_KFIFO(dev->raw->kfifo);

	return 0;
}

int ir_raw_event_register(struct rc_dev *dev)
{
	struct task_struct *thread;

	thread = kthread_run(ir_raw_event_thread, dev->raw, "rc%u", dev->minor);
	if (IS_ERR(thread))
		return PTR_ERR(thread);

	dev->raw->thread = thread;

	mutex_lock(&ir_raw_handler_lock);
	list_add_tail(&dev->raw->list, &ir_raw_client_list);
	mutex_unlock(&ir_raw_handler_lock);

	return 0;
}

void ir_raw_event_free(struct rc_dev *dev)
{
	if (!dev)
		return;

	kfree(dev->raw);
	dev->raw = NULL;
}

void ir_raw_event_unregister(struct rc_dev *dev)
{
	struct ir_raw_handler *handler;

	if (!dev || !dev->raw)
		return;

	kthread_stop(dev->raw->thread);
	del_timer_sync(&dev->raw->edge_handle);

	mutex_lock(&ir_raw_handler_lock);
	list_del(&dev->raw->list);
	list_for_each_entry(handler, &ir_raw_handler_list, list)
		if (handler->raw_unregister &&
		    (handler->protocols & dev->enabled_protocols))
			handler->raw_unregister(dev);

	lirc_bpf_free(dev);

	ir_raw_event_free(dev);

	 
	mutex_unlock(&ir_raw_handler_lock);
}

 

int ir_raw_handler_register(struct ir_raw_handler *ir_raw_handler)
{
	mutex_lock(&ir_raw_handler_lock);
	list_add_tail(&ir_raw_handler->list, &ir_raw_handler_list);
	atomic64_or(ir_raw_handler->protocols, &available_protocols);
	mutex_unlock(&ir_raw_handler_lock);

	return 0;
}
EXPORT_SYMBOL(ir_raw_handler_register);

void ir_raw_handler_unregister(struct ir_raw_handler *ir_raw_handler)
{
	struct ir_raw_event_ctrl *raw;
	u64 protocols = ir_raw_handler->protocols;

	mutex_lock(&ir_raw_handler_lock);
	list_del(&ir_raw_handler->list);
	list_for_each_entry(raw, &ir_raw_client_list, list) {
		if (ir_raw_handler->raw_unregister &&
		    (raw->dev->enabled_protocols & protocols))
			ir_raw_handler->raw_unregister(raw->dev);
		ir_raw_disable_protocols(raw->dev, protocols);
	}
	atomic64_andnot(protocols, &available_protocols);
	mutex_unlock(&ir_raw_handler_lock);
}
EXPORT_SYMBOL(ir_raw_handler_unregister);
