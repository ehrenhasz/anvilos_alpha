 

#include <linux/oom.h>
#include <linux/sched/mm.h>
#include <linux/shmem_fs.h>
#include <linux/slab.h>
#include <linux/swap.h>
#include <linux/pci.h>
#include <linux/dma-buf.h>
#include <linux/vmalloc.h>

#include "gt/intel_gt_requests.h"

#include "i915_trace.h"

static bool swap_available(void)
{
	return get_nr_swap_pages() > 0;
}

static bool can_release_pages(struct drm_i915_gem_object *obj)
{
	 
	if (!i915_gem_object_is_shrinkable(obj))
		return false;

	 
	return swap_available() || obj->mm.madv == I915_MADV_DONTNEED;
}

static bool drop_pages(struct drm_i915_gem_object *obj,
		       unsigned long shrink, bool trylock_vm)
{
	unsigned long flags;

	flags = 0;
	if (shrink & I915_SHRINK_ACTIVE)
		flags |= I915_GEM_OBJECT_UNBIND_ACTIVE;
	if (!(shrink & I915_SHRINK_BOUND))
		flags |= I915_GEM_OBJECT_UNBIND_TEST;
	if (trylock_vm)
		flags |= I915_GEM_OBJECT_UNBIND_VM_TRYLOCK;

	if (i915_gem_object_unbind(obj, flags) == 0)
		return true;

	return false;
}

static int try_to_writeback(struct drm_i915_gem_object *obj, unsigned int flags)
{
	if (obj->ops->shrink) {
		unsigned int shrink_flags = 0;

		if (!(flags & I915_SHRINK_ACTIVE))
			shrink_flags |= I915_GEM_OBJECT_SHRINK_NO_GPU_WAIT;

		if (flags & I915_SHRINK_WRITEBACK)
			shrink_flags |= I915_GEM_OBJECT_SHRINK_WRITEBACK;

		return obj->ops->shrink(obj, shrink_flags);
	}

	return 0;
}

 
unsigned long
i915_gem_shrink(struct i915_gem_ww_ctx *ww,
		struct drm_i915_private *i915,
		unsigned long target,
		unsigned long *nr_scanned,
		unsigned int shrink)
{
	const struct {
		struct list_head *list;
		unsigned int bit;
	} phases[] = {
		{ &i915->mm.purge_list, ~0u },
		{
			&i915->mm.shrink_list,
			I915_SHRINK_BOUND | I915_SHRINK_UNBOUND
		},
		{ NULL, 0 },
	}, *phase;
	intel_wakeref_t wakeref = 0;
	unsigned long count = 0;
	unsigned long scanned = 0;
	int err = 0;

	 
	bool trylock_vm = !ww && intel_vm_no_concurrent_access_wa(i915);

	trace_i915_gem_shrink(i915, target, shrink);

	 
	if (shrink & I915_SHRINK_BOUND) {
		wakeref = intel_runtime_pm_get_if_in_use(&i915->runtime_pm);
		if (!wakeref)
			shrink &= ~I915_SHRINK_BOUND;
	}

	 
	if (shrink & I915_SHRINK_ACTIVE)
		 
		intel_gt_retire_requests(to_gt(i915));

	 
	for (phase = phases; phase->list; phase++) {
		struct list_head still_in_list;
		struct drm_i915_gem_object *obj;
		unsigned long flags;

		if ((shrink & phase->bit) == 0)
			continue;

		INIT_LIST_HEAD(&still_in_list);

		 
		spin_lock_irqsave(&i915->mm.obj_lock, flags);
		while (count < target &&
		       (obj = list_first_entry_or_null(phase->list,
						       typeof(*obj),
						       mm.link))) {
			list_move_tail(&obj->mm.link, &still_in_list);

			if (shrink & I915_SHRINK_VMAPS &&
			    !is_vmalloc_addr(obj->mm.mapping))
				continue;

			if (!(shrink & I915_SHRINK_ACTIVE) &&
			    i915_gem_object_is_framebuffer(obj))
				continue;

			if (!can_release_pages(obj))
				continue;

			if (!kref_get_unless_zero(&obj->base.refcount))
				continue;

			spin_unlock_irqrestore(&i915->mm.obj_lock, flags);

			 
			if (!ww) {
				if (!i915_gem_object_trylock(obj, NULL))
					goto skip;
			} else {
				err = i915_gem_object_lock(obj, ww);
				if (err)
					goto skip;
			}

			if (drop_pages(obj, shrink, trylock_vm) &&
			    !__i915_gem_object_put_pages(obj) &&
			    !try_to_writeback(obj, shrink))
				count += obj->base.size >> PAGE_SHIFT;

			if (!ww)
				i915_gem_object_unlock(obj);

			scanned += obj->base.size >> PAGE_SHIFT;
skip:
			i915_gem_object_put(obj);

			spin_lock_irqsave(&i915->mm.obj_lock, flags);
			if (err)
				break;
		}
		list_splice_tail(&still_in_list, phase->list);
		spin_unlock_irqrestore(&i915->mm.obj_lock, flags);
		if (err)
			break;
	}

	if (shrink & I915_SHRINK_BOUND)
		intel_runtime_pm_put(&i915->runtime_pm, wakeref);

	if (err)
		return err;

	if (nr_scanned)
		*nr_scanned += scanned;
	return count;
}

 
unsigned long i915_gem_shrink_all(struct drm_i915_private *i915)
{
	intel_wakeref_t wakeref;
	unsigned long freed = 0;

	with_intel_runtime_pm(&i915->runtime_pm, wakeref) {
		freed = i915_gem_shrink(NULL, i915, -1UL, NULL,
					I915_SHRINK_BOUND |
					I915_SHRINK_UNBOUND);
	}

	return freed;
}

