
 

#include <linux/module.h>

#include <linux/sched.h>
#include <linux/interrupt.h>
#include <linux/slab.h>
#include <linux/mempool.h>
#include <linux/smp.h>
#include <linux/spinlock.h>
#include <linux/mutex.h>
#include <linux/freezer.h>
#include <linux/sched/mm.h>

#include <linux/sunrpc/clnt.h>
#include <linux/sunrpc/metrics.h>

#include "sunrpc.h"

#define CREATE_TRACE_POINTS
#include <trace/events/sunrpc.h>

 
#define RPC_BUFFER_MAXSIZE	(2048)
#define RPC_BUFFER_POOLSIZE	(8)
#define RPC_TASK_POOLSIZE	(8)
static struct kmem_cache	*rpc_task_slabp __read_mostly;
static struct kmem_cache	*rpc_buffer_slabp __read_mostly;
static mempool_t	*rpc_task_mempool __read_mostly;
static mempool_t	*rpc_buffer_mempool __read_mostly;

static void			rpc_async_schedule(struct work_struct *);
static void			 rpc_release_task(struct rpc_task *task);
static void __rpc_queue_timer_fn(struct work_struct *);

 
static struct rpc_wait_queue delay_queue;

 
struct workqueue_struct *rpciod_workqueue __read_mostly;
struct workqueue_struct *xprtiod_workqueue __read_mostly;
EXPORT_SYMBOL_GPL(xprtiod_workqueue);

gfp_t rpc_task_gfp_mask(void)
{
	if (current->flags & PF_WQ_WORKER)
		return GFP_KERNEL | __GFP_NORETRY | __GFP_NOWARN;
	return GFP_KERNEL;
}
EXPORT_SYMBOL_GPL(rpc_task_gfp_mask);

bool rpc_task_set_rpc_status(struct rpc_task *task, int rpc_status)
{
	if (cmpxchg(&task->tk_rpc_status, 0, rpc_status) == 0)
		return true;
	return false;
}

unsigned long
rpc_task_timeout(const struct rpc_task *task)
{
	unsigned long timeout = READ_ONCE(task->tk_timeout);

	if (timeout != 0) {
		unsigned long now = jiffies;
		if (time_before(now, timeout))
			return timeout - now;
	}
	return 0;
}
EXPORT_SYMBOL_GPL(rpc_task_timeout);

 
static void
__rpc_disable_timer(struct rpc_wait_queue *queue, struct rpc_task *task)
{
	if (list_empty(&task->u.tk_wait.timer_list))
		return;
	task->tk_timeout = 0;
	list_del(&task->u.tk_wait.timer_list);
	if (list_empty(&queue->timer_list.list))
		cancel_delayed_work(&queue->timer_list.dwork);
}

static void
rpc_set_queue_timer(struct rpc_wait_queue *queue, unsigned long expires)
{
	unsigned long now = jiffies;
	queue->timer_list.expires = expires;
	if (time_before_eq(expires, now))
		expires = 0;
	else
		expires -= now;
	mod_delayed_work(rpciod_workqueue, &queue->timer_list.dwork, expires);
}

 
static void
__rpc_add_timer(struct rpc_wait_queue *queue, struct rpc_task *task,
		unsigned long timeout)
{
	task->tk_timeout = timeout;
	if (list_empty(&queue->timer_list.list) || time_before(timeout, queue->timer_list.expires))
		rpc_set_queue_timer(queue, timeout);
	list_add(&task->u.tk_wait.timer_list, &queue->timer_list.list);
}

static void rpc_set_waitqueue_priority(struct rpc_wait_queue *queue, int priority)
{
	if (queue->priority != priority) {
		queue->priority = priority;
		queue->nr = 1U << priority;
	}
}

