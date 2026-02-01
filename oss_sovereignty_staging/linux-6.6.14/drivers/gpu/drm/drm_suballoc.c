
 
 

#include <drm/drm_suballoc.h>
#include <drm/drm_print.h>
#include <linux/slab.h>
#include <linux/sched.h>
#include <linux/wait.h>
#include <linux/dma-fence.h>

static void drm_suballoc_remove_locked(struct drm_suballoc *sa);
static void drm_suballoc_try_free(struct drm_suballoc_manager *sa_manager);

 
void drm_suballoc_manager_init(struct drm_suballoc_manager *sa_manager,
			       size_t size, size_t align)
{
	unsigned int i;

	BUILD_BUG_ON(!is_power_of_2(DRM_SUBALLOC_MAX_QUEUES));

	if (!align)
		align = 1;

	 
	if (WARN_ON_ONCE(align & (align - 1)))
		align = roundup_pow_of_two(align);

	init_waitqueue_head(&sa_manager->wq);
	sa_manager->size = size;
	sa_manager->align = align;
	sa_manager->hole = &sa_manager->olist;
	INIT_LIST_HEAD(&sa_manager->olist);
	for (i = 0; i < DRM_SUBALLOC_MAX_QUEUES; ++i)
		INIT_LIST_HEAD(&sa_manager->flist[i]);
}
EXPORT_SYMBOL(drm_suballoc_manager_init);

 
void drm_suballoc_manager_fini(struct drm_suballoc_manager *sa_manager)
{
	struct drm_suballoc *sa, *tmp;

	if (!sa_manager->size)
		return;

	if (!list_empty(&sa_manager->olist)) {
		sa_manager->hole = &sa_manager->olist;
		drm_suballoc_try_free(sa_manager);
		if (!list_empty(&sa_manager->olist))
			DRM_ERROR("sa_manager is not empty, clearing anyway\n");
	}
	list_for_each_entry_safe(sa, tmp, &sa_manager->olist, olist) {
		drm_suballoc_remove_locked(sa);
	}

	sa_manager->size = 0;
}
EXPORT_SYMBOL(drm_suballoc_manager_fini);

static void drm_suballoc_remove_locked(struct drm_suballoc *sa)
{
	struct drm_suballoc_manager *sa_manager = sa->manager;

	if (sa_manager->hole == &sa->olist)
		sa_manager->hole = sa->olist.prev;

	list_del_init(&sa->olist);
	list_del_init(&sa->flist);
	dma_fence_put(sa->fence);
	kfree(sa);
}

static void drm_suballoc_try_free(struct drm_suballoc_manager *sa_manager)
{
	struct drm_suballoc *sa, *tmp;

	if (sa_manager->hole->next == &sa_manager->olist)
		return;

	sa = list_entry(sa_manager->hole->next, struct drm_suballoc, olist);
	list_for_each_entry_safe_from(sa, tmp, &sa_manager->olist, olist) {
		if (!sa->fence || !dma_fence_is_signaled(sa->fence))
			return;

		drm_suballoc_remove_locked(sa);
	}
}

static size_t drm_suballoc_hole_soffset(struct drm_suballoc_manager *sa_manager)
{
	struct list_head *hole = sa_manager->hole;

	if (hole != &sa_manager->olist)
		return list_entry(hole, struct drm_suballoc, olist)->eoffset;

	return 0;
}

static size_t drm_suballoc_hole_eoffset(struct drm_suballoc_manager *sa_manager)
{
	struct list_head *hole = sa_manager->hole;

	if (hole->next != &sa_manager->olist)
		return list_entry(hole->next, struct drm_suballoc, olist)->soffset;
	return sa_manager->size;
}

