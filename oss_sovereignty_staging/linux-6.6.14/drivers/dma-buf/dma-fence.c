
 

#include <linux/slab.h>
#include <linux/export.h>
#include <linux/atomic.h>
#include <linux/dma-fence.h>
#include <linux/sched/signal.h>
#include <linux/seq_file.h>

#define CREATE_TRACE_POINTS
#include <trace/events/dma_fence.h>

EXPORT_TRACEPOINT_SYMBOL(dma_fence_emit);
EXPORT_TRACEPOINT_SYMBOL(dma_fence_enable_signal);
EXPORT_TRACEPOINT_SYMBOL(dma_fence_signaled);

static DEFINE_SPINLOCK(dma_fence_stub_lock);
static struct dma_fence dma_fence_stub;

 
static atomic64_t dma_fence_context_counter = ATOMIC64_INIT(1);

 

 

static const char *dma_fence_stub_get_name(struct dma_fence *fence)
{
        return "stub";
}

static const struct dma_fence_ops dma_fence_stub_ops = {
	.get_driver_name = dma_fence_stub_get_name,
	.get_timeline_name = dma_fence_stub_get_name,
};

 
struct dma_fence *dma_fence_get_stub(void)
{
	spin_lock(&dma_fence_stub_lock);
	if (!dma_fence_stub.ops) {
		dma_fence_init(&dma_fence_stub,
			       &dma_fence_stub_ops,
			       &dma_fence_stub_lock,
			       0, 0);

		set_bit(DMA_FENCE_FLAG_ENABLE_SIGNAL_BIT,
			&dma_fence_stub.flags);

		dma_fence_signal_locked(&dma_fence_stub);
	}
	spin_unlock(&dma_fence_stub_lock);

	return dma_fence_get(&dma_fence_stub);
}
EXPORT_SYMBOL(dma_fence_get_stub);

 
struct dma_fence *dma_fence_allocate_private_stub(ktime_t timestamp)
{
	struct dma_fence *fence;

	fence = kzalloc(sizeof(*fence), GFP_KERNEL);
	if (fence == NULL)
		return NULL;

	dma_fence_init(fence,
		       &dma_fence_stub_ops,
		       &dma_fence_stub_lock,
		       0, 0);

	set_bit(DMA_FENCE_FLAG_ENABLE_SIGNAL_BIT,
		&fence->flags);

	dma_fence_signal_timestamp(fence, timestamp);

	return fence;
}
EXPORT_SYMBOL(dma_fence_allocate_private_stub);

 
u64 dma_fence_context_alloc(unsigned num)
{
	WARN_ON(!num);
	return atomic64_fetch_add(num, &dma_fence_context_counter);
}
EXPORT_SYMBOL(dma_fence_context_alloc);

 
#ifdef CONFIG_LOCKDEP
static struct lockdep_map dma_fence_lockdep_map = {
	.name = "dma_fence_map"
};

 
bool dma_fence_begin_signalling(void)
{
	 
	if (lock_is_held_type(&dma_fence_lockdep_map, 1))
		return true;

	 
	if (in_atomic())
		return true;

	 
	lock_acquire(&dma_fence_lockdep_map, 0, 0, 1, 1, NULL, _RET_IP_);

	return false;
}
EXPORT_SYMBOL(dma_fence_begin_signalling);

 
void dma_fence_end_signalling(bool cookie)
{
	if (cookie)
		return;

	lock_release(&dma_fence_lockdep_map, _RET_IP_);
}
EXPORT_SYMBOL(dma_fence_end_signalling);