static void rpc_reset_waitqueue_priority(struct rpc_wait_queue *queue)
{
	rpc_set_waitqueue_priority(queue, queue->maxpriority);
}

 
static void
__rpc_list_enqueue_task(struct list_head *q, struct rpc_task *task)
{
	struct rpc_task *t;

	list_for_each_entry(t, q, u.tk_wait.list) {
		if (t->tk_owner == task->tk_owner) {
			list_add_tail(&task->u.tk_wait.links,
					&t->u.tk_wait.links);
			 
			task->u.tk_wait.list.next = q;
			task->u.tk_wait.list.prev = NULL;
			return;
		}
	}
	INIT_LIST_HEAD(&task->u.tk_wait.links);
	list_add_tail(&task->u.tk_wait.list, q);
}

 
static void
__rpc_list_dequeue_task(struct rpc_task *task)
{
	struct list_head *q;
	struct rpc_task *t;

	if (task->u.tk_wait.list.prev == NULL) {
		list_del(&task->u.tk_wait.links);
		return;
	}
	if (!list_empty(&task->u.tk_wait.links)) {
		t = list_first_entry(&task->u.tk_wait.links,
				struct rpc_task,
				u.tk_wait.links);
		 
		q = t->u.tk_wait.list.next;
		list_add_tail(&t->u.tk_wait.list, q);
		list_del(&task->u.tk_wait.links);
	}
	list_del(&task->u.tk_wait.list);
}

 
static void __rpc_add_wait_queue_priority(struct rpc_wait_queue *queue,
		struct rpc_task *task,
		unsigned char queue_priority)
{
	if (unlikely(queue_priority > queue->maxpriority))
		queue_priority = queue->maxpriority;
	__rpc_list_enqueue_task(&queue->tasks[queue_priority], task);
}

 
static void __rpc_add_wait_queue(struct rpc_wait_queue *queue,
		struct rpc_task *task,
		unsigned char queue_priority)
{
	INIT_LIST_HEAD(&task->u.tk_wait.timer_list);
	if (RPC_IS_PRIORITY(queue))
		__rpc_add_wait_queue_priority(queue, task, queue_priority);
	else
		list_add_tail(&task->u.tk_wait.list, &queue->tasks[0]);
	task->tk_waitqueue = queue;
	queue->qlen++;
	 
	smp_wmb();
	rpc_set_queued(task);
}

 
static void __rpc_remove_wait_queue_priority(struct rpc_task *task)
{
	__rpc_list_dequeue_task(task);
}

 
static void __rpc_remove_wait_queue(struct rpc_wait_queue *queue, struct rpc_task *task)
{
	__rpc_disable_timer(queue, task);
	if (RPC_IS_PRIORITY(queue))
		__rpc_remove_wait_queue_priority(task);
	else
		list_del(&task->u.tk_wait.list);
	queue->qlen--;
}

static void __rpc_init_priority_wait_queue(struct rpc_wait_queue *queue, const char *qname, unsigned char nr_queues)
{
	int i;

	spin_lock_init(&queue->lock);
	for (i = 0; i < ARRAY_SIZE(queue->tasks); i++)
		INIT_LIST_HEAD(&queue->tasks[i]);
	queue->maxpriority = nr_queues - 1;
	rpc_reset_waitqueue_priority(queue);
	queue->qlen = 0;
	queue->timer_list.expires = 0;
	INIT_DELAYED_WORK(&queue->timer_list.dwork, __rpc_queue_timer_fn);
	INIT_LIST_HEAD(&queue->timer_list.list);
	rpc_assign_waitqueue_name(queue, qname);
}

void rpc_init_priority_wait_queue(struct rpc_wait_queue *queue, const char *qname)
{
	__rpc_init_priority_wait_queue(queue, qname, RPC_NR_PRIORITY);
}
EXPORT_SYMBOL_GPL(rpc_init_priority_wait_queue);

void rpc_init_wait_queue(struct rpc_wait_queue *queue, const char *qname)
{
	__rpc_init_priority_wait_queue(queue, qname, 1);
}
EXPORT_SYMBOL_GPL(rpc_init_wait_queue);

void rpc_destroy_wait_queue(struct rpc_wait_queue *queue)
{
	cancel_delayed_work_sync(&queue->timer_list.dwork);
}
EXPORT_SYMBOL_GPL(rpc_destroy_wait_queue);

static int rpc_wait_bit_killable(struct wait_bit_key *key, int mode)
{
	schedule();
	if (signal_pending_state(mode, current))
		return -ERESTARTSYS;
	return 0;
}

#if IS_ENABLED(CONFIG_SUNRPC_DEBUG) || IS_ENABLED(CONFIG_TRACEPOINTS)
static void rpc_task_set_debuginfo(struct rpc_task *task)
{
	struct rpc_clnt *clnt = task->tk_client;

	 
	if (!clnt) {
		static atomic_t rpc_pid;

		task->tk_pid = atomic_inc_return(&rpc_pid);
		return;
	}

	task->tk_pid = atomic_inc_return(&clnt->cl_pid);
}
#else
static inline void rpc_task_set_debuginfo(struct rpc_task *task)
{
}
#endif

