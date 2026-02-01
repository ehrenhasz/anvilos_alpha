
 

#include <linux/dma-fence-chain.h>

static bool dma_fence_chain_enable_signaling(struct dma_fence *fence);

 
static struct dma_fence *dma_fence_chain_get_prev(struct dma_fence_chain *chain)
{
	struct dma_fence *prev;

	rcu_read_lock();
	prev = dma_fence_get_rcu_safe(&chain->prev);
	rcu_read_unlock();
	return prev;
}

 
struct dma_fence *dma_fence_chain_walk(struct dma_fence *fence)
{
	struct dma_fence_chain *chain, *prev_chain;
	struct dma_fence *prev, *replacement, *tmp;

	chain = to_dma_fence_chain(fence);
	if (!chain) {
		dma_fence_put(fence);
		return NULL;
	}

	while ((prev = dma_fence_chain_get_prev(chain))) {

		prev_chain = to_dma_fence_chain(prev);
		if (prev_chain) {
			if (!dma_fence_is_signaled(prev_chain->fence))
				break;

			replacement = dma_fence_chain_get_prev(prev_chain);
		} else {
			if (!dma_fence_is_signaled(prev))
				break;

			replacement = NULL;
		}

		tmp = unrcu_pointer(cmpxchg(&chain->prev, RCU_INITIALIZER(prev),
					     RCU_INITIALIZER(replacement)));
		if (tmp == prev)
			dma_fence_put(tmp);
		else
			dma_fence_put(replacement);
		dma_fence_put(prev);
	}

	dma_fence_put(fence);
	return prev;
}
EXPORT_SYMBOL(dma_fence_chain_walk);

 
int dma_fence_chain_find_seqno(struct dma_fence **pfence, uint64_t seqno)
{
	struct dma_fence_chain *chain;

	if (!seqno)
		return 0;

	chain = to_dma_fence_chain(*pfence);
	if (!chain || chain->base.seqno < seqno)
		return -EINVAL;

	dma_fence_chain_for_each(*pfence, &chain->base) {
		if ((*pfence)->context != chain->base.context ||
		    to_dma_fence_chain(*pfence)->prev_seqno < seqno)
			break;
	}
	dma_fence_put(&chain->base);

	return 0;
}
EXPORT_SYMBOL(dma_fence_chain_find_seqno);

static const char *dma_fence_chain_get_driver_name(struct dma_fence *fence)
{
        return "dma_fence_chain";
}

static const char *dma_fence_chain_get_timeline_name(struct dma_fence *fence)
{
        return "unbound";
}

static void dma_fence_chain_irq_work(struct irq_work *work)
{
	struct dma_fence_chain *chain;

	chain = container_of(work, typeof(*chain), work);

	 
	if (!dma_fence_chain_enable_signaling(&chain->base))
		 
		dma_fence_signal(&chain->base);
	dma_fence_put(&chain->base);
}

static void dma_fence_chain_cb(struct dma_fence *f, struct dma_fence_cb *cb)
{
	struct dma_fence_chain *chain;

	chain = container_of(cb, typeof(*chain), cb);
	init_irq_work(&chain->work, dma_fence_chain_irq_work);
	irq_work_queue(&chain->work);
	dma_fence_put(f);
}

static bool dma_fence_chain_enable_signaling(struct dma_fence *fence)
{
	struct dma_fence_chain *head = to_dma_fence_chain(fence);

	dma_fence_get(&head->base);
	dma_fence_chain_for_each(fence, &head->base) {
		struct dma_fence *f = dma_fence_chain_contained(fence);

		dma_fence_get(f);
		if (!dma_fence_add_callback(f, &head->cb, dma_fence_chain_cb)) {
			dma_fence_put(fence);
			return true;
		}
		dma_fence_put(f);
	}
	dma_fence_put(&head->base);
	return false;
}

static bool dma_fence_chain_signaled(struct dma_fence *fence)
{
	dma_fence_chain_for_each(fence, fence) {
		struct dma_fence *f = dma_fence_chain_contained(fence);

		if (!dma_fence_is_signaled(f)) {
			dma_fence_put(fence);
			return false;
		}
	}

	return true;
}

static void dma_fence_chain_release(struct dma_fence *fence)
{
	struct dma_fence_chain *chain = to_dma_fence_chain(fence);
	struct dma_fence *prev;

	 
	while ((prev = rcu_dereference_protected(chain->prev, true))) {
		struct dma_fence_chain *prev_chain;

		if (kref_read(&prev->refcount) > 1)
		       break;

		prev_chain = to_dma_fence_chain(prev);
		if (!prev_chain)
			break;

		 
		chain->prev = prev_chain->prev;
		RCU_INIT_POINTER(prev_chain->prev, NULL);
		dma_fence_put(prev);
	}
	dma_fence_put(prev);

	dma_fence_put(chain->fence);
	dma_fence_free(fence);
}


static void dma_fence_chain_set_deadline(struct dma_fence *fence,
					 ktime_t deadline)
{
	dma_fence_chain_for_each(fence, fence) {
		struct dma_fence *f = dma_fence_chain_contained(fence);

		dma_fence_set_deadline(f, deadline);
	}
}

const struct dma_fence_ops dma_fence_chain_ops = {
	.use_64bit_seqno = true,
	.get_driver_name = dma_fence_chain_get_driver_name,
	.get_timeline_name = dma_fence_chain_get_timeline_name,
	.enable_signaling = dma_fence_chain_enable_signaling,
	.signaled = dma_fence_chain_signaled,
	.release = dma_fence_chain_release,
	.set_deadline = dma_fence_chain_set_deadline,
};
EXPORT_SYMBOL(dma_fence_chain_ops);

 
void dma_fence_chain_init(struct dma_fence_chain *chain,
			  struct dma_fence *prev,
			  struct dma_fence *fence,
			  uint64_t seqno)
{
	struct dma_fence_chain *prev_chain = to_dma_fence_chain(prev);
	uint64_t context;

	spin_lock_init(&chain->lock);
	rcu_assign_pointer(chain->prev, prev);
	chain->fence = fence;
	chain->prev_seqno = 0;

	 
	if (prev_chain && __dma_fence_is_later(seqno, prev->seqno, prev->ops)) {
		context = prev->context;
		chain->prev_seqno = prev->seqno;
	} else {
		context = dma_fence_context_alloc(1);
		 
		if (prev_chain)
			seqno = max(prev->seqno, seqno);
	}

	dma_fence_init(&chain->base, &dma_fence_chain_ops,
		       &chain->lock, context, seqno);

	 
	WARN_ON(dma_fence_is_chain(fence));
}
EXPORT_SYMBOL(dma_fence_chain_init);
