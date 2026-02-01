
 

#include <linux/types.h>

#include "gt/intel_gt.h"
#include "gt/intel_gt_print.h"
#include "intel_gsc_fw.h"
#include "intel_gsc_proxy.h"
#include "intel_gsc_uc.h"
#include "i915_drv.h"
#include "i915_reg.h"

static void gsc_work(struct work_struct *work)
{
	struct intel_gsc_uc *gsc = container_of(work, typeof(*gsc), work);
	struct intel_gt *gt = gsc_uc_to_gt(gsc);
	intel_wakeref_t wakeref;
	u32 actions;
	int ret;

	wakeref = intel_runtime_pm_get(gt->uncore->rpm);

	spin_lock_irq(gt->irq_lock);
	actions = gsc->gsc_work_actions;
	gsc->gsc_work_actions = 0;
	spin_unlock_irq(gt->irq_lock);

	if (actions & GSC_ACTION_FW_LOAD) {
		ret = intel_gsc_uc_fw_upload(gsc);
		if (!ret)
			 
			actions |= GSC_ACTION_SW_PROXY;
		else if (ret != -EEXIST)
			goto out_put;

		 
		if (intel_uc_uses_huc(&gt->uc) &&
		    intel_huc_is_authenticated(&gt->uc.huc, INTEL_HUC_AUTH_BY_GUC))
			intel_huc_auth(&gt->uc.huc, INTEL_HUC_AUTH_BY_GSC);
	}

	if (actions & GSC_ACTION_SW_PROXY) {
		if (!intel_gsc_uc_fw_init_done(gsc)) {
			gt_err(gt, "Proxy request received with GSC not loaded!\n");
			goto out_put;
		}

		ret = intel_gsc_proxy_request_handler(gsc);
		if (ret) {
			if (actions & GSC_ACTION_FW_LOAD) {
				 
				drm_err(&gt->i915->drm,
					"GSC proxy handler failed to init\n");
				intel_uc_fw_change_status(&gsc->fw, INTEL_UC_FIRMWARE_LOAD_FAIL);
			}
			goto out_put;
		}

		 
		if (actions & GSC_ACTION_FW_LOAD) {
			 
			if (intel_gsc_uc_fw_proxy_init_done(gsc, false)) {
				drm_dbg(&gt->i915->drm, "GSC Proxy initialized\n");
				intel_uc_fw_change_status(&gsc->fw, INTEL_UC_FIRMWARE_RUNNING);
			} else {
				drm_err(&gt->i915->drm,
					"GSC status reports proxy init not complete\n");
				intel_uc_fw_change_status(&gsc->fw, INTEL_UC_FIRMWARE_LOAD_FAIL);
			}
		}
	}

out_put:
	intel_runtime_pm_put(gt->uncore->rpm, wakeref);
}

static bool gsc_engine_supported(struct intel_gt *gt)
{
	intel_engine_mask_t mask;

	 
	GEM_BUG_ON(!gt_is_root(gt) && !gt->info.engine_mask);

	if (gt_is_root(gt))
		mask = INTEL_INFO(gt->i915)->platform_engine_mask;
	else
		mask = gt->info.engine_mask;

	return __HAS_ENGINE(mask, GSC0);
}

void intel_gsc_uc_init_early(struct intel_gsc_uc *gsc)
{
	struct intel_gt *gt = gsc_uc_to_gt(gsc);

	 
	intel_uc_fw_init_early(&gsc->fw, INTEL_UC_FW_TYPE_GSC, false);
	INIT_WORK(&gsc->work, gsc_work);

	 
	if (!gsc_engine_supported(gt)) {
		intel_uc_fw_change_status(&gsc->fw, INTEL_UC_FIRMWARE_NOT_SUPPORTED);
		return;
	}

	gsc->wq = alloc_ordered_workqueue("i915_gsc", 0);
	if (!gsc->wq) {
		gt_err(gt, "failed to allocate WQ for GSC, disabling FW\n");
		intel_uc_fw_change_status(&gsc->fw, INTEL_UC_FIRMWARE_NOT_SUPPORTED);
	}
}

static int gsc_allocate_and_map_vma(struct intel_gsc_uc *gsc, u32 size)
{
	struct intel_gt *gt = gsc_uc_to_gt(gsc);
	struct drm_i915_gem_object *obj;
	struct i915_vma *vma;
	void __iomem *vaddr;
	int ret = 0;

	 
	obj = i915_gem_object_create_stolen(gt->i915, size);
	if (IS_ERR(obj))
		return PTR_ERR(obj);

	vma = i915_gem_object_ggtt_pin(obj, NULL, 0, 0, 0);
	if (IS_ERR(vma)) {
		ret = PTR_ERR(vma);
		goto err;
	}

	vaddr = i915_vma_pin_iomap(vma);
	i915_vma_unpin(vma);
	if (IS_ERR(vaddr)) {
		ret = PTR_ERR(vaddr);
		goto err;
	}

	i915_vma_make_unshrinkable(vma);

	gsc->local = vma;
	gsc->local_vaddr = vaddr;

	return 0;

err:
	i915_gem_object_put(obj);
	return ret;
}

