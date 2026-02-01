
 

#include <linux/rculist.h>
#include <linux/mmu_notifier.h>
#include <linux/export.h>
#include <linux/mm.h>
#include <linux/err.h>
#include <linux/interval_tree.h>
#include <linux/srcu.h>
#include <linux/rcupdate.h>
#include <linux/sched.h>
#include <linux/sched/mm.h>
#include <linux/slab.h>

 
DEFINE_STATIC_SRCU(srcu);

#ifdef CONFIG_LOCKDEP
struct lockdep_map __mmu_notifier_invalidate_range_start_map = {
	.name = "mmu_notifier_invalidate_range_start"
};
#endif

 
struct mmu_notifier_subscriptions {
	 
	struct hlist_head list;
	bool has_itree;
	 
	spinlock_t lock;
	unsigned long invalidate_seq;
	unsigned long active_invalidate_ranges;
	struct rb_root_cached itree;
	wait_queue_head_t wq;
	struct hlist_head deferred_list;
};

 
static bool
mn_itree_is_invalidating(struct mmu_notifier_subscriptions *subscriptions)
{
	lockdep_assert_held(&subscriptions->lock);
	return subscriptions->invalidate_seq & 1;
}

static struct mmu_interval_notifier *
mn_itree_inv_start_range(struct mmu_notifier_subscriptions *subscriptions,
			 const struct mmu_notifier_range *range,
			 unsigned long *seq)
{
	struct interval_tree_node *node;
	struct mmu_interval_notifier *res = NULL;

	spin_lock(&subscriptions->lock);
	subscriptions->active_invalidate_ranges++;
	node = interval_tree_iter_first(&subscriptions->itree, range->start,
					range->end - 1);
	if (node) {
		subscriptions->invalidate_seq |= 1;
		res = container_of(node, struct mmu_interval_notifier,
				   interval_tree);
	}

	*seq = subscriptions->invalidate_seq;
	spin_unlock(&subscriptions->lock);
	return res;
}

static struct mmu_interval_notifier *
mn_itree_inv_next(struct mmu_interval_notifier *interval_sub,
		  const struct mmu_notifier_range *range)
{
	struct interval_tree_node *node;

	node = interval_tree_iter_next(&interval_sub->interval_tree,
				       range->start, range->end - 1);
	if (!node)
		return NULL;
	return container_of(node, struct mmu_interval_notifier, interval_tree);
}

static void mn_itree_inv_end(struct mmu_notifier_subscriptions *subscriptions)
{
	struct mmu_interval_notifier *interval_sub;
	struct hlist_node *next;

	spin_lock(&subscriptions->lock);
	if (--subscriptions->active_invalidate_ranges ||
	    !mn_itree_is_invalidating(subscriptions)) {
		spin_unlock(&subscriptions->lock);
		return;
	}

	 
	subscriptions->invalidate_seq++;

	 
	hlist_for_each_entry_safe(interval_sub, next,
				  &subscriptions->deferred_list,
				  deferred_item) {
		if (RB_EMPTY_NODE(&interval_sub->interval_tree.rb))
			interval_tree_insert(&interval_sub->interval_tree,
					     &subscriptions->itree);
		else
			interval_tree_remove(&interval_sub->interval_tree,
					     &subscriptions->itree);
		hlist_del(&interval_sub->deferred_item);
	}
	spin_unlock(&subscriptions->lock);

	wake_up_all(&subscriptions->wq);
}

 
unsigned long
mmu_interval_read_begin(struct mmu_interval_notifier *interval_sub)
{
	struct mmu_notifier_subscriptions *subscriptions =
		interval_sub->mm->notifier_subscriptions;
	unsigned long seq;
	bool is_invalidating;

	 
	spin_lock(&subscriptions->lock);
	 
	seq = READ_ONCE(interval_sub->invalidate_seq);
	is_invalidating = seq == subscriptions->invalidate_seq;
	spin_unlock(&subscriptions->lock);

	 
	lock_map_acquire(&__mmu_notifier_invalidate_range_start_map);
	lock_map_release(&__mmu_notifier_invalidate_range_start_map);
	if (is_invalidating)
		wait_event(subscriptions->wq,
			   READ_ONCE(subscriptions->invalidate_seq) != seq);

	 

	return seq;
}
EXPORT_SYMBOL_GPL(mmu_interval_read_begin);