static void rpc_set_active(struct rpc_task *task)
{
	rpc_task_set_debuginfo(task);
	set_bit(RPC_TASK_ACTIVE, &task->tk_runstate);
	trace_rpc_task_begin(task, NULL);
}

 
static int rpc_complete_task(struct rpc_task *task)
{
	void *m = &task->tk_runstate;
	wait_queue_head_t *wq = bit_waitqueue(m, RPC_TASK_ACTIVE);
	struct wait_bit_key k = __WAIT_BIT_KEY_INITIALIZER(m, RPC_TASK_ACTIVE);
	unsigned long flags;
	int ret;

	trace_rpc_task_complete(task, NULL);

	spin_lock_irqsave(&wq->lock, flags);
	clear_bit(RPC_TASK_ACTIVE, &task->tk_runstate);
	ret = atomic_dec_and_test(&task->tk_count);
	if (waitqueue_active(wq))
		__wake_up_locked_key(wq, TASK_NORMAL, &k);
	spin_unlock_irqrestore(&wq->lock, flags);
	return ret;
}

 
int rpc_wait_for_completion_task(struct rpc_task *task)
{
	return out_of_line_wait_on_bit(&task->tk_runstate, RPC_TASK_ACTIVE,
			rpc_wait_bit_killable, TASK_KILLABLE|TASK_FREEZABLE_UNSAFE);
}
EXPORT_SYMBOL_GPL(rpc_wait_for_completion_task);

 
static void rpc_make_runnable(struct workqueue_struct *wq,
		struct rpc_task *task)
{
	bool need_wakeup = !rpc_test_and_set_running(task);

	rpc_clear_queued(task);
	if (!need_wakeup)
		return;
	if (RPC_IS_ASYNC(task)) {
		INIT_WORK(&task->u.tk_work, rpc_async_schedule);
		queue_work(wq, &task->u.tk_work);
	} else
		wake_up_bit(&task->tk_runstate, RPC_TASK_QUEUED);
}

 
static void __rpc_do_sleep_on_priority(struct rpc_wait_queue *q,
		struct rpc_task *task,
		unsigned char queue_priority)
{
	trace_rpc_task_sleep(task, q);

	__rpc_add_wait_queue(q, task, queue_priority);
}

static void __rpc_sleep_on_priority(struct rpc_wait_queue *q,
		struct rpc_task *task,
		unsigned char queue_priority)
{
	if (WARN_ON_ONCE(RPC_IS_QUEUED(task)))
		return;
	__rpc_do_sleep_on_priority(q, task, queue_priority);
}

static void __rpc_sleep_on_priority_timeout(struct rpc_wait_queue *q,
		struct rpc_task *task, unsigned long timeout,
		unsigned char queue_priority)
{
	if (WARN_ON_ONCE(RPC_IS_QUEUED(task)))
		return;
	if (time_is_after_jiffies(timeout)) {
		__rpc_do_sleep_on_priority(q, task, queue_priority);
		__rpc_add_timer(q, task, timeout);
	} else
		task->tk_status = -ETIMEDOUT;
}

static void rpc_set_tk_callback(struct rpc_task *task, rpc_action action)
{
	if (action && !WARN_ON_ONCE(task->tk_callback != NULL))
		task->tk_callback = action;
}

static bool rpc_sleep_check_activated(struct rpc_task *task)
{
	 
	if (WARN_ON_ONCE(!RPC_IS_ACTIVATED(task))) {
		task->tk_status = -EIO;
		rpc_put_task_async(task);
		return false;
	}
	return true;
}

void rpc_sleep_on_timeout(struct rpc_wait_queue *q, struct rpc_task *task,
				rpc_action action, unsigned long timeout)
{
	if (!rpc_sleep_check_activated(task))
		return;

	rpc_set_tk_callback(task, action);

	 
	spin_lock(&q->lock);
	__rpc_sleep_on_priority_timeout(q, task, timeout, task->tk_priority);
	spin_unlock(&q->lock);
}
EXPORT_SYMBOL_GPL(rpc_sleep_on_timeout);

void rpc_sleep_on(struct rpc_wait_queue *q, struct rpc_task *task,
				rpc_action action)
{
	if (!rpc_sleep_check_activated(task))
		return;

	rpc_set_tk_callback(task, action);

	WARN_ON_ONCE(task->tk_timeout != 0);
	 
	spin_lock(&q->lock);
	__rpc_sleep_on_priority(q, task, task->tk_priority);
	spin_unlock(&q->lock);
}
EXPORT_SYMBOL_GPL(rpc_sleep_on);

void rpc_sleep_on_priority_timeout(struct rpc_wait_queue *q,
		struct rpc_task *task, unsigned long timeout, int priority)
{
	if (!rpc_sleep_check_activated(task))
		return;

	priority -= RPC_PRIORITY_LOW;
	 