static void gsc_unmap_and_free_vma(struct intel_gsc_uc *gsc)
{
	struct i915_vma *vma = fetch_and_zero(&gsc->local);

	if (!vma)
		return;

	gsc->local_vaddr = NULL;
	i915_vma_unpin_iomap(vma);
	i915_gem_object_put(vma->obj);
}

int intel_gsc_uc_init(struct intel_gsc_uc *gsc)
{
	static struct lock_class_key gsc_lock;
	struct intel_gt *gt = gsc_uc_to_gt(gsc);
	struct intel_engine_cs *engine = gt->engine[GSC0];
	struct intel_context *ce;
	int err;

	err = intel_uc_fw_init(&gsc->fw);
	if (err)
		goto out;

	err = gsc_allocate_and_map_vma(gsc, SZ_4M);
	if (err)
		goto out_fw;

	ce = intel_engine_create_pinned_context(engine, engine->gt->vm, SZ_4K,
						I915_GEM_HWS_GSC_ADDR,
						&gsc_lock, "gsc_context");
	if (IS_ERR(ce)) {
		gt_err(gt, "failed to create GSC CS ctx for FW communication\n");
		err =  PTR_ERR(ce);
		goto out_vma;
	}

	gsc->ce = ce;

	 
	intel_gsc_proxy_init(gsc);

	intel_uc_fw_change_status(&gsc->fw, INTEL_UC_FIRMWARE_LOADABLE);

	return 0;

out_vma:
	gsc_unmap_and_free_vma(gsc);
out_fw:
	intel_uc_fw_fini(&gsc->fw);
out:
	gt_probe_error(gt, "GSC init failed %pe\n", ERR_PTR(err));
	return err;
}

void intel_gsc_uc_fini(struct intel_gsc_uc *gsc)
{
	if (!intel_uc_fw_is_loadable(&gsc->fw))
		return;

	flush_work(&gsc->work);
	if (gsc->wq) {
		destroy_workqueue(gsc->wq);
		gsc->wq = NULL;
	}

	intel_gsc_proxy_fini(gsc);

	if (gsc->ce)
		intel_engine_destroy_pinned_context(fetch_and_zero(&gsc->ce));

	gsc_unmap_and_free_vma(gsc);

	intel_uc_fw_fini(&gsc->fw);
}

void intel_gsc_uc_flush_work(struct intel_gsc_uc *gsc)
{
	if (!intel_uc_fw_is_loadable(&gsc->fw))
		return;

	flush_work(&gsc->work);
}

void intel_gsc_uc_resume(struct intel_gsc_uc *gsc)
{
	if (!intel_uc_fw_is_loadable(&gsc->fw))
		return;

	 
	if (!gsc_uc_to_gt(gsc)->engine[GSC0]->default_state)
		return;

	intel_gsc_uc_load_start(gsc);
}

void intel_gsc_uc_load_start(struct intel_gsc_uc *gsc)
{
	struct intel_gt *gt = gsc_uc_to_gt(gsc);

	if (!intel_uc_fw_is_loadable(&gsc->fw))
		return;

	if (intel_gsc_uc_fw_init_done(gsc))
		return;

	spin_lock_irq(gt->irq_lock);
	gsc->gsc_work_actions |= GSC_ACTION_FW_LOAD;
	spin_unlock_irq(gt->irq_lock);

	queue_work(gsc->wq, &gsc->work);
}

void intel_gsc_uc_load_status(struct intel_gsc_uc *gsc, struct drm_printer *p)
{
	struct intel_gt *gt = gsc_uc_to_gt(gsc);
	struct intel_uncore *uncore = gt->uncore;
	intel_wakeref_t wakeref;

	if (!intel_gsc_uc_is_supported(gsc)) {
		drm_printf(p, "GSC not supported\n");
		return;
	}

	if (!intel_gsc_uc_is_wanted(gsc)) {
		drm_printf(p, "GSC disabled\n");
		return;
	}

	drm_printf(p, "GSC firmware: %s\n", gsc->fw.file_selected.path);
	if (gsc->fw.file_selected.path != gsc->fw.file_wanted.path)
		drm_printf(p, "GSC firmware wanted: %s\n", gsc->fw.file_wanted.path);
	drm_printf(p, "\tstatus: %s\n", intel_uc_fw_status_repr(gsc->fw.status));

	drm_printf(p, "Release: %u.%u.%u.%u\n",
		   gsc->release.major, gsc->release.minor,
		   gsc->release.patch, gsc->release.build);

	drm_printf(p, "Compatibility Version: %u.%u [min expected %u.%u]\n",
		   gsc->fw.file_selected.ver.major, gsc->fw.file_selected.ver.minor,
		   gsc->fw.file_wanted.ver.major, gsc->fw.file_wanted.ver.minor);

	drm_printf(p, "SVN: %u\n", gsc->security_version);

	with_intel_runtime_pm(uncore->rpm, wakeref) {
		u32 i;

		for (i = 1; i <= 6; i++) {
			u32 status = intel_uncore_read(uncore,
						       HECI_FWSTS(MTL_GSC_HECI1_BASE, i));
			drm_printf(p, "HECI1 FWSTST%u = 0x%08x\n", i, status);
		}
	}
}