void __dma_fence_might_wait(void)
{
	bool tmp;

	tmp = lock_is_held_type(&dma_fence_lockdep_map, 1);
	if (tmp)
		lock_release(&dma_fence_lockdep_map, _THIS_IP_);
	lock_map_acquire(&dma_fence_lockdep_map);
	lock_map_release(&dma_fence_lockdep_map);
	if (tmp)
		lock_acquire(&dma_fence_lockdep_map, 0, 0, 1, 1, NULL, _THIS_IP_);
}
#endif


 
int dma_fence_signal_timestamp_locked(struct dma_fence *fence,
				      ktime_t timestamp)
{
	struct dma_fence_cb *cur, *tmp;
	struct list_head cb_list;

	lockdep_assert_held(fence->lock);

	if (unlikely(test_and_set_bit(DMA_FENCE_FLAG_SIGNALED_BIT,
				      &fence->flags)))
		return -EINVAL;

	 
	list_replace(&fence->cb_list, &cb_list);

	fence->timestamp = timestamp;
	set_bit(DMA_FENCE_FLAG_TIMESTAMP_BIT, &fence->flags);
	trace_dma_fence_signaled(fence);

	list_for_each_entry_safe(cur, tmp, &cb_list, node) {
		INIT_LIST_HEAD(&cur->node);
		cur->func(fence, cur);
	}

	return 0;
}
EXPORT_SYMBOL(dma_fence_signal_timestamp_locked);

 
int dma_fence_signal_timestamp(struct dma_fence *fence, ktime_t timestamp)
{
	unsigned long flags;
	int ret;

	if (!fence)
		return -EINVAL;

	spin_lock_irqsave(fence->lock, flags);
	ret = dma_fence_signal_timestamp_locked(fence, timestamp);
	spin_unlock_irqrestore(fence->lock, flags);

	return ret;
}
EXPORT_SYMBOL(dma_fence_signal_timestamp);

 
int dma_fence_signal_locked(struct dma_fence *fence)
{
	return dma_fence_signal_timestamp_locked(fence, ktime_get());
}
EXPORT_SYMBOL(dma_fence_signal_locked);

 
int dma_fence_signal(struct dma_fence *fence)
{
	unsigned long flags;
	int ret;
	bool tmp;

	if (!fence)
		return -EINVAL;

	tmp = dma_fence_begin_signalling();

	spin_lock_irqsave(fence->lock, flags);
	ret = dma_fence_signal_timestamp_locked(fence, ktime_get());
	spin_unlock_irqrestore(fence->lock, flags);

	dma_fence_end_signalling(tmp);

	return ret;
}
EXPORT_SYMBOL(dma_fence_signal);

 
signed long
dma_fence_wait_timeout(struct dma_fence *fence, bool intr, signed long timeout)
{
	signed long ret;

	if (WARN_ON(timeout < 0))
		return -EINVAL;

	might_sleep();

	__dma_fence_might_wait();

	dma_fence_enable_sw_signaling(fence);

	trace_dma_fence_wait_start(fence);
	if (fence->ops->wait)
		ret = fence->ops->wait(fence, intr, timeout);
	else
		ret = dma_fence_default_wait(fence, intr, timeout);
	trace_dma_fence_wait_end(fence);
	return ret;
}
EXPORT_SYMBOL(dma_fence_wait_timeout);

 
void dma_fence_release(struct kref *kref)
{
	struct dma_fence *fence =
		container_of(kref, struct dma_fence, refcount);

	trace_dma_fence_destroy(fence);

	if (WARN(!list_empty(&fence->cb_list) &&
		 !test_bit(DMA_FENCE_FLAG_SIGNALED_BIT, &fence->flags),
		 "Fence %s:%s:%llx:%llx released with pending signals!\n",
		 fence->ops->get_driver_name(fence),
		 fence->ops->get_timeline_name(fence),
		 fence->context, fence->seqno)) {
		unsigned long flags;

		 
		spin_lock_irqsave(fence->lock, flags);
		fence->error = -EDEADLK;
		dma_fence_signal_locked(fence);
		spin_unlock_irqrestore(fence->lock, flags);
	}

	if (fence->ops->release)
		fence->ops->release(fence);
	else
		dma_fence_free(fence);
}
EXPORT_SYMBOL(dma_fence_release);

 
void dma_fence_free(struct dma_fence *fence)
{
	kfree_rcu(fence, rcu);
}
EXPORT_SYMBOL(dma_fence_free);