static bool drm_suballoc_try_alloc(struct drm_suballoc_manager *sa_manager,
				   struct drm_suballoc *sa,
				   size_t size, size_t align)
{
	size_t soffset, eoffset, wasted;

	soffset = drm_suballoc_hole_soffset(sa_manager);
	eoffset = drm_suballoc_hole_eoffset(sa_manager);
	wasted = round_up(soffset, align) - soffset;

	if ((eoffset - soffset) >= (size + wasted)) {
		soffset += wasted;

		sa->manager = sa_manager;
		sa->soffset = soffset;
		sa->eoffset = soffset + size;
		list_add(&sa->olist, sa_manager->hole);
		INIT_LIST_HEAD(&sa->flist);
		sa_manager->hole = &sa->olist;
		return true;
	}
	return false;
}

static bool __drm_suballoc_event(struct drm_suballoc_manager *sa_manager,
				 size_t size, size_t align)
{
	size_t soffset, eoffset, wasted;
	unsigned int i;

	for (i = 0; i < DRM_SUBALLOC_MAX_QUEUES; ++i)
		if (!list_empty(&sa_manager->flist[i]))
			return true;

	soffset = drm_suballoc_hole_soffset(sa_manager);
	eoffset = drm_suballoc_hole_eoffset(sa_manager);
	wasted = round_up(soffset, align) - soffset;

	return ((eoffset - soffset) >= (size + wasted));
}

 
static bool drm_suballoc_event(struct drm_suballoc_manager *sa_manager,
			       size_t size, size_t align)
{
	bool ret;

	spin_lock(&sa_manager->wq.lock);
	ret = __drm_suballoc_event(sa_manager, size, align);
	spin_unlock(&sa_manager->wq.lock);
	return ret;
}

static bool drm_suballoc_next_hole(struct drm_suballoc_manager *sa_manager,
				   struct dma_fence **fences,
				   unsigned int *tries)
{
	struct drm_suballoc *best_bo = NULL;
	unsigned int i, best_idx;
	size_t soffset, best, tmp;

	 
	if (sa_manager->hole->next == &sa_manager->olist) {
		 
		sa_manager->hole = &sa_manager->olist;
		return true;
	}

	soffset = drm_suballoc_hole_soffset(sa_manager);
	 
	best = sa_manager->size * 2;
	 
	for (i = 0; i < DRM_SUBALLOC_MAX_QUEUES; ++i) {
		struct drm_suballoc *sa;

		fences[i] = NULL;

		if (list_empty(&sa_manager->flist[i]))
			continue;

		sa = list_first_entry(&sa_manager->flist[i],
				      struct drm_suballoc, flist);

		if (!dma_fence_is_signaled(sa->fence)) {
			fences[i] = sa->fence;
			continue;
		}

		 
		if (tries[i] > 2)
			continue;

		tmp = sa->soffset;
		if (tmp < soffset) {
			 
			tmp += sa_manager->size;
		}
		tmp -= soffset;
		if (tmp < best) {
			 
			best = tmp;
			best_idx = i;
			best_bo = sa;
		}
	}

	if (best_bo) {
		++tries[best_idx];
		sa_manager->hole = best_bo->olist.prev;

		 
		drm_suballoc_remove_locked(best_bo);
		return true;
	}
	return false;
}

 
struct drm_suballoc *
drm_suballoc_new(struct drm_suballoc_manager *sa_manager, size_t size,
		 gfp_t gfp, bool intr, size_t align)
{
	struct dma_fence *fences[DRM_SUBALLOC_MAX_QUEUES];
	unsigned int tries[DRM_SUBALLOC_MAX_QUEUES];
	unsigned int count;
	int i, r;
	struct drm_suballoc *sa;

	if (WARN_ON_ONCE(align > sa_manager->align))
		return ERR_PTR(-EINVAL);
	if (WARN_ON_ONCE(size > sa_manager->size || !size))
		return ERR_PTR(-EINVAL);

	if (!align)
		align = sa_manager->align;

	sa = kmalloc(sizeof(*sa), gfp);
	if (!sa)
		return ERR_PTR(-ENOMEM);
	sa->manager = sa_manager;
	sa->fence = NULL;
	INIT_LIST_HEAD(&sa->olist);
	INIT_LIST_HEAD(&sa->flist);

	spin_lock(&sa_manager->wq.lock);
	do {
		for (i = 0; i < DRM_SUBALLOC_MAX_QUEUES; ++i)
			tries[i] = 0;

		do {
			drm_suballoc_try_free(sa_manager);

			if (drm_suballoc_try_alloc(sa_manager, sa,
						   size, align)) {
				spin_unlock(&sa_manager->wq.lock);
				return sa;
			}

			 
		} while (drm_suballoc_next_hole(sa_manager, fences, tries));

		for (i = 0, count = 0; i < DRM_SUBALLOC_MAX_QUEUES; ++i)
			if (fences[i])
				fences[count++] = dma_fence_get(fences[i]);

		if (count) {
			long t;

			spin_unlock(&sa_manager->wq.lock);
			t = dma_fence_wait_any_timeout(fences, count, intr,
						       MAX_SCHEDULE_TIMEOUT,
						       NULL);
			for (i = 0; i < count; ++i)
				dma_fence_put(fences[i]);

			r = (t > 0) ? 0 : t;
			spin_lock(&sa_manager->wq.lock);
		} else if (intr) {
			 
			r = wait_event_interruptible_locked
				(sa_manager->wq,
				 __drm_suballoc_event(sa_manager, size, align));
		} else {
			spin_unlock(&sa_manager->wq.lock);
			wait_event(sa_manager->wq,
				   drm_suballoc_event(sa_manager, size, align));
			r = 0;
			spin_lock(&sa_manager->wq.lock);
		}
	} while (!r);

	spin_unlock(&sa_manager->wq.lock);
	kfree(sa);
	return ERR_PTR(r);
}
EXPORT_SYMBOL(drm_suballoc_new);

 
void drm_suballoc_free(struct drm_suballoc *suballoc,
		       struct dma_fence *fence)
{
	struct drm_suballoc_manager *sa_manager;

