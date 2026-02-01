
 

#include <linux/kthread.h>
#include <linux/slab.h>
#include <linux/list.h>
#include <linux/spinlock.h>
#include <linux/freezer.h>
#include "async-thread.h"
#include "ctree.h"

enum {
	WORK_DONE_BIT,
	WORK_ORDER_DONE_BIT,
};

#define NO_THRESHOLD (-1)
#define DFT_THRESHOLD (32)

struct btrfs_workqueue {
	struct workqueue_struct *normal_wq;

	 
	struct btrfs_fs_info *fs_info;

	 
	struct list_head ordered_list;

	 
	spinlock_t list_lock;

	 
	atomic_t pending;

	 
	int limit_active;

	 
	int current_active;

	 
	int thresh;
	unsigned int count;
	spinlock_t thres_lock;
};

struct btrfs_fs_info * __pure btrfs_workqueue_owner(const struct btrfs_workqueue *wq)
{
	return wq->fs_info;
}

struct btrfs_fs_info * __pure btrfs_work_owner(const struct btrfs_work *work)
{
	return work->wq->fs_info;
}

bool btrfs_workqueue_normal_congested(const struct btrfs_workqueue *wq)
{
	 
	if (wq->thresh == NO_THRESHOLD)
		return false;

	return atomic_read(&wq->pending) > wq->thresh * 2;
}

static void btrfs_init_workqueue(struct btrfs_workqueue *wq,
				 struct btrfs_fs_info *fs_info)
{
	wq->fs_info = fs_info;
	atomic_set(&wq->pending, 0);
	INIT_LIST_HEAD(&wq->ordered_list);
	spin_lock_init(&wq->list_lock);
	spin_lock_init(&wq->thres_lock);
}

struct btrfs_workqueue *btrfs_alloc_workqueue(struct btrfs_fs_info *fs_info,
					      const char *name, unsigned int flags,
					      int limit_active, int thresh)
{
	struct btrfs_workqueue *ret = kzalloc(sizeof(*ret), GFP_KERNEL);

	if (!ret)
		return NULL;

	btrfs_init_workqueue(ret, fs_info);

	ret->limit_active = limit_active;
	if (thresh == 0)
		thresh = DFT_THRESHOLD;
	 
	if (thresh < DFT_THRESHOLD) {
		ret->current_active = limit_active;
		ret->thresh = NO_THRESHOLD;
	} else {
		 
		ret->current_active = 1;
		ret->thresh = thresh;
	}

	ret->normal_wq = alloc_workqueue("btrfs-%s", flags, ret->current_active,
					 name);
	if (!ret->normal_wq) {
		kfree(ret);
		return NULL;
	}

	trace_btrfs_workqueue_alloc(ret, name);
	return ret;
}

struct btrfs_workqueue *btrfs_alloc_ordered_workqueue(
				struct btrfs_fs_info *fs_info, const char *name,
				unsigned int flags)
{
	struct btrfs_workqueue *ret;

	ret = kzalloc(sizeof(*ret), GFP_KERNEL);
	if (!ret)
		return NULL;

	btrfs_init_workqueue(ret, fs_info);

	 
	ret->limit_active = 1;
	ret->current_active = 1;
	ret->thresh = NO_THRESHOLD;

	ret->normal_wq = alloc_ordered_workqueue("btrfs-%s", flags, name);
	if (!ret->normal_wq) {
		kfree(ret);
		return NULL;
	}

	trace_btrfs_workqueue_alloc(ret, name);
	return ret;
}

 
static inline void thresh_queue_hook(struct btrfs_workqueue *wq)
{
	if (wq->thresh == NO_THRESHOLD)
		return;
	atomic_inc(&wq->pending);
}

 
static inline void thresh_exec_hook(struct btrfs_workqueue *wq)
{
	int new_current_active;
	long pending;
	int need_change = 0;

	if (wq->thresh == NO_THRESHOLD)
		return;

	atomic_dec(&wq->pending);
	spin_lock(&wq->thres_lock);
	 
	wq->count++;
	wq->count %= (wq->thresh / 4);
	if (!wq->count)
		goto  out;
	new_current_active = wq->current_active;

	 
	pending = atomic_read(&wq->pending);
	if (pending > wq->thresh)
		new_current_active++;
	if (pending < wq->thresh / 2)
		new_current_active--;
	new_current_active = clamp_val(new_current_active, 1, wq->limit_active);
	if (new_current_active != wq->current_active)  {
		need_change = 1;
		wq->current_active = new_current_active;
	}
out:
	spin_unlock(&wq->thres_lock);

	if (need_change) {
		workqueue_set_max_active(wq->normal_wq, wq->current_active);
	}
}