static bool __dma_fence_enable_signaling(struct dma_fence *fence)
{
	bool was_set;

	lockdep_assert_held(fence->lock);

	was_set = test_and_set_bit(DMA_FENCE_FLAG_ENABLE_SIGNAL_BIT,
				   &fence->flags);

	if (test_bit(DMA_FENCE_FLAG_SIGNALED_BIT, &fence->flags))
		return false;

	if (!was_set && fence->ops->enable_signaling) {
		trace_dma_fence_enable_signal(fence);

		if (!fence->ops->enable_signaling(fence)) {
			dma_fence_signal_locked(fence);
			return false;
		}
	}

	return true;
}

 
void dma_fence_enable_sw_signaling(struct dma_fence *fence)
{
	unsigned long flags;

	spin_lock_irqsave(fence->lock, flags);
	__dma_fence_enable_signaling(fence);
	spin_unlock_irqrestore(fence->lock, flags);
}
EXPORT_SYMBOL(dma_fence_enable_sw_signaling);

 
int dma_fence_add_callback(struct dma_fence *fence, struct dma_fence_cb *cb,
			   dma_fence_func_t func)
{
	unsigned long flags;
	int ret = 0;

	if (WARN_ON(!fence || !func))
		return -EINVAL;

	if (test_bit(DMA_FENCE_FLAG_SIGNALED_BIT, &fence->flags)) {
		INIT_LIST_HEAD(&cb->node);
		return -ENOENT;
	}

	spin_lock_irqsave(fence->lock, flags);

	if (__dma_fence_enable_signaling(fence)) {
		cb->func = func;
		list_add_tail(&cb->node, &fence->cb_list);
	} else {
		INIT_LIST_HEAD(&cb->node);
		ret = -ENOENT;
	}

	spin_unlock_irqrestore(fence->lock, flags);

	return ret;
}
EXPORT_SYMBOL(dma_fence_add_callback);

 
int dma_fence_get_status(struct dma_fence *fence)
{
	unsigned long flags;
	int status;

	spin_lock_irqsave(fence->lock, flags);
	status = dma_fence_get_status_locked(fence);
	spin_unlock_irqrestore(fence->lock, flags);

	return status;
}
EXPORT_SYMBOL(dma_fence_get_status);

 
bool
dma_fence_remove_callback(struct dma_fence *fence, struct dma_fence_cb *cb)
{
	unsigned long flags;
	bool ret;

	spin_lock_irqsave(fence->lock, flags);

	ret = !list_empty(&cb->node);
	if (ret)
		list_del_init(&cb->node);

	spin_unlock_irqrestore(fence->lock, flags);

	return ret;
}
EXPORT_SYMBOL(dma_fence_remove_callback);

struct default_wait_cb {
	struct dma_fence_cb base;
	struct task_struct *task;
};

static void
dma_fence_default_wait_cb(struct dma_fence *fence, struct dma_fence_cb *cb)
{
	struct default_wait_cb *wait =
		container_of(cb, struct default_wait_cb, base);

	wake_up_state(wait->task, TASK_NORMAL);
}

 
signed long
dma_fence_default_wait(struct dma_fence *fence, bool intr, signed long timeout)
{
	struct default_wait_cb cb;
	unsigned long flags;
	signed long ret = timeout ? timeout : 1;

	spin_lock_irqsave(fence->lock, flags);

	if (test_bit(DMA_FENCE_FLAG_SIGNALED_BIT, &fence->flags))
		goto out;

	if (intr && signal_pending(current)) {
		ret = -ERESTARTSYS;
		goto out;
	}

	if (!timeout) {
		ret = 0;
		goto out;
	}

	cb.base.func = dma_fence_default_wait_cb;
	cb.task = current;
	list_add(&cb.base.node, &fence->cb_list);

	while (!test_bit(DMA_FENCE_FLAG_SIGNALED_BIT, &fence->flags) && ret > 0) {
		if (intr)
			__set_current_state(TASK_INTERRUPTIBLE);
		else
			__set_current_state(TASK_UNINTERRUPTIBLE);
		spin_unlock_irqrestore(fence->lock, flags);

		ret = schedule_timeout(ret);

		spin_lock_irqsave(fence->lock, flags);
		if (ret > 0 && intr && signal_pending(current))
			ret = -ERESTARTSYS;
	}

	if (!list_empty(&cb.base.node))
		list_del(&cb.base.node);
	__set_current_state(TASK_RUNNING);

out:
	spin_unlock_irqrestore(fence->lock, flags);
	return ret;
}
EXPORT_SYMBOL(dma_fence_default_wait);

