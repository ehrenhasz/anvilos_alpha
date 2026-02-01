

#include <uapi/linux/sched/types.h>

#include <drm/drm_print.h>
#include <drm/drm_vblank.h>
#include <drm/drm_vblank_work.h>
#include <drm/drm_crtc.h>

#include "drm_internal.h"

 

void drm_handle_vblank_works(struct drm_vblank_crtc *vblank)
{
	struct drm_vblank_work *work, *next;
	u64 count = atomic64_read(&vblank->count);
	bool wake = false;

	assert_spin_locked(&vblank->dev->event_lock);

	list_for_each_entry_safe(work, next, &vblank->pending_work, node) {
		if (!drm_vblank_passed(count, work->count))
			continue;

		list_del_init(&work->node);
		drm_vblank_put(vblank->dev, vblank->pipe);
		kthread_queue_work(vblank->worker, &work->base);
		wake = true;
	}
	if (wake)
		wake_up_all(&vblank->work_wait_queue);
}

 
void drm_vblank_cancel_pending_works(struct drm_vblank_crtc *vblank)
{
	struct drm_vblank_work *work, *next;

	assert_spin_locked(&vblank->dev->event_lock);

	list_for_each_entry_safe(work, next, &vblank->pending_work, node) {
		list_del_init(&work->node);
		drm_vblank_put(vblank->dev, vblank->pipe);
	}

	wake_up_all(&vblank->work_wait_queue);
}

 
int drm_vblank_work_schedule(struct drm_vblank_work *work,
			     u64 count, bool nextonmiss)
{
	struct drm_vblank_crtc *vblank = work->vblank;
	struct drm_device *dev = vblank->dev;
	u64 cur_vbl;
	unsigned long irqflags;
	bool passed, inmodeset, rescheduling = false, wake = false;
	int ret = 0;

	spin_lock_irqsave(&dev->event_lock, irqflags);
	if (work->cancelling)
		goto out;

	spin_lock(&dev->vbl_lock);
	inmodeset = vblank->inmodeset;
	spin_unlock(&dev->vbl_lock);
	if (inmodeset)
		goto out;

	if (list_empty(&work->node)) {
		ret = drm_vblank_get(dev, vblank->pipe);
		if (ret < 0)
			goto out;
	} else if (work->count == count) {
		 
		goto out;
	} else {
		rescheduling = true;
	}

	work->count = count;
	cur_vbl = drm_vblank_count(dev, vblank->pipe);
	passed = drm_vblank_passed(cur_vbl, count);
	if (passed)
		drm_dbg_core(dev,
			     "crtc %d vblank %llu already passed (current %llu)\n",
			     vblank->pipe, count, cur_vbl);

	if (!nextonmiss && passed) {
		drm_vblank_put(dev, vblank->pipe);
		ret = kthread_queue_work(vblank->worker, &work->base);

		if (rescheduling) {
			list_del_init(&work->node);
			wake = true;
		}
	} else {
		if (!rescheduling)
			list_add_tail(&work->node, &vblank->pending_work);
		ret = true;
	}

out:
	spin_unlock_irqrestore(&dev->event_lock, irqflags);
	if (wake)
		wake_up_all(&vblank->work_wait_queue);
	return ret;
}
EXPORT_SYMBOL(drm_vblank_work_schedule);

 
bool drm_vblank_work_cancel_sync(struct drm_vblank_work *work)
{
	struct drm_vblank_crtc *vblank = work->vblank;
	struct drm_device *dev = vblank->dev;
	bool ret = false;

	spin_lock_irq(&dev->event_lock);
	if (!list_empty(&work->node)) {
		list_del_init(&work->node);
		drm_vblank_put(vblank->dev, vblank->pipe);
		ret = true;
	}

	work->cancelling++;
	spin_unlock_irq(&dev->event_lock);

	wake_up_all(&vblank->work_wait_queue);

	if (kthread_cancel_work_sync(&work->base))
		ret = true;

	spin_lock_irq(&dev->event_lock);
	work->cancelling--;
	spin_unlock_irq(&dev->event_lock);

	return ret;
}
EXPORT_SYMBOL(drm_vblank_work_cancel_sync);

 
void drm_vblank_work_flush(struct drm_vblank_work *work)
{
	struct drm_vblank_crtc *vblank = work->vblank;
	struct drm_device *dev = vblank->dev;

	spin_lock_irq(&dev->event_lock);
	wait_event_lock_irq(vblank->work_wait_queue, list_empty(&work->node),
			    dev->event_lock);
	spin_unlock_irq(&dev->event_lock);

	kthread_flush_work(&work->base);
}
EXPORT_SYMBOL(drm_vblank_work_flush);

 
void drm_vblank_work_init(struct drm_vblank_work *work, struct drm_crtc *crtc,
			  void (*func)(struct kthread_work *work))
{
	kthread_init_work(&work->base, func);
	INIT_LIST_HEAD(&work->node);
	work->vblank = &crtc->dev->vblank[drm_crtc_index(crtc)];
}
EXPORT_SYMBOL(drm_vblank_work_init);

int drm_vblank_worker_init(struct drm_vblank_crtc *vblank)
{
	struct kthread_worker *worker;

	INIT_LIST_HEAD(&vblank->pending_work);
	init_waitqueue_head(&vblank->work_wait_queue);
	worker = kthread_create_worker(0, "card%d-crtc%d",
				       vblank->dev->primary->index,
				       vblank->pipe);
	if (IS_ERR(worker))
		return PTR_ERR(worker);

	vblank->worker = worker;

	sched_set_fifo(worker->task);
	return 0;
}