	spin_lock(&q->lock);
	__rpc_sleep_on_priority_timeout(q, task, timeout, priority);
	spin_unlock(&q->lock);
}
EXPORT_SYMBOL_GPL(rpc_sleep_on_priority_timeout);

void rpc_sleep_on_priority(struct rpc_wait_queue *q, struct rpc_task *task,
		int priority)
{
	if (!rpc_sleep_check_activated(task))
		return;

	WARN_ON_ONCE(task->tk_timeout != 0);
	priority -= RPC_PRIORITY_LOW;
	 
	spin_lock(&q->lock);
	__rpc_sleep_on_priority(q, task, priority);
	spin_unlock(&q->lock);
}
EXPORT_SYMBOL_GPL(rpc_sleep_on_priority);

 
static void __rpc_do_wake_up_task_on_wq(struct workqueue_struct *wq,
		struct rpc_wait_queue *queue,
		struct rpc_task *task)
{
	 
	if (!RPC_IS_ACTIVATED(task)) {
		printk(KERN_ERR "RPC: Inactive task (%p) being woken up!\n", task);
		return;
	}

	trace_rpc_task_wakeup(task, queue);

	__rpc_remove_wait_queue(queue, task);

	rpc_make_runnable(wq, task);
}

 
static struct rpc_task *
rpc_wake_up_task_on_wq_queue_action_locked(struct workqueue_struct *wq,
		struct rpc_wait_queue *queue, struct rpc_task *task,
		bool (*action)(struct rpc_task *, void *), void *data)
{
	if (RPC_IS_QUEUED(task)) {
		smp_rmb();
		if (task->tk_waitqueue == queue) {
			if (action == NULL || action(task, data)) {
				__rpc_do_wake_up_task_on_wq(wq, queue, task);
				return task;
			}
		}
	}
	return NULL;
}

 
static void rpc_wake_up_task_queue_locked(struct rpc_wait_queue *queue,
					  struct rpc_task *task)
{
	rpc_wake_up_task_on_wq_queue_action_locked(rpciod_workqueue, queue,
						   task, NULL, NULL);
}

 
void rpc_wake_up_queued_task(struct rpc_wait_queue *queue, struct rpc_task *task)
{
	if (!RPC_IS_QUEUED(task))
		return;
	spin_lock(&queue->lock);
	rpc_wake_up_task_queue_locked(queue, task);
	spin_unlock(&queue->lock);
}
EXPORT_SYMBOL_GPL(rpc_wake_up_queued_task);

static bool rpc_task_action_set_status(struct rpc_task *task, void *status)
{
	task->tk_status = *(int *)status;
	return true;
}

static void
rpc_wake_up_task_queue_set_status_locked(struct rpc_wait_queue *queue,
		struct rpc_task *task, int status)
{
	rpc_wake_up_task_on_wq_queue_action_locked(rpciod_workqueue, queue,
			task, rpc_task_action_set_status, &status);
}

 
void
rpc_wake_up_queued_task_set_status(struct rpc_wait_queue *queue,
		struct rpc_task *task, int status)
{
	if (!RPC_IS_QUEUED(task))
		return;
	spin_lock(&queue->lock);
	rpc_wake_up_task_queue_set_status_locked(queue, task, status);
	spin_unlock(&queue->lock);
}

 
static struct rpc_task *__rpc_find_next_queued_priority(struct rpc_wait_queue *queue)
{
	struct list_head *q;
	struct rpc_task *task;

	 
	q = &queue->tasks[RPC_NR_PRIORITY - 1];
	if (queue->maxpriority > RPC_PRIORITY_PRIVILEGED && !list_empty(q)) {
		task = list_first_entry(q, struct rpc_task, u.tk_wait.list);
		goto out;
	}

	 
	q = &queue->tasks[queue->priority];
	if (!list_empty(q) && queue->nr) {
		queue->nr--;
		task = list_first_entry(q, struct rpc_task, u.tk_wait.list);
		goto out;
	}

	 
	do {
		if (q == &queue->tasks[0])
			q = &queue->tasks[queue->maxpriority];
		else
			q = q - 1;
		if (!list_empty(q)) {
			task = list_first_entry(q, struct rpc_task, u.tk_wait.list);
			goto new_queue;
		}
	} while (q != &queue->tasks[queue->priority]);

	rpc_reset_waitqueue_priority(queue);
	return NULL;

new_queue:
	rpc_set_waitqueue_priority(queue, (unsigned int)(q - &queue->tasks[0]));
out:
	return task;
}