	if (!suballoc)
		return;

	sa_manager = suballoc->manager;

	spin_lock(&sa_manager->wq.lock);
	if (fence && !dma_fence_is_signaled(fence)) {
		u32 idx;

		suballoc->fence = dma_fence_get(fence);
		idx = fence->context & (DRM_SUBALLOC_MAX_QUEUES - 1);
		list_add_tail(&suballoc->flist, &sa_manager->flist[idx]);
	} else {
		drm_suballoc_remove_locked(suballoc);
	}
	wake_up_all_locked(&sa_manager->wq);
	spin_unlock(&sa_manager->wq.lock);
}
EXPORT_SYMBOL(drm_suballoc_free);

#ifdef CONFIG_DEBUG_FS
void drm_suballoc_dump_debug_info(struct drm_suballoc_manager *sa_manager,
				  struct drm_printer *p,
				  unsigned long long suballoc_base)
{
	struct drm_suballoc *i;

	spin_lock(&sa_manager->wq.lock);
	list_for_each_entry(i, &sa_manager->olist, olist) {
		unsigned long long soffset = i->soffset;
		unsigned long long eoffset = i->eoffset;

		if (&i->olist == sa_manager->hole)
			drm_puts(p, ">");
		else
			drm_puts(p, " ");

		drm_printf(p, "[0x%010llx 0x%010llx] size %8lld",
			   suballoc_base + soffset, suballoc_base + eoffset,
			   eoffset - soffset);

		if (i->fence)
			drm_printf(p, " protected by 0x%016llx on context %llu",
				   (unsigned long long)i->fence->seqno,
				   (unsigned long long)i->fence->context);

		drm_puts(p, "\n");
	}
	spin_unlock(&sa_manager->wq.lock);
}
EXPORT_SYMBOL(drm_suballoc_dump_debug_info);
#endif
MODULE_AUTHOR("Multiple");
MODULE_DESCRIPTION("Range suballocator helper");
MODULE_LICENSE("Dual MIT/GPL");