static void run_ordered_work(struct btrfs_workqueue *wq,
			     struct btrfs_work *self)
{
	struct list_head *list = &wq->ordered_list;
	struct btrfs_work *work;
	spinlock_t *lock = &wq->list_lock;
	unsigned long flags;
	bool free_self = false;

	while (1) {
		spin_lock_irqsave(lock, flags);
		if (list_empty(list))
			break;
		work = list_entry(list->next, struct btrfs_work,
				  ordered_list);
		if (!test_bit(WORK_DONE_BIT, &work->flags))
			break;
		 
		smp_rmb();

		 
		if (test_and_set_bit(WORK_ORDER_DONE_BIT, &work->flags))
			break;
		trace_btrfs_ordered_sched(work);
		spin_unlock_irqrestore(lock, flags);
		work->ordered_func(work);

		 
		spin_lock_irqsave(lock, flags);
		list_del(&work->ordered_list);
		spin_unlock_irqrestore(lock, flags);

		if (work == self) {
			 
			free_self = true;
		} else {
			 
			work->ordered_free(work);
			 
			trace_btrfs_all_work_done(wq->fs_info, work);
		}
	}
	spin_unlock_irqrestore(lock, flags);

	if (free_self) {
		self->ordered_free(self);
		 
		trace_btrfs_all_work_done(wq->fs_info, self);
	}
}

static void btrfs_work_helper(struct work_struct *normal_work)
{
	struct btrfs_work *work = container_of(normal_work, struct btrfs_work,
					       normal_work);
	struct btrfs_workqueue *wq = work->wq;
	int need_order = 0;

	 
	if (work->ordered_func)
		need_order = 1;

	trace_btrfs_work_sched(work);
	thresh_exec_hook(wq);
	work->func(work);
	if (need_order) {
		 
		smp_mb__before_atomic();
		set_bit(WORK_DONE_BIT, &work->flags);
		run_ordered_work(wq, work);
	} else {
		 
		trace_btrfs_all_work_done(wq->fs_info, work);
	}
}

void btrfs_init_work(struct btrfs_work *work, btrfs_func_t func,
		     btrfs_func_t ordered_func, btrfs_func_t ordered_free)
{
	work->func = func;
	work->ordered_func = ordered_func;
	work->ordered_free = ordered_free;
	INIT_WORK(&work->normal_work, btrfs_work_helper);
	INIT_LIST_HEAD(&work->ordered_list);
	work->flags = 0;
}

void btrfs_queue_work(struct btrfs_workqueue *wq, struct btrfs_work *work)
{
	unsigned long flags;

	work->wq = wq;
	thresh_queue_hook(wq);
	if (work->ordered_func) {
		spin_lock_irqsave(&wq->list_lock, flags);
		list_add_tail(&work->ordered_list, &wq->ordered_list);
		spin_unlock_irqrestore(&wq->list_lock, flags);
	}
	trace_btrfs_work_queued(work);
	queue_work(wq->normal_wq, &work->normal_work);
}

void btrfs_destroy_workqueue(struct btrfs_workqueue *wq)
{
	if (!wq)
		return;
	destroy_workqueue(wq->normal_wq);
	trace_btrfs_workqueue_destroy(wq);
	kfree(wq);
}

void btrfs_workqueue_set_max(struct btrfs_workqueue *wq, int limit_active)
{
	if (wq)
		wq->limit_active = limit_active;
}

void btrfs_flush_workqueue(struct btrfs_workqueue *wq)
{
	flush_workqueue(wq->normal_wq);
}
