
 
 

#include <linux/dma-resv.h>
#include <linux/dma-fence-array.h>
#include <linux/export.h>
#include <linux/mm.h>
#include <linux/sched/mm.h>
#include <linux/mmu_notifier.h>
#include <linux/seq_file.h>

 

DEFINE_WD_CLASS(reservation_ww_class);
EXPORT_SYMBOL(reservation_ww_class);

 
#define DMA_RESV_LIST_MASK	0x3

struct dma_resv_list {
	struct rcu_head rcu;
	u32 num_fences, max_fences;
	struct dma_fence __rcu *table[];
};

 
static void dma_resv_list_entry(struct dma_resv_list *list, unsigned int index,
				struct dma_resv *resv, struct dma_fence **fence,
				enum dma_resv_usage *usage)
{
	long tmp;

	tmp = (long)rcu_dereference_check(list->table[index],
					  resv ? dma_resv_held(resv) : true);
	*fence = (struct dma_fence *)(tmp & ~DMA_RESV_LIST_MASK);
	if (usage)
		*usage = tmp & DMA_RESV_LIST_MASK;
}

 
static void dma_resv_list_set(struct dma_resv_list *list,
			      unsigned int index,
			      struct dma_fence *fence,
			      enum dma_resv_usage usage)
{
	long tmp = ((long)fence) | usage;

	RCU_INIT_POINTER(list->table[index], (struct dma_fence *)tmp);
}

 
static struct dma_resv_list *dma_resv_list_alloc(unsigned int max_fences)
{
	struct dma_resv_list *list;
	size_t size;

	 
	size = kmalloc_size_roundup(struct_size(list, table, max_fences));

	list = kmalloc(size, GFP_KERNEL);
	if (!list)
		return NULL;

	 
	list->max_fences = (size - offsetof(typeof(*list), table)) /
		sizeof(*list->table);

	return list;
}

 
static void dma_resv_list_free(struct dma_resv_list *list)
{
	unsigned int i;

	if (!list)
		return;

	for (i = 0; i < list->num_fences; ++i) {
		struct dma_fence *fence;

		dma_resv_list_entry(list, i, NULL, &fence, NULL);
		dma_fence_put(fence);
	}
	kfree_rcu(list, rcu);
}

 
void dma_resv_init(struct dma_resv *obj)
{
	ww_mutex_init(&obj->lock, &reservation_ww_class);

	RCU_INIT_POINTER(obj->fences, NULL);
}
EXPORT_SYMBOL(dma_resv_init);

 
void dma_resv_fini(struct dma_resv *obj)
{
	 
	dma_resv_list_free(rcu_dereference_protected(obj->fences, true));
	ww_mutex_destroy(&obj->lock);
}
EXPORT_SYMBOL(dma_resv_fini);

 
static inline struct dma_resv_list *dma_resv_fences_list(struct dma_resv *obj)
{
	return rcu_dereference_check(obj->fences, dma_resv_held(obj));
}

 
int dma_resv_reserve_fences(struct dma_resv *obj, unsigned int num_fences)
{
	struct dma_resv_list *old, *new;
	unsigned int i, j, k, max;

	dma_resv_assert_held(obj);

	old = dma_resv_fences_list(obj);
	if (old && old->max_fences) {
		if ((old->num_fences + num_fences) <= old->max_fences)
			return 0;
		max = max(old->num_fences + num_fences, old->max_fences * 2);
	} else {
		max = max(4ul, roundup_pow_of_two(num_fences));
	}

	new = dma_resv_list_alloc(max);
	if (!new)
		return -ENOMEM;

	 
	for (i = 0, j = 0, k = max; i < (old ? old->num_fences : 0); ++i) {
		enum dma_resv_usage usage;
		struct dma_fence *fence;

		dma_resv_list_entry(old, i, obj, &fence, &usage);
		if (dma_fence_is_signaled(fence))
			RCU_INIT_POINTER(new->table[--k], fence);
		else
			dma_resv_list_set(new, j++, fence, usage);
	}
	new->num_fences = j;

	 
	rcu_assign_pointer(obj->fences, new);

	if (!old)
		return 0;

	 
	for (i = k; i < max; ++i) {
		struct dma_fence *fence;

		fence = rcu_dereference_protected(new->table[i],
						  dma_resv_held(obj));
		dma_fence_put(fence);
	}
	kfree_rcu(old, rcu);

	return 0;
}
EXPORT_SYMBOL(dma_resv_reserve_fences);