static void mn_itree_release(struct mmu_notifier_subscriptions *subscriptions,
			     struct mm_struct *mm)
{
	struct mmu_notifier_range range = {
		.flags = MMU_NOTIFIER_RANGE_BLOCKABLE,
		.event = MMU_NOTIFY_RELEASE,
		.mm = mm,
		.start = 0,
		.end = ULONG_MAX,
	};
	struct mmu_interval_notifier *interval_sub;
	unsigned long cur_seq;
	bool ret;

	for (interval_sub =
		     mn_itree_inv_start_range(subscriptions, &range, &cur_seq);
	     interval_sub;
	     interval_sub = mn_itree_inv_next(interval_sub, &range)) {
		ret = interval_sub->ops->invalidate(interval_sub, &range,
						    cur_seq);
		WARN_ON(!ret);
	}

	mn_itree_inv_end(subscriptions);
}

 
static void mn_hlist_release(struct mmu_notifier_subscriptions *subscriptions,
			     struct mm_struct *mm)
{
	struct mmu_notifier *subscription;
	int id;

	 
	id = srcu_read_lock(&srcu);
	hlist_for_each_entry_rcu(subscription, &subscriptions->list, hlist,
				 srcu_read_lock_held(&srcu))
		 
		if (subscription->ops->release)
			subscription->ops->release(subscription, mm);

	spin_lock(&subscriptions->lock);
	while (unlikely(!hlist_empty(&subscriptions->list))) {
		subscription = hlist_entry(subscriptions->list.first,
					   struct mmu_notifier, hlist);
		 
		hlist_del_init_rcu(&subscription->hlist);
	}
	spin_unlock(&subscriptions->lock);
	srcu_read_unlock(&srcu, id);

	 
	synchronize_srcu(&srcu);
}

void __mmu_notifier_release(struct mm_struct *mm)
{
	struct mmu_notifier_subscriptions *subscriptions =
		mm->notifier_subscriptions;

	if (subscriptions->has_itree)
		mn_itree_release(subscriptions, mm);

	if (!hlist_empty(&subscriptions->list))
		mn_hlist_release(subscriptions, mm);
}

 
int __mmu_notifier_clear_flush_young(struct mm_struct *mm,
					unsigned long start,
					unsigned long end)
{
	struct mmu_notifier *subscription;
	int young = 0, id;

	id = srcu_read_lock(&srcu);
	hlist_for_each_entry_rcu(subscription,
				 &mm->notifier_subscriptions->list, hlist,
				 srcu_read_lock_held(&srcu)) {
		if (subscription->ops->clear_flush_young)
			young |= subscription->ops->clear_flush_young(
				subscription, mm, start, end);
	}
	srcu_read_unlock(&srcu, id);

	return young;
}

int __mmu_notifier_clear_young(struct mm_struct *mm,
			       unsigned long start,
			       unsigned long end)
{
	struct mmu_notifier *subscription;
	int young = 0, id;

	id = srcu_read_lock(&srcu);
	hlist_for_each_entry_rcu(subscription,
				 &mm->notifier_subscriptions->list, hlist,
				 srcu_read_lock_held(&srcu)) {
		if (subscription->ops->clear_young)
			young |= subscription->ops->clear_young(subscription,
								mm, start, end);
	}
	srcu_read_unlock(&srcu, id);

	return young;
}

int __mmu_notifier_test_young(struct mm_struct *mm,
			      unsigned long address)
{
	struct mmu_notifier *subscription;
	int young = 0, id;

	id = srcu_read_lock(&srcu);
	hlist_for_each_entry_rcu(subscription,
				 &mm->notifier_subscriptions->list, hlist,
				 srcu_read_lock_held(&srcu)) {
		if (subscription->ops->test_young) {
			young = subscription->ops->test_young(subscription, mm,
							      address);
			if (young)
				break;
		}
	}
	srcu_read_unlock(&srcu, id);

	return young;
}