static struct rpc_task *__rpc_find_next_queued(struct rpc_wait_queue *queue)
{
	if (RPC_IS_PRIORITY(queue))
		return __rpc_find_next_queued_priority(queue);
	if (!list_empty(&queue->tasks[0]))
		return list_first_entry(&queue->tasks[0], struct rpc_task, u.tk_wait.list);
	return NULL;
}

 
struct rpc_task *rpc_wake_up_first_on_wq(struct workqueue_struct *wq,
		struct rpc_wait_queue *queue,
		bool (*func)(struct rpc_task *, void *), void *data)
{
	struct rpc_task	*task = NULL;

	spin_lock(&queue->lock);
	task = __rpc_find_next_queued(queue);
	if (task != NULL)
		task = rpc_wake_up_task_on_wq_queue_action_locked(wq, queue,
				task, func, data);
	spin_unlock(&queue->lock);

	return task;
}

 
struct rpc_task *rpc_wake_up_first(struct rpc_wait_queue *queue,
		bool (*func)(struct rpc_task *, void *), void *data)
{
	return rpc_wake_up_first_on_wq(rpciod_workqueue, queue, func, data);
}
EXPORT_SYMBOL_GPL(rpc_wake_up_first);

static bool rpc_wake_up_next_func(struct rpc_task *task, void *data)
{
	return true;
}

 
struct rpc_task *rpc_wake_up_next(struct rpc_wait_queue *queue)
{
	return rpc_wake_up_first(queue, rpc_wake_up_next_func, NULL);
}
EXPORT_SYMBOL_GPL(rpc_wake_up_next);

 
static void rpc_wake_up_locked(struct rpc_wait_queue *queue)
{
	struct rpc_task *task;

	for (;;) {
		task = __rpc_find_next_queued(queue);
		if (task == NULL)
			break;
		rpc_wake_up_task_queue_locked(queue, task);
	}
}

 
void rpc_wake_up(struct rpc_wait_queue *queue)
{
	spin_lock(&queue->lock);
	rpc_wake_up_locked(queue);
	spin_unlock(&queue->lock);
}
EXPORT_SYMBOL_GPL(rpc_wake_up);

 
static void rpc_wake_up_status_locked(struct rpc_wait_queue *queue, int status)
{
	struct rpc_task *task;

	for (;;) {
		task = __rpc_find_next_queued(queue);
		if (task == NULL)
			break;
		rpc_wake_up_task_queue_set_status_locked(queue, task, status);
	}
}

 
void rpc_wake_up_status(struct rpc_wait_queue *queue, int status)
{
	spin_lock(&queue->lock);
	rpc_wake_up_status_locked(queue, status);
	spin_unlock(&queue->lock);
}
EXPORT_SYMBOL_GPL(rpc_wake_up_status);

static void __rpc_queue_timer_fn(struct work_struct *work)
{
	struct rpc_wait_queue *queue = container_of(work,
			struct rpc_wait_queue,
			timer_list.dwork.work);
	struct rpc_task *task, *n;
	unsigned long expires, now, timeo;

	spin_lock(&queue->lock);
	expires = now = jiffies;
	list_for_each_entry_safe(task, n, &queue->timer_list.list, u.tk_wait.timer_list) {
		timeo = task->tk_timeout;
		if (time_after_eq(now, timeo)) {
			trace_rpc_task_timeout(task, task->tk_action);
			task->tk_status = -ETIMEDOUT;
			rpc_wake_up_task_queue_locked(queue, task);
			continue;
		}
		if (expires == now || time_after(expires, timeo))
			expires = timeo;
	}
	if (!list_empty(&queue->timer_list.list))
		rpc_set_queue_timer(queue, expires);
	spin_unlock(&queue->lock);
}

static void __rpc_atrun(struct rpc_task *task)
{
	if (task->tk_status == -ETIMEDOUT)
		task->tk_status = 0;
}

 
void rpc_delay(struct rpc_task *task, unsigned long delay)
{
	rpc_sleep_on_timeout(&delay_queue, task, __rpc_atrun, jiffies + delay);
}
EXPORT_SYMBOL_GPL(rpc_delay);

 
void rpc_prepare_task(struct rpc_task *task)
{
	task->tk_ops->rpc_call_prepare(task, task->tk_calldata);
}

static void
rpc_init_task_statistics(struct rpc_task *task)
{
	 
	task->tk_garb_retry = 2;
	task->tk_cred_retry = 2;

	 
	task->tk_start = ktime_get();
}