#ifdef CONFIG_DEBUG_MUTEXES
 
void dma_resv_reset_max_fences(struct dma_resv *obj)
{
	struct dma_resv_list *fences = dma_resv_fences_list(obj);

	dma_resv_assert_held(obj);

	 
	if (fences)
		fences->max_fences = fences->num_fences;
}
EXPORT_SYMBOL(dma_resv_reset_max_fences);
#endif

 
void dma_resv_add_fence(struct dma_resv *obj, struct dma_fence *fence,
			enum dma_resv_usage usage)
{
	struct dma_resv_list *fobj;
	struct dma_fence *old;
	unsigned int i, count;

	dma_fence_get(fence);

	dma_resv_assert_held(obj);

	 
	WARN_ON(dma_fence_is_container(fence));

	fobj = dma_resv_fences_list(obj);
	count = fobj->num_fences;

	for (i = 0; i < count; ++i) {
		enum dma_resv_usage old_usage;

		dma_resv_list_entry(fobj, i, obj, &old, &old_usage);
		if ((old->context == fence->context && old_usage >= usage &&
		     dma_fence_is_later_or_same(fence, old)) ||
		    dma_fence_is_signaled(old)) {
			dma_resv_list_set(fobj, i, fence, usage);
			dma_fence_put(old);
			return;
		}
	}

	BUG_ON(fobj->num_fences >= fobj->max_fences);
	count++;

	dma_resv_list_set(fobj, i, fence, usage);
	 
	smp_store_mb(fobj->num_fences, count);
}
EXPORT_SYMBOL(dma_resv_add_fence);

 
void dma_resv_replace_fences(struct dma_resv *obj, uint64_t context,
			     struct dma_fence *replacement,
			     enum dma_resv_usage usage)
{
	struct dma_resv_list *list;
	unsigned int i;

	dma_resv_assert_held(obj);

	list = dma_resv_fences_list(obj);
	for (i = 0; list && i < list->num_fences; ++i) {
		struct dma_fence *old;

		dma_resv_list_entry(list, i, obj, &old, NULL);
		if (old->context != context)
			continue;

		dma_resv_list_set(list, i, dma_fence_get(replacement), usage);
		dma_fence_put(old);
	}
}
EXPORT_SYMBOL(dma_resv_replace_fences);

 
static void dma_resv_iter_restart_unlocked(struct dma_resv_iter *cursor)
{
	cursor->index = 0;
	cursor->num_fences = 0;
	cursor->fences = dma_resv_fences_list(cursor->obj);
	if (cursor->fences)
		cursor->num_fences = cursor->fences->num_fences;
	cursor->is_restarted = true;
}

 
static void dma_resv_iter_walk_unlocked(struct dma_resv_iter *cursor)
{
	if (!cursor->fences)
		return;

	do {
		 
		dma_fence_put(cursor->fence);

		if (cursor->index >= cursor->num_fences) {
			cursor->fence = NULL;
			break;

		}

		dma_resv_list_entry(cursor->fences, cursor->index++,
				    cursor->obj, &cursor->fence,
				    &cursor->fence_usage);
		cursor->fence = dma_fence_get_rcu(cursor->fence);
		if (!cursor->fence) {
			dma_resv_iter_restart_unlocked(cursor);
			continue;
		}

		if (!dma_fence_is_signaled(cursor->fence) &&
		    cursor->usage >= cursor->fence_usage)
			break;
	} while (true);
}

 
struct dma_fence *dma_resv_iter_first_unlocked(struct dma_resv_iter *cursor)
{
	rcu_read_lock();
	do {
		dma_resv_iter_restart_unlocked(cursor);
		dma_resv_iter_walk_unlocked(cursor);
	} while (dma_resv_fences_list(cursor->obj) != cursor->fences);
	rcu_read_unlock();

	return cursor->fence;
}
EXPORT_SYMBOL(dma_resv_iter_first_unlocked);

 
struct dma_fence *dma_resv_iter_next_unlocked(struct dma_resv_iter *cursor)
{
	bool restart;