void __mmu_notifier_change_pte(struct mm_struct *mm, unsigned long address,
			       pte_t pte)
{
	struct mmu_notifier *subscription;
	int id;

	id = srcu_read_lock(&srcu);
	hlist_for_each_entry_rcu(subscription,
				 &mm->notifier_subscriptions->list, hlist,
				 srcu_read_lock_held(&srcu)) {
		if (subscription->ops->change_pte)
			subscription->ops->change_pte(subscription, mm, address,
						      pte);
	}
	srcu_read_unlock(&srcu, id);
}

static int mn_itree_invalidate(struct mmu_notifier_subscriptions *subscriptions,
			       const struct mmu_notifier_range *range)
{
	struct mmu_interval_notifier *interval_sub;
	unsigned long cur_seq;

	for (interval_sub =
		     mn_itree_inv_start_range(subscriptions, range, &cur_seq);
	     interval_sub;
	     interval_sub = mn_itree_inv_next(interval_sub, range)) {
		bool ret;

		ret = interval_sub->ops->invalidate(interval_sub, range,
						    cur_seq);
		if (!ret) {
			if (WARN_ON(mmu_notifier_range_blockable(range)))
				continue;
			goto out_would_block;
		}
	}
	return 0;

out_would_block:
	 
	mn_itree_inv_end(subscriptions);
	return -EAGAIN;
}

static int mn_hlist_invalidate_range_start(
	struct mmu_notifier_subscriptions *subscriptions,
	struct mmu_notifier_range *range)
{
	struct mmu_notifier *subscription;
	int ret = 0;
	int id;

	id = srcu_read_lock(&srcu);
	hlist_for_each_entry_rcu(subscription, &subscriptions->list, hlist,
				 srcu_read_lock_held(&srcu)) {
		const struct mmu_notifier_ops *ops = subscription->ops;

		if (ops->invalidate_range_start) {
			int _ret;

			if (!mmu_notifier_range_blockable(range))
				non_block_start();
			_ret = ops->invalidate_range_start(subscription, range);
			if (!mmu_notifier_range_blockable(range))
				non_block_end();
			if (_ret) {
				pr_info("%pS callback failed with %d in %sblockable context.\n",
					ops->invalidate_range_start, _ret,
					!mmu_notifier_range_blockable(range) ?
						"non-" :
						"");
				WARN_ON(mmu_notifier_range_blockable(range) ||
					_ret != -EAGAIN);
				 
				WARN_ON(ops->invalidate_range_end);
				ret = _ret;
			}
		}
	}

	if (ret) {
		 
		hlist_for_each_entry_rcu(subscription, &subscriptions->list,
					 hlist, srcu_read_lock_held(&srcu)) {
			if (!subscription->ops->invalidate_range_end)
				continue;

			subscription->ops->invalidate_range_end(subscription,
								range);
		}
	}
	srcu_read_unlock(&srcu, id);

	return ret;
}

int __mmu_notifier_invalidate_range_start(struct mmu_notifier_range *range)
{
	struct mmu_notifier_subscriptions *subscriptions =
		range->mm->notifier_subscriptions;
	int ret;

	if (subscriptions->has_itree) {
		ret = mn_itree_invalidate(subscriptions, range);
		if (ret)
			return ret;
	}
	if (!hlist_empty(&subscriptions->list))
		return mn_hlist_invalidate_range_start(subscriptions, range);
	return 0;
}

static void
mn_hlist_invalidate_end(struct mmu_notifier_subscriptions *subscriptions,
			struct mmu_notifier_range *range)
{
	struct mmu_notifier *subscription;
	int id;

	id = srcu_read_lock(&srcu);
	hlist_for_each_entry_rcu(subscription, &subscriptions->list, hlist,
				 srcu_read_lock_held(&srcu)) {
		if (subscription->ops->invalidate_range_end) {
			if (!mmu_notifier_range_blockable(range))
				non_block_start();
			subscription->ops->invalidate_range_end(subscription,
								range);
			if (!mmu_notifier_range_blockable(range))
				non_block_end();
		}
	}
	srcu_read_unlock(&srcu, id);
}