static void
rpc_reset_task_statistics(struct rpc_task *task)
{
	task->tk_timeouts = 0;
	task->tk_flags &= ~(RPC_CALL_MAJORSEEN|RPC_TASK_SENT);
	rpc_init_task_statistics(task);
}

 
void rpc_exit_task(struct rpc_task *task)
{
	trace_rpc_task_end(task, task->tk_action);
	task->tk_action = NULL;
	if (task->tk_ops->rpc_count_stats)
		task->tk_ops->rpc_count_stats(task, task->tk_calldata);
	else if (task->tk_client)
		rpc_count_iostats(task, task->tk_client->cl_metrics);
	if (task->tk_ops->rpc_call_done != NULL) {
		trace_rpc_task_call_done(task, task->tk_ops->rpc_call_done);
		task->tk_ops->rpc_call_done(task, task->tk_calldata);
		if (task->tk_action != NULL) {
			 
			xprt_release(task);
			rpc_reset_task_statistics(task);
		}
	}
}

void rpc_signal_task(struct rpc_task *task)
{
	struct rpc_wait_queue *queue;

	if (!RPC_IS_ACTIVATED(task))
		return;

	if (!rpc_task_set_rpc_status(task, -ERESTARTSYS))
		return;
	trace_rpc_task_signalled(task, task->tk_action);
	set_bit(RPC_TASK_SIGNALLED, &task->tk_runstate);
	smp_mb__after_atomic();
	queue = READ_ONCE(task->tk_waitqueue);
	if (queue)
		rpc_wake_up_queued_task(queue, task);
}

void rpc_task_try_cancel(struct rpc_task *task, int error)
{
	struct rpc_wait_queue *queue;

	if (!rpc_task_set_rpc_status(task, error))
		return;
	queue = READ_ONCE(task->tk_waitqueue);
	if (queue)
		rpc_wake_up_queued_task(queue, task);
}

void rpc_exit(struct rpc_task *task, int status)
{
	task->tk_status = status;
	task->tk_action = rpc_exit_task;
	rpc_wake_up_queued_task(task->tk_waitqueue, task);
}
EXPORT_SYMBOL_GPL(rpc_exit);

void rpc_release_calldata(const struct rpc_call_ops *ops, void *calldata)
{
	if (ops->rpc_release != NULL)
		ops->rpc_release(calldata);
}

static bool xprt_needs_memalloc(struct rpc_xprt *xprt, struct rpc_task *tk)
{
	if (!xprt)
		return false;
	if (!atomic_read(&xprt->swapper))
		return false;
	return test_bit(XPRT_LOCKED, &xprt->state) && xprt->snd_task == tk;
}

 
static void __rpc_execute(struct rpc_task *task)
{
	struct rpc_wait_queue *queue;
	int task_is_async = RPC_IS_ASYNC(task);
	int status = 0;
	unsigned long pflags = current->flags;

	WARN_ON_ONCE(RPC_IS_QUEUED(task));
	if (RPC_IS_QUEUED(task))
		return;

	for (;;) {
		void (*do_action)(struct rpc_task *);

		 
		do_action = task->tk_action;
		 
		if (do_action && do_action != rpc_exit_task &&
		    (status = READ_ONCE(task->tk_rpc_status)) != 0) {
			task->tk_status = status;
			do_action = rpc_exit_task;
		}
		 
		if (task->tk_callback) {
			do_action = task->tk_callback;
			task->tk_callback = NULL;
		}
		if (!do_action)
			break;
		if (RPC_IS_SWAPPER(task) ||
		    xprt_needs_memalloc(task->tk_xprt, task))
			current->flags |= PF_MEMALLOC;

		trace_rpc_task_run_action(task, do_action);
		do_action(task);

		 
		if (!RPC_IS_QUEUED(task)) {
			cond_resched();
			continue;
		}

		 
		queue = task->tk_waitqueue;
		spin_lock(&queue->lock);
		if (!RPC_IS_QUEUED(task)) {
			spin_unlock(&queue->lock);
			continue;
		}
		 
		if (READ_ONCE(task->tk_rpc_status) != 0) {
			rpc_wake_up_task_queue_locked(queue, task);
			spin_unlock(&queue->lock);
			continue;
		}
		rpc_clear_running(task);
		spin_unlock(&queue->lock);
		if (task_is_async)
			goto out;

		 
		trace_rpc_task_sync_sleep(task, task->tk_action);
		status = out_of_line_wait_on_bit(&task->tk_runstate,
				RPC_TASK_QUEUED, rpc_wait_bit_killable,
				TASK_KILLABLE|TASK_FREEZABLE);
		if (status < 0) {
			 
			rpc_signal_task(task);
		}
		trace_rpc_task_sync_wake(task, task->tk_action);
	}

	 
	rpc_release_task(task);
out:
	current_restore_flags(pflags, PF_MEMALLOC);
}

 
void rpc_execute(struct rpc_task *task)
{
	bool is_async = RPC_IS_ASYNC(task);

	rpc_set_active(task);
	rpc_make_runnable(rpciod_workqueue, task);
	if (!is_async) {
		unsigned int pflags = memalloc_nofs_save();
		__rpc_execute(task);
		memalloc_nofs_restore(pflags);
	}
}