	rcu_read_lock();
	cursor->is_restarted = false;
	restart = dma_resv_fences_list(cursor->obj) != cursor->fences;
	do {
		if (restart)
			dma_resv_iter_restart_unlocked(cursor);
		dma_resv_iter_walk_unlocked(cursor);
		restart = true;
	} while (dma_resv_fences_list(cursor->obj) != cursor->fences);
	rcu_read_unlock();

	return cursor->fence;
}
EXPORT_SYMBOL(dma_resv_iter_next_unlocked);

 
struct dma_fence *dma_resv_iter_first(struct dma_resv_iter *cursor)
{
	struct dma_fence *fence;

	dma_resv_assert_held(cursor->obj);

	cursor->index = 0;
	cursor->fences = dma_resv_fences_list(cursor->obj);

	fence = dma_resv_iter_next(cursor);
	cursor->is_restarted = true;
	return fence;
}
EXPORT_SYMBOL_GPL(dma_resv_iter_first);

 
struct dma_fence *dma_resv_iter_next(struct dma_resv_iter *cursor)
{
	struct dma_fence *fence;

	dma_resv_assert_held(cursor->obj);

	cursor->is_restarted = false;

	do {
		if (!cursor->fences ||
		    cursor->index >= cursor->fences->num_fences)
			return NULL;

		dma_resv_list_entry(cursor->fences, cursor->index++,
				    cursor->obj, &fence, &cursor->fence_usage);
	} while (cursor->fence_usage > cursor->usage);

	return fence;
}
EXPORT_SYMBOL_GPL(dma_resv_iter_next);

 
int dma_resv_copy_fences(struct dma_resv *dst, struct dma_resv *src)
{
	struct dma_resv_iter cursor;
	struct dma_resv_list *list;
	struct dma_fence *f;

	dma_resv_assert_held(dst);

	list = NULL;

	dma_resv_iter_begin(&cursor, src, DMA_RESV_USAGE_BOOKKEEP);
	dma_resv_for_each_fence_unlocked(&cursor, f) {

		if (dma_resv_iter_is_restarted(&cursor)) {
			dma_resv_list_free(list);

			list = dma_resv_list_alloc(cursor.num_fences);
			if (!list) {
				dma_resv_iter_end(&cursor);
				return -ENOMEM;
			}
			list->num_fences = 0;
		}

		dma_fence_get(f);
		dma_resv_list_set(list, list->num_fences++, f,
				  dma_resv_iter_usage(&cursor));
	}
	dma_resv_iter_end(&cursor);

	list = rcu_replace_pointer(dst->fences, list, dma_resv_held(dst));
	dma_resv_list_free(list);
	return 0;
}
EXPORT_SYMBOL(dma_resv_copy_fences);

 
int dma_resv_get_fences(struct dma_resv *obj, enum dma_resv_usage usage,
			unsigned int *num_fences, struct dma_fence ***fences)
{
	struct dma_resv_iter cursor;
	struct dma_fence *fence;

	*num_fences = 0;
	*fences = NULL;

	dma_resv_iter_begin(&cursor, obj, usage);
	dma_resv_for_each_fence_unlocked(&cursor, fence) {

		if (dma_resv_iter_is_restarted(&cursor)) {
			struct dma_fence **new_fences;
			unsigned int count;

			while (*num_fences)
				dma_fence_put((*fences)[--(*num_fences)]);

			count = cursor.num_fences + 1;

			 
			new_fences = krealloc_array(*fences, count,
						    sizeof(void *),
						    GFP_KERNEL);
			if (count && !new_fences) {
				kfree(*fences);
				*fences = NULL;
				*num_fences = 0;
				dma_resv_iter_end(&cursor);
				return -ENOMEM;
			}
			*fences = new_fences;
		}

		(*fences)[(*num_fences)++] = dma_fence_get(fence);
	}
	dma_resv_iter_end(&cursor);

	return 0;
}
EXPORT_SYMBOL_GPL(dma_resv_get_fences);

 
int dma_resv_get_singleton(struct dma_resv *obj, enum dma_resv_usage usage,
			   struct dma_fence **fence)
{
	struct dma_fence_array *array;
	struct dma_fence **fences;
	unsigned count;
	int r;

	r = dma_resv_get_fences(obj, usage, &count, &fences);
        if (r)
		return r;