void __mmu_notifier_invalidate_range_end(struct mmu_notifier_range *range)
{
	struct mmu_notifier_subscriptions *subscriptions =
		range->mm->notifier_subscriptions;

	lock_map_acquire(&__mmu_notifier_invalidate_range_start_map);
	if (subscriptions->has_itree)
		mn_itree_inv_end(subscriptions);

	if (!hlist_empty(&subscriptions->list))
		mn_hlist_invalidate_end(subscriptions, range);
	lock_map_release(&__mmu_notifier_invalidate_range_start_map);
}

void __mmu_notifier_arch_invalidate_secondary_tlbs(struct mm_struct *mm,
					unsigned long start, unsigned long end)
{
	struct mmu_notifier *subscription;
	int id;

	id = srcu_read_lock(&srcu);
	hlist_for_each_entry_rcu(subscription,
				 &mm->notifier_subscriptions->list, hlist,
				 srcu_read_lock_held(&srcu)) {
		if (subscription->ops->arch_invalidate_secondary_tlbs)
			subscription->ops->arch_invalidate_secondary_tlbs(
				subscription, mm,
				start, end);
	}
	srcu_read_unlock(&srcu, id);
}

 
int __mmu_notifier_register(struct mmu_notifier *subscription,
			    struct mm_struct *mm)
{
	struct mmu_notifier_subscriptions *subscriptions = NULL;
	int ret;

	mmap_assert_write_locked(mm);
	BUG_ON(atomic_read(&mm->mm_users) <= 0);

	 
	if (WARN_ON_ONCE(subscription &&
			 (subscription->ops->arch_invalidate_secondary_tlbs &&
			 (subscription->ops->invalidate_range_start ||
			  subscription->ops->invalidate_range_end))))
		return -EINVAL;

	if (!mm->notifier_subscriptions) {
		 
		subscriptions = kzalloc(
			sizeof(struct mmu_notifier_subscriptions), GFP_KERNEL);
		if (!subscriptions)
			return -ENOMEM;

		INIT_HLIST_HEAD(&subscriptions->list);
		spin_lock_init(&subscriptions->lock);
		subscriptions->invalidate_seq = 2;
		subscriptions->itree = RB_ROOT_CACHED;
		init_waitqueue_head(&subscriptions->wq);
		INIT_HLIST_HEAD(&subscriptions->deferred_list);
	}

	ret = mm_take_all_locks(mm);
	if (unlikely(ret))
		goto out_clean;

	 
	if (subscriptions)
		smp_store_release(&mm->notifier_subscriptions, subscriptions);

	if (subscription) {
		 
		mmgrab(mm);
		subscription->mm = mm;
		subscription->users = 1;

		spin_lock(&mm->notifier_subscriptions->lock);
		hlist_add_head_rcu(&subscription->hlist,
				   &mm->notifier_subscriptions->list);
		spin_unlock(&mm->notifier_subscriptions->lock);
	} else
		mm->notifier_subscriptions->has_itree = true;

	mm_drop_all_locks(mm);
	BUG_ON(atomic_read(&mm->mm_users) <= 0);
	return 0;

out_clean:
	kfree(subscriptions);
	return ret;
}
EXPORT_SYMBOL_GPL(__mmu_notifier_register);

 
int mmu_notifier_register(struct mmu_notifier *subscription,
			  struct mm_struct *mm)
{
	int ret;

	mmap_write_lock(mm);
	ret = __mmu_notifier_register(subscription, mm);
	mmap_write_unlock(mm);
	return ret;
}
EXPORT_SYMBOL_GPL(mmu_notifier_register);

static struct mmu_notifier *
find_get_mmu_notifier(struct mm_struct *mm, const struct mmu_notifier_ops *ops)
{
	struct mmu_notifier *subscription;

	spin_lock(&mm->notifier_subscriptions->lock);
	hlist_for_each_entry_rcu(subscription,
				 &mm->notifier_subscriptions->list, hlist,
				 lockdep_is_held(&mm->notifier_subscriptions->lock)) {
		if (subscription->ops != ops)
			continue;

		if (likely(subscription->users != UINT_MAX))
			subscription->users++;
		else
			subscription = ERR_PTR(-EOVERFLOW);
		spin_unlock(&mm->notifier_subscriptions->lock);
		return subscription;
	}
	spin_unlock(&mm->notifier_subscriptions->lock);
	return NULL;
}

 
struct mmu_notifier *mmu_notifier_get_locked(const struct mmu_notifier_ops *ops,
					     struct mm_struct *mm)
{
	struct mmu_notifier *subscription;
	int ret;

