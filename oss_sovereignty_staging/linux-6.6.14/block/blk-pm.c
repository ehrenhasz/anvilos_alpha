

#include <linux/blk-pm.h>
#include <linux/blkdev.h>
#include <linux/pm_runtime.h>
#include "blk-mq.h"

 
void blk_pm_runtime_init(struct request_queue *q, struct device *dev)
{
	q->dev = dev;
	q->rpm_status = RPM_ACTIVE;
	pm_runtime_set_autosuspend_delay(q->dev, -1);
	pm_runtime_use_autosuspend(q->dev);
}
EXPORT_SYMBOL(blk_pm_runtime_init);

 
int blk_pre_runtime_suspend(struct request_queue *q)
{
	int ret = 0;

	if (!q->dev)
		return ret;

	WARN_ON_ONCE(q->rpm_status != RPM_ACTIVE);

	spin_lock_irq(&q->queue_lock);
	q->rpm_status = RPM_SUSPENDING;
	spin_unlock_irq(&q->queue_lock);

	 
	blk_set_pm_only(q);
	ret = -EBUSY;
	 
	blk_freeze_queue_start(q);
	 
	percpu_ref_switch_to_atomic_sync(&q->q_usage_counter);
	if (percpu_ref_is_zero(&q->q_usage_counter))
		ret = 0;
	 
	blk_mq_unfreeze_queue(q);

	if (ret < 0) {
		spin_lock_irq(&q->queue_lock);
		q->rpm_status = RPM_ACTIVE;
		pm_runtime_mark_last_busy(q->dev);
		spin_unlock_irq(&q->queue_lock);

		blk_clear_pm_only(q);
	}

	return ret;
}
EXPORT_SYMBOL(blk_pre_runtime_suspend);

 
void blk_post_runtime_suspend(struct request_queue *q, int err)
{
	if (!q->dev)
		return;

	spin_lock_irq(&q->queue_lock);
	if (!err) {
		q->rpm_status = RPM_SUSPENDED;
	} else {
		q->rpm_status = RPM_ACTIVE;
		pm_runtime_mark_last_busy(q->dev);
	}
	spin_unlock_irq(&q->queue_lock);

	if (err)
		blk_clear_pm_only(q);
}
EXPORT_SYMBOL(blk_post_runtime_suspend);

 
void blk_pre_runtime_resume(struct request_queue *q)
{
	if (!q->dev)
		return;

	spin_lock_irq(&q->queue_lock);
	q->rpm_status = RPM_RESUMING;
	spin_unlock_irq(&q->queue_lock);
}
EXPORT_SYMBOL(blk_pre_runtime_resume);

 
void blk_post_runtime_resume(struct request_queue *q)
{
	blk_set_runtime_active(q);
}
EXPORT_SYMBOL(blk_post_runtime_resume);

 
void blk_set_runtime_active(struct request_queue *q)
{
	int old_status;

	if (!q->dev)
		return;

	spin_lock_irq(&q->queue_lock);
	old_status = q->rpm_status;
	q->rpm_status = RPM_ACTIVE;
	pm_runtime_mark_last_busy(q->dev);
	pm_request_autosuspend(q->dev);
	spin_unlock_irq(&q->queue_lock);

	if (old_status != RPM_ACTIVE)
		blk_clear_pm_only(q);
}
EXPORT_SYMBOL(blk_set_runtime_active);
