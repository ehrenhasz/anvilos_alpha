
 

#include <linux/mmc/card.h>
#include <linux/mmc/host.h>
#include <linux/module.h>

#include "mmc_hsq.h"

static void mmc_hsq_retry_handler(struct work_struct *work)
{
	struct mmc_hsq *hsq = container_of(work, struct mmc_hsq, retry_work);
	struct mmc_host *mmc = hsq->mmc;

	mmc->ops->request(mmc, hsq->mrq);
}

static void mmc_hsq_pump_requests(struct mmc_hsq *hsq)
{
	struct mmc_host *mmc = hsq->mmc;
	struct hsq_slot *slot;
	unsigned long flags;
	int ret = 0;

	spin_lock_irqsave(&hsq->lock, flags);

	 
	if (hsq->mrq || hsq->recovery_halt) {
		spin_unlock_irqrestore(&hsq->lock, flags);
		return;
	}

	 
	if (!hsq->qcnt || !hsq->enabled) {
		spin_unlock_irqrestore(&hsq->lock, flags);
		return;
	}

	slot = &hsq->slot[hsq->next_tag];
	hsq->mrq = slot->mrq;
	hsq->qcnt--;

	spin_unlock_irqrestore(&hsq->lock, flags);

	if (mmc->ops->request_atomic)
		ret = mmc->ops->request_atomic(mmc, hsq->mrq);
	else
		mmc->ops->request(mmc, hsq->mrq);

	 
	if (ret == -EBUSY)
		schedule_work(&hsq->retry_work);
	else
		WARN_ON_ONCE(ret);
}

static void mmc_hsq_update_next_tag(struct mmc_hsq *hsq, int remains)
{
	int tag;

	 
	if (!remains) {
		hsq->next_tag = HSQ_INVALID_TAG;
		hsq->tail_tag = HSQ_INVALID_TAG;
		return;
	}

	tag = hsq->tag_slot[hsq->next_tag];
	hsq->tag_slot[hsq->next_tag] = HSQ_INVALID_TAG;
	hsq->next_tag = tag;
}

static void mmc_hsq_post_request(struct mmc_hsq *hsq)
{
	unsigned long flags;
	int remains;

	spin_lock_irqsave(&hsq->lock, flags);

	remains = hsq->qcnt;
	hsq->mrq = NULL;

	 
	mmc_hsq_update_next_tag(hsq, remains);

	if (hsq->waiting_for_idle && !remains) {
		hsq->waiting_for_idle = false;
		wake_up(&hsq->wait_queue);
	}

	 
	if (hsq->recovery_halt) {
		spin_unlock_irqrestore(&hsq->lock, flags);
		return;
	}

	spin_unlock_irqrestore(&hsq->lock, flags);

	  
	if (remains > 0)
		mmc_hsq_pump_requests(hsq);
}

 
bool mmc_hsq_finalize_request(struct mmc_host *mmc, struct mmc_request *mrq)
{
	struct mmc_hsq *hsq = mmc->cqe_private;
	unsigned long flags;

	spin_lock_irqsave(&hsq->lock, flags);

	if (!hsq->enabled || !hsq->mrq || hsq->mrq != mrq) {
		spin_unlock_irqrestore(&hsq->lock, flags);
		return false;
	}

	 
	hsq->slot[hsq->next_tag].mrq = NULL;

	spin_unlock_irqrestore(&hsq->lock, flags);

	mmc_cqe_request_done(mmc, hsq->mrq);

	mmc_hsq_post_request(hsq);

	return true;
}
EXPORT_SYMBOL_GPL(mmc_hsq_finalize_request);

static void mmc_hsq_recovery_start(struct mmc_host *mmc)
{
	struct mmc_hsq *hsq = mmc->cqe_private;
	unsigned long flags;

	spin_lock_irqsave(&hsq->lock, flags);

	hsq->recovery_halt = true;

	spin_unlock_irqrestore(&hsq->lock, flags);
}

static void mmc_hsq_recovery_finish(struct mmc_host *mmc)
{
	struct mmc_hsq *hsq = mmc->cqe_private;
	int remains;

	spin_lock_irq(&hsq->lock);

	hsq->recovery_halt = false;
	remains = hsq->qcnt;

	spin_unlock_irq(&hsq->lock);

	 
	if (remains > 0)
		mmc_hsq_pump_requests(hsq);
}