	mmap_assert_write_locked(mm);

	if (mm->notifier_subscriptions) {
		subscription = find_get_mmu_notifier(mm, ops);
		if (subscription)
			return subscription;
	}

	subscription = ops->alloc_notifier(mm);
	if (IS_ERR(subscription))
		return subscription;
	subscription->ops = ops;
	ret = __mmu_notifier_register(subscription, mm);
	if (ret)
		goto out_free;
	return subscription;
out_free:
	subscription->ops->free_notifier(subscription);
	return ERR_PTR(ret);
}
EXPORT_SYMBOL_GPL(mmu_notifier_get_locked);

 
void __mmu_notifier_subscriptions_destroy(struct mm_struct *mm)
{
	BUG_ON(!hlist_empty(&mm->notifier_subscriptions->list));
	kfree(mm->notifier_subscriptions);
	mm->notifier_subscriptions = LIST_POISON1;  
}

 
void mmu_notifier_unregister(struct mmu_notifier *subscription,
			     struct mm_struct *mm)
{
	BUG_ON(atomic_read(&mm->mm_count) <= 0);

	if (!hlist_unhashed(&subscription->hlist)) {
		 
		int id;

		id = srcu_read_lock(&srcu);
		 
		if (subscription->ops->release)
			subscription->ops->release(subscription, mm);
		srcu_read_unlock(&srcu, id);

		spin_lock(&mm->notifier_subscriptions->lock);
		 
		hlist_del_init_rcu(&subscription->hlist);
		spin_unlock(&mm->notifier_subscriptions->lock);
	}

	 
	synchronize_srcu(&srcu);

	BUG_ON(atomic_read(&mm->mm_count) <= 0);

	mmdrop(mm);
}
EXPORT_SYMBOL_GPL(mmu_notifier_unregister);

static void mmu_notifier_free_rcu(struct rcu_head *rcu)
{
	struct mmu_notifier *subscription =
		container_of(rcu, struct mmu_notifier, rcu);
	struct mm_struct *mm = subscription->mm;

	subscription->ops->free_notifier(subscription);
	 
	mmdrop(mm);
}

 
void mmu_notifier_put(struct mmu_notifier *subscription)
{
	struct mm_struct *mm = subscription->mm;

	spin_lock(&mm->notifier_subscriptions->lock);
	if (WARN_ON(!subscription->users) || --subscription->users)
		goto out_unlock;
	hlist_del_init_rcu(&subscription->hlist);
	spin_unlock(&mm->notifier_subscriptions->lock);

	call_srcu(&srcu, &subscription->rcu, mmu_notifier_free_rcu);
	return;

out_unlock:
	spin_unlock(&mm->notifier_subscriptions->lock);
}
EXPORT_SYMBOL_GPL(mmu_notifier_put);