static unsigned long
i915_gem_shrinker_count(struct shrinker *shrinker, struct shrink_control *sc)
{
	struct drm_i915_private *i915 =
		container_of(shrinker, struct drm_i915_private, mm.shrinker);
	unsigned long num_objects;
	unsigned long count;

	count = READ_ONCE(i915->mm.shrink_memory) >> PAGE_SHIFT;
	num_objects = READ_ONCE(i915->mm.shrink_count);

	 
	if (num_objects) {
		unsigned long avg = 2 * count / num_objects;

		i915->mm.shrinker.batch =
			max((i915->mm.shrinker.batch + avg) >> 1,
			    128ul  );
	}

	return count;
}

static unsigned long
i915_gem_shrinker_scan(struct shrinker *shrinker, struct shrink_control *sc)
{
	struct drm_i915_private *i915 =
		container_of(shrinker, struct drm_i915_private, mm.shrinker);
	unsigned long freed;

	sc->nr_scanned = 0;

	freed = i915_gem_shrink(NULL, i915,
				sc->nr_to_scan,
				&sc->nr_scanned,
				I915_SHRINK_BOUND |
				I915_SHRINK_UNBOUND);
	if (sc->nr_scanned < sc->nr_to_scan && current_is_kswapd()) {
		intel_wakeref_t wakeref;

		with_intel_runtime_pm(&i915->runtime_pm, wakeref) {
			freed += i915_gem_shrink(NULL, i915,
						 sc->nr_to_scan - sc->nr_scanned,
						 &sc->nr_scanned,
						 I915_SHRINK_ACTIVE |
						 I915_SHRINK_BOUND |
						 I915_SHRINK_UNBOUND |
						 I915_SHRINK_WRITEBACK);
		}
	}

	return sc->nr_scanned ? freed : SHRINK_STOP;
}

static int
i915_gem_shrinker_oom(struct notifier_block *nb, unsigned long event, void *ptr)
{
	struct drm_i915_private *i915 =
		container_of(nb, struct drm_i915_private, mm.oom_notifier);
	struct drm_i915_gem_object *obj;
	unsigned long unevictable, available, freed_pages;
	intel_wakeref_t wakeref;
	unsigned long flags;

	freed_pages = 0;
	with_intel_runtime_pm(&i915->runtime_pm, wakeref)
		freed_pages += i915_gem_shrink(NULL, i915, -1UL, NULL,
					       I915_SHRINK_BOUND |
					       I915_SHRINK_UNBOUND |
					       I915_SHRINK_WRITEBACK);

	 
	available = unevictable = 0;
	spin_lock_irqsave(&i915->mm.obj_lock, flags);
	list_for_each_entry(obj, &i915->mm.shrink_list, mm.link) {
		if (!can_release_pages(obj))
			unevictable += obj->base.size >> PAGE_SHIFT;
		else
			available += obj->base.size >> PAGE_SHIFT;
	}
	spin_unlock_irqrestore(&i915->mm.obj_lock, flags);

	if (freed_pages || available)
		pr_info("Purging GPU memory, %lu pages freed, "
			"%lu pages still pinned, %lu pages left available.\n",
			freed_pages, unevictable, available);

	*(unsigned long *)ptr += freed_pages;
	return NOTIFY_DONE;
}

static int
i915_gem_shrinker_vmap(struct notifier_block *nb, unsigned long event, void *ptr)
{
	struct drm_i915_private *i915 =
		container_of(nb, struct drm_i915_private, mm.vmap_notifier);
	struct i915_vma *vma, *next;
	unsigned long freed_pages = 0;
	intel_wakeref_t wakeref;

	with_intel_runtime_pm(&i915->runtime_pm, wakeref)
		freed_pages += i915_gem_shrink(NULL, i915, -1UL, NULL,
					       I915_SHRINK_BOUND |
					       I915_SHRINK_UNBOUND |
					       I915_SHRINK_VMAPS);

	 
	mutex_lock(&to_gt(i915)->ggtt->vm.mutex);
	list_for_each_entry_safe(vma, next,
				 &to_gt(i915)->ggtt->vm.bound_list, vm_link) {
		unsigned long count = i915_vma_size(vma) >> PAGE_SHIFT;
		struct drm_i915_gem_object *obj = vma->obj;

		if (!vma->iomap || i915_vma_is_active(vma))
			continue;

		if (!i915_gem_object_trylock(obj, NULL))
			continue;

		if (__i915_vma_unbind(vma) == 0)
			freed_pages += count;

		i915_gem_object_unlock(obj);
	}
	mutex_unlock(&to_gt(i915)->ggtt->vm.mutex);

	*(unsigned long *)ptr += freed_pages;
	return NOTIFY_DONE;
}

