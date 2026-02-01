 

#include "gem/i915_gem_pm.h"
#include "gem/i915_gem_ttm_pm.h"
#include "gt/intel_gt.h"
#include "gt/intel_gt_pm.h"
#include "gt/intel_gt_requests.h"

#include "i915_driver.h"
#include "i915_drv.h"

#if defined(CONFIG_X86)
#include <asm/smp.h>
#else
#define wbinvd_on_all_cpus() \
	pr_warn(DRIVER_NAME ": Missing cache flush in %s\n", __func__)
#endif

void i915_gem_suspend(struct drm_i915_private *i915)
{
	struct intel_gt *gt;
	unsigned int i;

	GEM_TRACE("%s\n", dev_name(i915->drm.dev));

	intel_wakeref_auto(&i915->runtime_pm.userfault_wakeref, 0);
	flush_workqueue(i915->wq);

	 
	for_each_gt(gt, i915, i)
		intel_gt_suspend_prepare(gt);

	i915_gem_drain_freed_objects(i915);
}

static int lmem_restore(struct drm_i915_private *i915, u32 flags)
{
	struct intel_memory_region *mr;
	int ret = 0, id;

	for_each_memory_region(mr, i915, id) {
		if (mr->type == INTEL_MEMORY_LOCAL) {
			ret = i915_ttm_restore_region(mr, flags);
			if (ret)
				break;
		}
	}

	return ret;
}

static int lmem_suspend(struct drm_i915_private *i915, u32 flags)
{
	struct intel_memory_region *mr;
	int ret = 0, id;

	for_each_memory_region(mr, i915, id) {
		if (mr->type == INTEL_MEMORY_LOCAL) {
			ret = i915_ttm_backup_region(mr, flags);
			if (ret)
				break;
		}
	}

	return ret;
}

static void lmem_recover(struct drm_i915_private *i915)
{
	struct intel_memory_region *mr;
	int id;

	for_each_memory_region(mr, i915, id)
		if (mr->type == INTEL_MEMORY_LOCAL)
			i915_ttm_recover_region(mr);
}

int i915_gem_backup_suspend(struct drm_i915_private *i915)
{
	int ret;

	 
	ret = lmem_suspend(i915, I915_TTM_BACKUP_ALLOW_GPU);
	if (ret)
		goto out_recover;

	i915_gem_suspend(i915);

	 
	ret = lmem_suspend(i915, I915_TTM_BACKUP_ALLOW_GPU |
			   I915_TTM_BACKUP_PINNED);
	if (ret)
		goto out_recover;

	 
	ret = lmem_suspend(i915, I915_TTM_BACKUP_PINNED);
	if (ret)
		goto out_recover;

	return 0;

out_recover:
	lmem_recover(i915);

	return ret;
}

void i915_gem_suspend_late(struct drm_i915_private *i915)
{
	struct drm_i915_gem_object *obj;
	struct list_head *phases[] = {
		&i915->mm.shrink_list,
		&i915->mm.purge_list,
		NULL
	}, **phase;
	struct intel_gt *gt;
	unsigned long flags;
	unsigned int i;
	bool flush = false;

	 

	for_each_gt(gt, i915, i)
		intel_gt_suspend_late(gt);

	spin_lock_irqsave(&i915->mm.obj_lock, flags);
	for (phase = phases; *phase; phase++) {
		list_for_each_entry(obj, *phase, mm.link) {
			if (!(obj->cache_coherent & I915_BO_CACHE_COHERENT_FOR_READ))
				flush |= (obj->read_domains & I915_GEM_DOMAIN_CPU) == 0;
			__start_cpu_write(obj);  
		}
	}
	spin_unlock_irqrestore(&i915->mm.obj_lock, flags);
	if (flush)
		wbinvd_on_all_cpus();
}

int i915_gem_freeze(struct drm_i915_private *i915)
{
	 
	i915_gem_shrink_all(i915);

	return 0;
}

int i915_gem_freeze_late(struct drm_i915_private *i915)
{
	struct drm_i915_gem_object *obj;
	intel_wakeref_t wakeref;

	 

	with_intel_runtime_pm(&i915->runtime_pm, wakeref)
		i915_gem_shrink(NULL, i915, -1UL, NULL, ~0);
	i915_gem_drain_freed_objects(i915);

	wbinvd_on_all_cpus();
	list_for_each_entry(obj, &i915->mm.shrink_list, mm.link)
		__start_cpu_write(obj);

	return 0;
}

void i915_gem_resume(struct drm_i915_private *i915)
{
	struct intel_gt *gt;
	int ret, i, j;

	GEM_TRACE("%s\n", dev_name(i915->drm.dev));

	ret = lmem_restore(i915, 0);
	GEM_WARN_ON(ret);

	 
	for_each_gt(gt, i915, i)
		if (intel_gt_resume(gt))
			goto err_wedged;

	ret = lmem_restore(i915, I915_TTM_BACKUP_ALLOW_GPU);
	GEM_WARN_ON(ret);

	return;

err_wedged:
	for_each_gt(gt, i915, j) {
		if (!intel_gt_is_wedged(gt)) {
			dev_err(i915->drm.dev,
				"Failed to re-initialize GPU[%u], declaring it wedged!\n",
				j);
			intel_gt_set_wedged(gt);
		}

		if (j == i)
			break;
	}
}