static int mmc_hsq_request(struct mmc_host *mmc, struct mmc_request *mrq)
{
	struct mmc_hsq *hsq = mmc->cqe_private;
	int tag = mrq->tag;

	spin_lock_irq(&hsq->lock);

	if (!hsq->enabled) {
		spin_unlock_irq(&hsq->lock);
		return -ESHUTDOWN;
	}

	 
	if (hsq->recovery_halt) {
		spin_unlock_irq(&hsq->lock);
		return -EBUSY;
	}

	hsq->slot[tag].mrq = mrq;

	 
	if (hsq->next_tag == HSQ_INVALID_TAG) {
		hsq->next_tag = tag;
		hsq->tail_tag = tag;
		hsq->tag_slot[hsq->tail_tag] = HSQ_INVALID_TAG;
	} else {
		hsq->tag_slot[hsq->tail_tag] = tag;
		hsq->tail_tag = tag;
	}

	hsq->qcnt++;

	spin_unlock_irq(&hsq->lock);

	mmc_hsq_pump_requests(hsq);

	return 0;
}

static void mmc_hsq_post_req(struct mmc_host *mmc, struct mmc_request *mrq)
{
	if (mmc->ops->post_req)
		mmc->ops->post_req(mmc, mrq, 0);
}

static bool mmc_hsq_queue_is_idle(struct mmc_hsq *hsq, int *ret)
{
	bool is_idle;

	spin_lock_irq(&hsq->lock);

	is_idle = (!hsq->mrq && !hsq->qcnt) ||
		hsq->recovery_halt;

	*ret = hsq->recovery_halt ? -EBUSY : 0;
	hsq->waiting_for_idle = !is_idle;

	spin_unlock_irq(&hsq->lock);

	return is_idle;
}

static int mmc_hsq_wait_for_idle(struct mmc_host *mmc)
{
	struct mmc_hsq *hsq = mmc->cqe_private;
	int ret;

	wait_event(hsq->wait_queue,
		   mmc_hsq_queue_is_idle(hsq, &ret));

	return ret;
}

static void mmc_hsq_disable(struct mmc_host *mmc)
{
	struct mmc_hsq *hsq = mmc->cqe_private;
	u32 timeout = 500;
	int ret;

	spin_lock_irq(&hsq->lock);

	if (!hsq->enabled) {
		spin_unlock_irq(&hsq->lock);
		return;
	}

	spin_unlock_irq(&hsq->lock);

	ret = wait_event_timeout(hsq->wait_queue,
				 mmc_hsq_queue_is_idle(hsq, &ret),
				 msecs_to_jiffies(timeout));
	if (ret == 0) {
		pr_warn("could not stop mmc software queue\n");
		return;
	}

	spin_lock_irq(&hsq->lock);

	hsq->enabled = false;

	spin_unlock_irq(&hsq->lock);
}

static int mmc_hsq_enable(struct mmc_host *mmc, struct mmc_card *card)
{
	struct mmc_hsq *hsq = mmc->cqe_private;

	spin_lock_irq(&hsq->lock);

	if (hsq->enabled) {
		spin_unlock_irq(&hsq->lock);
		return -EBUSY;
	}

	hsq->enabled = true;

	spin_unlock_irq(&hsq->lock);

	return 0;
}

static const struct mmc_cqe_ops mmc_hsq_ops = {
	.cqe_enable = mmc_hsq_enable,
	.cqe_disable = mmc_hsq_disable,
	.cqe_request = mmc_hsq_request,
	.cqe_post_req = mmc_hsq_post_req,
	.cqe_wait_for_idle = mmc_hsq_wait_for_idle,
	.cqe_recovery_start = mmc_hsq_recovery_start,
	.cqe_recovery_finish = mmc_hsq_recovery_finish,
};

int mmc_hsq_init(struct mmc_hsq *hsq, struct mmc_host *mmc)
{
	int i;
	hsq->num_slots = HSQ_NUM_SLOTS;
	hsq->next_tag = HSQ_INVALID_TAG;
	hsq->tail_tag = HSQ_INVALID_TAG;

	hsq->slot = devm_kcalloc(mmc_dev(mmc), hsq->num_slots,
				 sizeof(struct hsq_slot), GFP_KERNEL);
	if (!hsq->slot)
		return -ENOMEM;

	hsq->mmc = mmc;
	hsq->mmc->cqe_private = hsq;
	mmc->cqe_ops = &mmc_hsq_ops;

	for (i = 0; i < HSQ_NUM_SLOTS; i++)
		hsq->tag_slot[i] = HSQ_INVALID_TAG;

	INIT_WORK(&hsq->retry_work, mmc_hsq_retry_handler);
	spin_lock_init(&hsq->lock);
	init_waitqueue_head(&hsq->wait_queue);

	return 0;
}
EXPORT_SYMBOL_GPL(mmc_hsq_init);

void mmc_hsq_suspend(struct mmc_host *mmc)
{
	mmc_hsq_disable(mmc);
}
EXPORT_SYMBOL_GPL(mmc_hsq_suspend);

int mmc_hsq_resume(struct mmc_host *mmc)
{
	return mmc_hsq_enable(mmc, NULL);
}
EXPORT_SYMBOL_GPL(mmc_hsq_resume);

MODULE_DESCRIPTION("MMC Host Software Queue support");
MODULE_LICENSE("GPL v2");