void i915_gem_driver_register__shrinker(struct drm_i915_private *i915)
{
	i915->mm.shrinker.scan_objects = i915_gem_shrinker_scan;
	i915->mm.shrinker.count_objects = i915_gem_shrinker_count;
	i915->mm.shrinker.seeks = DEFAULT_SEEKS;
	i915->mm.shrinker.batch = 4096;
	drm_WARN_ON(&i915->drm, register_shrinker(&i915->mm.shrinker,
						  "drm-i915_gem"));

	i915->mm.oom_notifier.notifier_call = i915_gem_shrinker_oom;
	drm_WARN_ON(&i915->drm, register_oom_notifier(&i915->mm.oom_notifier));

	i915->mm.vmap_notifier.notifier_call = i915_gem_shrinker_vmap;
	drm_WARN_ON(&i915->drm,
		    register_vmap_purge_notifier(&i915->mm.vmap_notifier));
}

void i915_gem_driver_unregister__shrinker(struct drm_i915_private *i915)
{
	drm_WARN_ON(&i915->drm,
		    unregister_vmap_purge_notifier(&i915->mm.vmap_notifier));
	drm_WARN_ON(&i915->drm,
		    unregister_oom_notifier(&i915->mm.oom_notifier));
	unregister_shrinker(&i915->mm.shrinker);
}

void i915_gem_shrinker_taints_mutex(struct drm_i915_private *i915,
				    struct mutex *mutex)
{
	if (!IS_ENABLED(CONFIG_LOCKDEP))
		return;

	fs_reclaim_acquire(GFP_KERNEL);

	mutex_acquire(&mutex->dep_map, 0, 0, _RET_IP_);
	mutex_release(&mutex->dep_map, _RET_IP_);

	fs_reclaim_release(GFP_KERNEL);
}

 
void i915_gem_object_make_unshrinkable(struct drm_i915_gem_object *obj)
{
	struct drm_i915_private *i915 = obj_to_i915(obj);
	unsigned long flags;

	 
	if (atomic_add_unless(&obj->mm.shrink_pin, 1, 0))
		return;

	spin_lock_irqsave(&i915->mm.obj_lock, flags);
	if (!atomic_fetch_inc(&obj->mm.shrink_pin) &&
	    !list_empty(&obj->mm.link)) {
		list_del_init(&obj->mm.link);
		i915->mm.shrink_count--;
		i915->mm.shrink_memory -= obj->base.size;
	}
	spin_unlock_irqrestore(&i915->mm.obj_lock, flags);
}

static void ___i915_gem_object_make_shrinkable(struct drm_i915_gem_object *obj,
					       struct list_head *head)
{
	struct drm_i915_private *i915 = obj_to_i915(obj);
	unsigned long flags;

	if (!i915_gem_object_is_shrinkable(obj))
		return;

	if (atomic_add_unless(&obj->mm.shrink_pin, -1, 1))
		return;

	spin_lock_irqsave(&i915->mm.obj_lock, flags);
	GEM_BUG_ON(!kref_read(&obj->base.refcount));
	if (atomic_dec_and_test(&obj->mm.shrink_pin)) {
		GEM_BUG_ON(!list_empty(&obj->mm.link));

		list_add_tail(&obj->mm.link, head);
		i915->mm.shrink_count++;
		i915->mm.shrink_memory += obj->base.size;

	}
	spin_unlock_irqrestore(&i915->mm.obj_lock, flags);
}

 
void __i915_gem_object_make_shrinkable(struct drm_i915_gem_object *obj)
{
	___i915_gem_object_make_shrinkable(obj,
					   &obj_to_i915(obj)->mm.shrink_list);
}

 
void __i915_gem_object_make_purgeable(struct drm_i915_gem_object *obj)
{
	___i915_gem_object_make_shrinkable(obj,
					   &obj_to_i915(obj)->mm.purge_list);
}

 
void i915_gem_object_make_shrinkable(struct drm_i915_gem_object *obj)
{
	GEM_BUG_ON(!i915_gem_object_has_pages(obj));
	__i915_gem_object_make_shrinkable(obj);
}

 
void i915_gem_object_make_purgeable(struct drm_i915_gem_object *obj)
{
	GEM_BUG_ON(!i915_gem_object_has_pages(obj));
	__i915_gem_object_make_purgeable(obj);
}