	if (count == 0) {
		*fence = NULL;
		return 0;
	}

	if (count == 1) {
		*fence = fences[0];
		kfree(fences);
		return 0;
	}

	array = dma_fence_array_create(count, fences,
				       dma_fence_context_alloc(1),
				       1, false);
	if (!array) {
		while (count--)
			dma_fence_put(fences[count]);
		kfree(fences);
		return -ENOMEM;
	}

	*fence = &array->base;
	return 0;
}
EXPORT_SYMBOL_GPL(dma_resv_get_singleton);

 
long dma_resv_wait_timeout(struct dma_resv *obj, enum dma_resv_usage usage,
			   bool intr, unsigned long timeout)
{
	long ret = timeout ? timeout : 1;
	struct dma_resv_iter cursor;
	struct dma_fence *fence;

	dma_resv_iter_begin(&cursor, obj, usage);
	dma_resv_for_each_fence_unlocked(&cursor, fence) {

		ret = dma_fence_wait_timeout(fence, intr, ret);
		if (ret <= 0) {
			dma_resv_iter_end(&cursor);
			return ret;
		}
	}
	dma_resv_iter_end(&cursor);

	return ret;
}
EXPORT_SYMBOL_GPL(dma_resv_wait_timeout);

 
void dma_resv_set_deadline(struct dma_resv *obj, enum dma_resv_usage usage,
			   ktime_t deadline)
{
	struct dma_resv_iter cursor;
	struct dma_fence *fence;

	dma_resv_iter_begin(&cursor, obj, usage);
	dma_resv_for_each_fence_unlocked(&cursor, fence) {
		dma_fence_set_deadline(fence, deadline);
	}
	dma_resv_iter_end(&cursor);
}
EXPORT_SYMBOL_GPL(dma_resv_set_deadline);

 
bool dma_resv_test_signaled(struct dma_resv *obj, enum dma_resv_usage usage)
{
	struct dma_resv_iter cursor;
	struct dma_fence *fence;

	dma_resv_iter_begin(&cursor, obj, usage);
	dma_resv_for_each_fence_unlocked(&cursor, fence) {
		dma_resv_iter_end(&cursor);
		return false;
	}
	dma_resv_iter_end(&cursor);
	return true;
}
EXPORT_SYMBOL_GPL(dma_resv_test_signaled);

 
void dma_resv_describe(struct dma_resv *obj, struct seq_file *seq)
{
	static const char *usage[] = { "kernel", "write", "read", "bookkeep" };
	struct dma_resv_iter cursor;
	struct dma_fence *fence;

	dma_resv_for_each_fence(&cursor, obj, DMA_RESV_USAGE_READ, fence) {
		seq_printf(seq, "\t%s fence:",
			   usage[dma_resv_iter_usage(&cursor)]);
		dma_fence_describe(fence, seq);
	}
}
EXPORT_SYMBOL_GPL(dma_resv_describe);

#if IS_ENABLED(CONFIG_LOCKDEP)
static int __init dma_resv_lockdep(void)
{
	struct mm_struct *mm = mm_alloc();
	struct ww_acquire_ctx ctx;
	struct dma_resv obj;
	struct address_space mapping;
	int ret;

	if (!mm)
		return -ENOMEM;

	dma_resv_init(&obj);
	address_space_init_once(&mapping);

	mmap_read_lock(mm);
	ww_acquire_init(&ctx, &reservation_ww_class);
	ret = dma_resv_lock(&obj, &ctx);
	if (ret == -EDEADLK)
		dma_resv_lock_slow(&obj, &ctx);
	fs_reclaim_acquire(GFP_KERNEL);
	 
	i_mmap_lock_write(&mapping);
	i_mmap_unlock_write(&mapping);
#ifdef CONFIG_MMU_NOTIFIER
	lock_map_acquire(&__mmu_notifier_invalidate_range_start_map);
	__dma_fence_might_wait();
	lock_map_release(&__mmu_notifier_invalidate_range_start_map);
#else
	__dma_fence_might_wait();
#endif
	fs_reclaim_release(GFP_KERNEL);
	ww_mutex_unlock(&obj.lock);
	ww_acquire_fini(&ctx);
	mmap_read_unlock(mm);

	mmput(mm);

	return 0;
}
subsys_initcall(dma_resv_lockdep);
#endif
