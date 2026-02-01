
 

#include "rxe.h"

static struct workqueue_struct *rxe_wq;

int rxe_alloc_wq(void)
{
	rxe_wq = alloc_workqueue("rxe_wq", WQ_UNBOUND, WQ_MAX_ACTIVE);
	if (!rxe_wq)
		return -ENOMEM;

	return 0;
}

void rxe_destroy_wq(void)
{
	destroy_workqueue(rxe_wq);
}

 
static bool __reserve_if_idle(struct rxe_task *task)
{
	WARN_ON(rxe_read(task->qp) <= 0);

	if (task->state == TASK_STATE_IDLE) {
		rxe_get(task->qp);
		task->state = TASK_STATE_BUSY;
		task->num_sched++;
		return true;
	}

	if (task->state == TASK_STATE_BUSY)
		task->state = TASK_STATE_ARMED;

	return false;
}

 
static bool __is_done(struct rxe_task *task)
{
	if (work_pending(&task->work))
		return false;

	if (task->state == TASK_STATE_IDLE ||
	    task->state == TASK_STATE_DRAINED) {
		return true;
	}

	return false;
}

 
static bool is_done(struct rxe_task *task)
{
	unsigned long flags;
	int done;

	spin_lock_irqsave(&task->lock, flags);
	done = __is_done(task);
	spin_unlock_irqrestore(&task->lock, flags);

	return done;
}

 
static void do_task(struct rxe_task *task)
{
	unsigned int iterations;
	unsigned long flags;
	int resched = 0;
	int cont;
	int ret;

	WARN_ON(rxe_read(task->qp) <= 0);

	spin_lock_irqsave(&task->lock, flags);
	if (task->state >= TASK_STATE_DRAINED) {
		rxe_put(task->qp);
		task->num_done++;
		spin_unlock_irqrestore(&task->lock, flags);
		return;
	}
	spin_unlock_irqrestore(&task->lock, flags);

	do {
		iterations = RXE_MAX_ITERATIONS;
		cont = 0;

		do {
			ret = task->func(task->qp);
		} while (ret == 0 && iterations-- > 0);

		spin_lock_irqsave(&task->lock, flags);
		 
		if (!ret) {
			task->state = TASK_STATE_IDLE;
			resched = 1;
			goto exit;
		}

		switch (task->state) {
		case TASK_STATE_BUSY:
			task->state = TASK_STATE_IDLE;
			break;

		 
		case TASK_STATE_ARMED:
			task->state = TASK_STATE_BUSY;
			cont = 1;
			break;

		case TASK_STATE_DRAINING:
			task->state = TASK_STATE_DRAINED;
			break;

		default:
			WARN_ON(1);
			rxe_dbg_qp(task->qp, "unexpected task state = %d",
				   task->state);
			task->state = TASK_STATE_IDLE;
		}

exit:
		if (!cont) {
			task->num_done++;
			if (WARN_ON(task->num_done != task->num_sched))
				rxe_dbg_qp(
					task->qp,
					"%ld tasks scheduled, %ld tasks done",
					task->num_sched, task->num_done);
		}
		spin_unlock_irqrestore(&task->lock, flags);
	} while (cont);

	task->ret = ret;

	if (resched)
		rxe_sched_task(task);

	rxe_put(task->qp);
}

 
static void do_work(struct work_struct *work)
{
	do_task(container_of(work, struct rxe_task, work));
}

int rxe_init_task(struct rxe_task *task, struct rxe_qp *qp,
		  int (*func)(struct rxe_qp *))
{
	WARN_ON(rxe_read(qp) <= 0);

	task->qp = qp;
	task->func = func;
	task->state = TASK_STATE_IDLE;
	spin_lock_init(&task->lock);
	INIT_WORK(&task->work, do_work);

	return 0;
}

 
void rxe_cleanup_task(struct rxe_task *task)
{
	unsigned long flags;

	spin_lock_irqsave(&task->lock, flags);
	if (!__is_done(task) && task->state < TASK_STATE_DRAINED) {
		task->state = TASK_STATE_DRAINING;
	} else {
		task->state = TASK_STATE_INVALID;
		spin_unlock_irqrestore(&task->lock, flags);
		return;
	}
	spin_unlock_irqrestore(&task->lock, flags);

	 
	while (!is_done(task))
		cond_resched();

	spin_lock_irqsave(&task->lock, flags);
	task->state = TASK_STATE_INVALID;
	spin_unlock_irqrestore(&task->lock, flags);
}

 
void rxe_run_task(struct rxe_task *task)
{
	unsigned long flags;
	bool run;

	WARN_ON(rxe_read(task->qp) <= 0);

	spin_lock_irqsave(&task->lock, flags);
	run = __reserve_if_idle(task);
	spin_unlock_irqrestore(&task->lock, flags);

	if (run)
		do_task(task);
}

 
void rxe_sched_task(struct rxe_task *task)
{
	unsigned long flags;

	WARN_ON(rxe_read(task->qp) <= 0);

	spin_lock_irqsave(&task->lock, flags);
	if (__reserve_if_idle(task))
		queue_work(rxe_wq, &task->work);
	spin_unlock_irqrestore(&task->lock, flags);
}

 
void rxe_disable_task(struct rxe_task *task)
{
	unsigned long flags;

	WARN_ON(rxe_read(task->qp) <= 0);

	spin_lock_irqsave(&task->lock, flags);
	if (!__is_done(task) && task->state < TASK_STATE_DRAINED) {
		task->state = TASK_STATE_DRAINING;
	} else {
		task->state = TASK_STATE_DRAINED;
		spin_unlock_irqrestore(&task->lock, flags);
		return;
	}
	spin_unlock_irqrestore(&task->lock, flags);

	while (!is_done(task))
		cond_resched();

	spin_lock_irqsave(&task->lock, flags);
	task->state = TASK_STATE_DRAINED;
	spin_unlock_irqrestore(&task->lock, flags);
}

void rxe_enable_task(struct rxe_task *task)
{
	unsigned long flags;

	WARN_ON(rxe_read(task->qp) <= 0);

	spin_lock_irqsave(&task->lock, flags);
	if (task->state == TASK_STATE_INVALID) {
		spin_unlock_irqrestore(&task->lock, flags);
		return;
	}

	task->state = TASK_STATE_IDLE;
	spin_unlock_irqrestore(&task->lock, flags);
}
