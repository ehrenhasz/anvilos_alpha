 
#include "priv.h"

s64
nvkm_timer_wait_test(struct nvkm_timer_wait *wait)
{
	struct nvkm_subdev *subdev = &wait->tmr->subdev;
	u64 time = nvkm_timer_read(wait->tmr);

	if (wait->reads == 0) {
		wait->time0 = time;
		wait->time1 = time;
	}

	if (wait->time1 == time) {
		if (wait->reads++ == 16) {
			nvkm_fatal(subdev, "stalled at %016llx\n", time);
			return -ETIMEDOUT;
		}
	} else {
		wait->time1 = time;
		wait->reads = 1;
	}

	if (wait->time1 - wait->time0 > wait->limit)
		return -ETIMEDOUT;

	return wait->time1 - wait->time0;
}

void
nvkm_timer_wait_init(struct nvkm_device *device, u64 nsec,
		     struct nvkm_timer_wait *wait)
{
	wait->tmr = device->timer;
	wait->limit = nsec;
	wait->reads = 0;
}

u64
nvkm_timer_read(struct nvkm_timer *tmr)
{
	return tmr->func->read(tmr);
}

void
nvkm_timer_alarm_trigger(struct nvkm_timer *tmr)
{
	struct nvkm_alarm *alarm, *atemp;
	unsigned long flags;
	LIST_HEAD(exec);

	 
	spin_lock_irqsave(&tmr->lock, flags);
	list_for_each_entry_safe(alarm, atemp, &tmr->alarms, head) {
		 
		if (alarm->timestamp > nvkm_timer_read(tmr)) {
			 
			tmr->func->alarm_init(tmr, alarm->timestamp);
			if (alarm->timestamp > nvkm_timer_read(tmr))
				break;
		}

		 
		list_del_init(&alarm->head);
		list_add(&alarm->exec, &exec);
	}

	 
	if (list_empty(&tmr->alarms))
		tmr->func->alarm_fini(tmr);
	spin_unlock_irqrestore(&tmr->lock, flags);

	 
	list_for_each_entry_safe(alarm, atemp, &exec, exec) {
		list_del(&alarm->exec);
		alarm->func(alarm);
	}
}

void
nvkm_timer_alarm(struct nvkm_timer *tmr, u32 nsec, struct nvkm_alarm *alarm)
{
	struct nvkm_alarm *list;
	unsigned long flags;

	 
	spin_lock_irqsave(&tmr->lock, flags);
	list_del_init(&alarm->head);

	if (nsec) {
		 
		alarm->timestamp = nvkm_timer_read(tmr) + nsec;
		list_for_each_entry(list, &tmr->alarms, head) {
			if (list->timestamp > alarm->timestamp)
				break;
		}

		list_add_tail(&alarm->head, &list->head);

		 
		list = list_first_entry(&tmr->alarms, typeof(*list), head);
		if (list == alarm) {
			tmr->func->alarm_init(tmr, alarm->timestamp);
			 
			WARN_ON(alarm->timestamp <= nvkm_timer_read(tmr));
		}
	}
	spin_unlock_irqrestore(&tmr->lock, flags);
}

static void
nvkm_timer_intr(struct nvkm_subdev *subdev)
{
	struct nvkm_timer *tmr = nvkm_timer(subdev);
	tmr->func->intr(tmr);
}

static int
nvkm_timer_fini(struct nvkm_subdev *subdev, bool suspend)
{
	struct nvkm_timer *tmr = nvkm_timer(subdev);
	tmr->func->alarm_fini(tmr);
	return 0;
}

static int
nvkm_timer_init(struct nvkm_subdev *subdev)
{
	struct nvkm_timer *tmr = nvkm_timer(subdev);
	if (tmr->func->init)
		tmr->func->init(tmr);
	tmr->func->time(tmr, ktime_to_ns(ktime_get()));
	nvkm_timer_alarm_trigger(tmr);
	return 0;
}

static void *
nvkm_timer_dtor(struct nvkm_subdev *subdev)
{
	return nvkm_timer(subdev);
}

static const struct nvkm_subdev_func
nvkm_timer = {
	.dtor = nvkm_timer_dtor,
	.init = nvkm_timer_init,
	.fini = nvkm_timer_fini,
	.intr = nvkm_timer_intr,
};

int
nvkm_timer_new_(const struct nvkm_timer_func *func, struct nvkm_device *device,
		enum nvkm_subdev_type type, int inst, struct nvkm_timer **ptmr)
{
	struct nvkm_timer *tmr;

	if (!(tmr = *ptmr = kzalloc(sizeof(*tmr), GFP_KERNEL)))
		return -ENOMEM;

	nvkm_subdev_ctor(&nvkm_timer, device, type, inst, &tmr->subdev);
	tmr->func = func;
	INIT_LIST_HEAD(&tmr->alarms);
	spin_lock_init(&tmr->lock);
	return 0;
}