static bool
dma_fence_test_signaled_any(struct dma_fence **fences, uint32_t count,
			    uint32_t *idx)
{
	int i;

	for (i = 0; i < count; ++i) {
		struct dma_fence *fence = fences[i];
		if (test_bit(DMA_FENCE_FLAG_SIGNALED_BIT, &fence->flags)) {
			if (idx)
				*idx = i;
			return true;
		}
	}
	return false;
}

 
signed long
dma_fence_wait_any_timeout(struct dma_fence **fences, uint32_t count,
			   bool intr, signed long timeout, uint32_t *idx)
{
	struct default_wait_cb *cb;
	signed long ret = timeout;
	unsigned i;

	if (WARN_ON(!fences || !count || timeout < 0))
		return -EINVAL;

	if (timeout == 0) {
		for (i = 0; i < count; ++i)
			if (dma_fence_is_signaled(fences[i])) {
				if (idx)
					*idx = i;
				return 1;
			}

		return 0;
	}

	cb = kcalloc(count, sizeof(struct default_wait_cb), GFP_KERNEL);
	if (cb == NULL) {
		ret = -ENOMEM;
		goto err_free_cb;
	}

	for (i = 0; i < count; ++i) {
		struct dma_fence *fence = fences[i];

		cb[i].task = current;
		if (dma_fence_add_callback(fence, &cb[i].base,
					   dma_fence_default_wait_cb)) {
			 
			if (idx)
				*idx = i;
			goto fence_rm_cb;
		}
	}

	while (ret > 0) {
		if (intr)
			set_current_state(TASK_INTERRUPTIBLE);
		else
			set_current_state(TASK_UNINTERRUPTIBLE);

		if (dma_fence_test_signaled_any(fences, count, idx))
			break;

		ret = schedule_timeout(ret);

		if (ret > 0 && intr && signal_pending(current))
			ret = -ERESTARTSYS;
	}

	__set_current_state(TASK_RUNNING);

fence_rm_cb:
	while (i-- > 0)
		dma_fence_remove_callback(fences[i], &cb[i].base);

err_free_cb:
	kfree(cb);

	return ret;
}
EXPORT_SYMBOL(dma_fence_wait_any_timeout);

 

 
void dma_fence_set_deadline(struct dma_fence *fence, ktime_t deadline)
{
	if (fence->ops->set_deadline && !dma_fence_is_signaled(fence))
		fence->ops->set_deadline(fence, deadline);
}
EXPORT_SYMBOL(dma_fence_set_deadline);

 
void dma_fence_describe(struct dma_fence *fence, struct seq_file *seq)
{
	seq_printf(seq, "%s %s seq %llu %ssignalled\n",
		   fence->ops->get_driver_name(fence),
		   fence->ops->get_timeline_name(fence), fence->seqno,
		   dma_fence_is_signaled(fence) ? "" : "un");
}
EXPORT_SYMBOL(dma_fence_describe);

 
void
dma_fence_init(struct dma_fence *fence, const struct dma_fence_ops *ops,
	       spinlock_t *lock, u64 context, u64 seqno)
{
	BUG_ON(!lock);
	BUG_ON(!ops || !ops->get_driver_name || !ops->get_timeline_name);

	kref_init(&fence->refcount);
	fence->ops = ops;
	INIT_LIST_HEAD(&fence->cb_list);
	fence->lock = lock;
	fence->context = context;
	fence->seqno = seqno;
	fence->flags = 0UL;
	fence->error = 0;

	trace_dma_fence_init(fence);
}
EXPORT_SYMBOL(dma_fence_init);