static int __mmu_interval_notifier_insert(
	struct mmu_interval_notifier *interval_sub, struct mm_struct *mm,
	struct mmu_notifier_subscriptions *subscriptions, unsigned long start,
	unsigned long length, const struct mmu_interval_notifier_ops *ops)
{
	interval_sub->mm = mm;
	interval_sub->ops = ops;
	RB_CLEAR_NODE(&interval_sub->interval_tree.rb);
	interval_sub->interval_tree.start = start;
	 
	if (length == 0 ||
	    check_add_overflow(start, length - 1,
			       &interval_sub->interval_tree.last))
		return -EOVERFLOW;

	 
	if (WARN_ON(atomic_read(&mm->mm_users) <= 0))
		return -EINVAL;

	 
	mmgrab(mm);

	 
	spin_lock(&subscriptions->lock);
	if (subscriptions->active_invalidate_ranges) {
		if (mn_itree_is_invalidating(subscriptions))
			hlist_add_head(&interval_sub->deferred_item,
				       &subscriptions->deferred_list);
		else {
			subscriptions->invalidate_seq |= 1;
			interval_tree_insert(&interval_sub->interval_tree,
					     &subscriptions->itree);
		}
		interval_sub->invalidate_seq = subscriptions->invalidate_seq;
	} else {
		WARN_ON(mn_itree_is_invalidating(subscriptions));
		 
		interval_sub->invalidate_seq =
			subscriptions->invalidate_seq - 1;
		interval_tree_insert(&interval_sub->interval_tree,
				     &subscriptions->itree);
	}
	spin_unlock(&subscriptions->lock);
	return 0;
}

 
int mmu_interval_notifier_insert(struct mmu_interval_notifier *interval_sub,
				 struct mm_struct *mm, unsigned long start,
				 unsigned long length,
				 const struct mmu_interval_notifier_ops *ops)
{
	struct mmu_notifier_subscriptions *subscriptions;
	int ret;

	might_lock(&mm->mmap_lock);

	subscriptions = smp_load_acquire(&mm->notifier_subscriptions);
	if (!subscriptions || !subscriptions->has_itree) {
		ret = mmu_notifier_register(NULL, mm);
		if (ret)
			return ret;
		subscriptions = mm->notifier_subscriptions;
	}
	return __mmu_interval_notifier_insert(interval_sub, mm, subscriptions,
					      start, length, ops);
}
EXPORT_SYMBOL_GPL(mmu_interval_notifier_insert);

int mmu_interval_notifier_insert_locked(
	struct mmu_interval_notifier *interval_sub, struct mm_struct *mm,
	unsigned long start, unsigned long length,
	const struct mmu_interval_notifier_ops *ops)
{
	struct mmu_notifier_subscriptions *subscriptions =
		mm->notifier_subscriptions;
	int ret;

	mmap_assert_write_locked(mm);

	if (!subscriptions || !subscriptions->has_itree) {
		ret = __mmu_notifier_register(NULL, mm);
		if (ret)
			return ret;
		subscriptions = mm->notifier_subscriptions;
	}
	return __mmu_interval_notifier_insert(interval_sub, mm, subscriptions,
					      start, length, ops);
}
EXPORT_SYMBOL_GPL(mmu_interval_notifier_insert_locked);

static bool
mmu_interval_seq_released(struct mmu_notifier_subscriptions *subscriptions,
			  unsigned long seq)
{
	bool ret;

	spin_lock(&subscriptions->lock);
	ret = subscriptions->invalidate_seq != seq;
	spin_unlock(&subscriptions->lock);
	return ret;
}

 
void mmu_interval_notifier_remove(struct mmu_interval_notifier *interval_sub)
{
	struct mm_struct *mm = interval_sub->mm;
	struct mmu_notifier_subscriptions *subscriptions =
		mm->notifier_subscriptions;
	unsigned long seq = 0;

	might_sleep();

	spin_lock(&subscriptions->lock);
	if (mn_itree_is_invalidating(subscriptions)) {
		 
		if (RB_EMPTY_NODE(&interval_sub->interval_tree.rb)) {
			hlist_del(&interval_sub->deferred_item);
		} else {
			hlist_add_head(&interval_sub->deferred_item,
				       &subscriptions->deferred_list);
			seq = subscriptions->invalidate_seq;
		}
	} else {
		WARN_ON(RB_EMPTY_NODE(&interval_sub->interval_tree.rb));
		interval_tree_remove(&interval_sub->interval_tree,
				     &subscriptions->itree);
	}
	spin_unlock(&subscriptions->lock);

	 
	lock_map_acquire(&__mmu_notifier_invalidate_range_start_map);
	lock_map_release(&__mmu_notifier_invalidate_range_start_map);
	if (seq)
		wait_event(subscriptions->wq,
			   mmu_interval_seq_released(subscriptions, seq));

	 
	mmdrop(mm);
}
EXPORT_SYMBOL_GPL(mmu_interval_notifier_remove);

 
void mmu_notifier_synchronize(void)
{
	synchronize_srcu(&srcu);
}
EXPORT_SYMBOL_GPL(mmu_notifier_synchronize);