static void rpc_async_schedule(struct work_struct *work)
{
	unsigned int pflags = memalloc_nofs_save();

	__rpc_execute(container_of(work, struct rpc_task, u.tk_work));
	memalloc_nofs_restore(pflags);
}

 
int rpc_malloc(struct rpc_task *task)
{
	struct rpc_rqst *rqst = task->tk_rqstp;
	size_t size = rqst->rq_callsize + rqst->rq_rcvsize;
	struct rpc_buffer *buf;
	gfp_t gfp = rpc_task_gfp_mask();

	size += sizeof(struct rpc_buffer);
	if (size <= RPC_BUFFER_MAXSIZE) {
		buf = kmem_cache_alloc(rpc_buffer_slabp, gfp);
		 
		if (!buf && RPC_IS_ASYNC(task))
			buf = mempool_alloc(rpc_buffer_mempool, GFP_NOWAIT);
	} else
		buf = kmalloc(size, gfp);

	if (!buf)
		return -ENOMEM;

	buf->len = size;
	rqst->rq_buffer = buf->data;
	rqst->rq_rbuffer = (char *)rqst->rq_buffer + rqst->rq_callsize;
	return 0;
}
EXPORT_SYMBOL_GPL(rpc_malloc);

 
void rpc_free(struct rpc_task *task)
{
	void *buffer = task->tk_rqstp->rq_buffer;
	size_t size;
	struct rpc_buffer *buf;

	buf = container_of(buffer, struct rpc_buffer, data);
	size = buf->len;

	if (size <= RPC_BUFFER_MAXSIZE)
		mempool_free(buf, rpc_buffer_mempool);
	else
		kfree(buf);
}
EXPORT_SYMBOL_GPL(rpc_free);

 
static void rpc_init_task(struct rpc_task *task, const struct rpc_task_setup *task_setup_data)
{
	memset(task, 0, sizeof(*task));
	atomic_set(&task->tk_count, 1);
	task->tk_flags  = task_setup_data->flags;
	task->tk_ops = task_setup_data->callback_ops;
	task->tk_calldata = task_setup_data->callback_data;
	INIT_LIST_HEAD(&task->tk_task);

	task->tk_priority = task_setup_data->priority - RPC_PRIORITY_LOW;
	task->tk_owner = current->tgid;

	 
	task->tk_workqueue = task_setup_data->workqueue;

	task->tk_xprt = rpc_task_get_xprt(task_setup_data->rpc_client,
			xprt_get(task_setup_data->rpc_xprt));

	task->tk_op_cred = get_rpccred(task_setup_data->rpc_op_cred);

	if (task->tk_ops->rpc_call_prepare != NULL)
		task->tk_action = rpc_prepare_task;

	rpc_init_task_statistics(task);
}

static struct rpc_task *rpc_alloc_task(void)
{
	struct rpc_task *task;

	task = kmem_cache_alloc(rpc_task_slabp, rpc_task_gfp_mask());
	if (task)
		return task;
	return mempool_alloc(rpc_task_mempool, GFP_NOWAIT);
}

 
struct rpc_task *rpc_new_task(const struct rpc_task_setup *setup_data)
{
	struct rpc_task	*task = setup_data->task;
	unsigned short flags = 0;

	if (task == NULL) {
		task = rpc_alloc_task();
		if (task == NULL) {
			rpc_release_calldata(setup_data->callback_ops,
					     setup_data->callback_data);
			return ERR_PTR(-ENOMEM);
		}
		flags = RPC_TASK_DYNAMIC;
	}

	rpc_init_task(task, setup_data);
	task->tk_flags |= flags;
	return task;
}

 
static void rpc_free_task(struct rpc_task *task)
{
	unsigned short tk_flags = task->tk_flags;

	put_rpccred(task->tk_op_cred);
	rpc_release_calldata(task->tk_ops, task->tk_calldata);

	if (tk_flags & RPC_TASK_DYNAMIC)
		mempool_free(task, rpc_task_mempool);
}

static void rpc_async_release(struct work_struct *work)
{
	unsigned int pflags = memalloc_nofs_save();

	rpc_free_task(container_of(work, struct rpc_task, u.tk_work));
	memalloc_nofs_restore(pflags);
}

static void rpc_release_resources_task(struct rpc_task *task)
{
	xprt_release(task);
	if (task->tk_msg.rpc_cred) {
		if (!(task->tk_flags & RPC_TASK_CRED_NOREF))
			put_cred(task->tk_msg.rpc_cred);
		task->tk_msg.rpc_cred = NULL;
	}
	rpc_task_release_client(task);
}

static void rpc_final_put_task(struct rpc_task *task,
		struct workqueue_struct *q)
{
	if (q != NULL) {
		INIT_WORK(&task->u.tk_work, rpc_async_release);
		queue_work(q, &task->u.tk_work);
	} else
		rpc_free_task(task);
}

static void rpc_do_put_task(struct rpc_task *task, struct workqueue_struct *q)
{
	if (atomic_dec_and_test(&task->tk_count)) {
		rpc_release_resources_task(task);
		rpc_final_put_task(task, q);
	}
}

void rpc_put_task(struct rpc_task *task)
{
	rpc_do_put_task(task, NULL);
}
EXPORT_SYMBOL_GPL(rpc_put_task);

void rpc_put_task_async(struct rpc_task *task)
{
	rpc_do_put_task(task, task->tk_workqueue);
}
EXPORT_SYMBOL_GPL(rpc_put_task_async);

static void rpc_release_task(struct rpc_task *task)
{
	WARN_ON_ONCE(RPC_IS_QUEUED(task));

	rpc_release_resources_task(task);

	 
	if (atomic_read(&task->tk_count) != 1 + !RPC_IS_ASYNC(task)) {
		 
		if (!rpc_complete_task(task))
			return;
	} else {
		if (!atomic_dec_and_test(&task->tk_count))
			return;
	}
	rpc_final_put_task(task, task->tk_workqueue);
}

int rpciod_up(void)
{
	return try_module_get(THIS_MODULE) ? 0 : -EINVAL;
}

void rpciod_down(void)
{
	module_put(THIS_MODULE);
}

 
static int rpciod_start(void)
{
	struct workqueue_struct *wq;

	 
	wq = alloc_workqueue("rpciod", WQ_MEM_RECLAIM | WQ_UNBOUND, 0);
	if (!wq)
		goto out_failed;
	rpciod_workqueue = wq;
	wq = alloc_workqueue("xprtiod", WQ_UNBOUND | WQ_MEM_RECLAIM, 0);
	if (!wq)
		goto free_rpciod;
	xprtiod_workqueue = wq;
	return 1;
free_rpciod:
	wq = rpciod_workqueue;
	rpciod_workqueue = NULL;
	destroy_workqueue(wq);
out_failed:
	return 0;
}

static void rpciod_stop(void)
{
	struct workqueue_struct *wq = NULL;

	if (rpciod_workqueue == NULL)
		return;

	wq = rpciod_workqueue;
	rpciod_workqueue = NULL;
	destroy_workqueue(wq);
	wq = xprtiod_workqueue;
	xprtiod_workqueue = NULL;
	destroy_workqueue(wq);
}

void
rpc_destroy_mempool(void)
{
	rpciod_stop();
	mempool_destroy(rpc_buffer_mempool);
	mempool_destroy(rpc_task_mempool);
	kmem_cache_destroy(rpc_task_slabp);
	kmem_cache_destroy(rpc_buffer_slabp);
	rpc_destroy_wait_queue(&delay_queue);
}

int
rpc_init_mempool(void)
{
	 
	rpc_init_wait_queue(&delay_queue, "delayq");
	if (!rpciod_start())
		goto err_nomem;

	rpc_task_slabp = kmem_cache_create("rpc_tasks",
					     sizeof(struct rpc_task),
					     0, SLAB_HWCACHE_ALIGN,
					     NULL);
	if (!rpc_task_slabp)
		goto err_nomem;
	rpc_buffer_slabp = kmem_cache_create("rpc_buffers",
					     RPC_BUFFER_MAXSIZE,
					     0, SLAB_HWCACHE_ALIGN,
					     NULL);
	if (!rpc_buffer_slabp)
		goto err_nomem;
	rpc_task_mempool = mempool_create_slab_pool(RPC_TASK_POOLSIZE,
						    rpc_task_slabp);
	if (!rpc_task_mempool)
		goto err_nomem;
	rpc_buffer_mempool = mempool_create_slab_pool(RPC_BUFFER_POOLSIZE,
						      rpc_buffer_slabp);
	if (!rpc_buffer_mempool)
		goto err_nomem;
	return 0;
err_nomem:
	rpc_destroy_mempool();
	return -ENOMEM;
}
