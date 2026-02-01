
 

#include <linux/types.h>

#include "gt/intel_gt.h"
#include "intel_guc_reg.h"
#include "intel_huc.h"
#include "intel_huc_print.h"
#include "i915_drv.h"
#include "i915_reg.h"
#include "pxp/intel_pxp_cmd_interface_43.h"

#include <linux/device/bus.h>
#include <linux/mei_aux.h>

 

 

 
#define GSC_INIT_TIMEOUT_MS 10000
#define PXP_INIT_TIMEOUT_MS 5000

static int sw_fence_dummy_notify(struct i915_sw_fence *sf,
				 enum i915_sw_fence_notify state)
{
	return NOTIFY_DONE;
}

static void __delayed_huc_load_complete(struct intel_huc *huc)
{
	if (!i915_sw_fence_done(&huc->delayed_load.fence))
		i915_sw_fence_complete(&huc->delayed_load.fence);
}

static void delayed_huc_load_complete(struct intel_huc *huc)
{
	hrtimer_cancel(&huc->delayed_load.timer);
	__delayed_huc_load_complete(huc);
}

static void __gsc_init_error(struct intel_huc *huc)
{
	huc->delayed_load.status = INTEL_HUC_DELAYED_LOAD_ERROR;
	__delayed_huc_load_complete(huc);
}

static void gsc_init_error(struct intel_huc *huc)
{
	hrtimer_cancel(&huc->delayed_load.timer);
	__gsc_init_error(huc);
}

static void gsc_init_done(struct intel_huc *huc)
{
	hrtimer_cancel(&huc->delayed_load.timer);

	 
	huc->delayed_load.status = INTEL_HUC_WAITING_ON_PXP;
	if (!i915_sw_fence_done(&huc->delayed_load.fence))
		hrtimer_start(&huc->delayed_load.timer,
			      ms_to_ktime(PXP_INIT_TIMEOUT_MS),
			      HRTIMER_MODE_REL);
}

static enum hrtimer_restart huc_delayed_load_timer_callback(struct hrtimer *hrtimer)
{
	struct intel_huc *huc = container_of(hrtimer, struct intel_huc, delayed_load.timer);

	if (!intel_huc_is_authenticated(huc, INTEL_HUC_AUTH_BY_GSC)) {
		if (huc->delayed_load.status == INTEL_HUC_WAITING_ON_GSC)
			huc_notice(huc, "timed out waiting for MEI GSC\n");
		else if (huc->delayed_load.status == INTEL_HUC_WAITING_ON_PXP)
			huc_notice(huc, "timed out waiting for MEI PXP\n");
		else
			MISSING_CASE(huc->delayed_load.status);

		__gsc_init_error(huc);
	}

	return HRTIMER_NORESTART;
}

static void huc_delayed_load_start(struct intel_huc *huc)
{
	ktime_t delay;

	GEM_BUG_ON(intel_huc_is_authenticated(huc, INTEL_HUC_AUTH_BY_GSC));

	 
	switch (huc->delayed_load.status) {
	case INTEL_HUC_WAITING_ON_GSC:
		delay = ms_to_ktime(GSC_INIT_TIMEOUT_MS);
		break;
	case INTEL_HUC_WAITING_ON_PXP:
		delay = ms_to_ktime(PXP_INIT_TIMEOUT_MS);
		break;
	default:
		gsc_init_error(huc);
		return;
	}

	 
	GEM_BUG_ON(!i915_sw_fence_done(&huc->delayed_load.fence));
	i915_sw_fence_fini(&huc->delayed_load.fence);
	i915_sw_fence_reinit(&huc->delayed_load.fence);
	i915_sw_fence_await(&huc->delayed_load.fence);
	i915_sw_fence_commit(&huc->delayed_load.fence);

	hrtimer_start(&huc->delayed_load.timer, delay, HRTIMER_MODE_REL);
}

static int gsc_notifier(struct notifier_block *nb, unsigned long action, void *data)
{
	struct device *dev = data;
	struct intel_huc *huc = container_of(nb, struct intel_huc, delayed_load.nb);
	struct intel_gsc_intf *intf = &huc_to_gt(huc)->gsc.intf[0];

	if (!intf->adev || &intf->adev->aux_dev.dev != dev)
		return 0;

	switch (action) {
	case BUS_NOTIFY_BOUND_DRIVER:  
		gsc_init_done(huc);
		break;

	case BUS_NOTIFY_DRIVER_NOT_BOUND:  
	case BUS_NOTIFY_UNBIND_DRIVER:  
		huc_info(huc, "MEI driver not bound, disabling load\n");
		gsc_init_error(huc);
		break;
	}

	return 0;
}

void intel_huc_register_gsc_notifier(struct intel_huc *huc, const struct bus_type *bus)
{
	int ret;

	if (!intel_huc_is_loaded_by_gsc(huc))
		return;

	huc->delayed_load.nb.notifier_call = gsc_notifier;
	ret = bus_register_notifier(bus, &huc->delayed_load.nb);
	if (ret) {
		huc_err(huc, "failed to register GSC notifier %pe\n", ERR_PTR(ret));
		huc->delayed_load.nb.notifier_call = NULL;
		gsc_init_error(huc);
	}
}

void intel_huc_unregister_gsc_notifier(struct intel_huc *huc, const struct bus_type *bus)
{
	if (!huc->delayed_load.nb.notifier_call)
		return;

	delayed_huc_load_complete(huc);

	bus_unregister_notifier(bus, &huc->delayed_load.nb);
	huc->delayed_load.nb.notifier_call = NULL;
}

static void delayed_huc_load_init(struct intel_huc *huc)
{
	 
	i915_sw_fence_init(&huc->delayed_load.fence,
			   sw_fence_dummy_notify);
	i915_sw_fence_commit(&huc->delayed_load.fence);

	hrtimer_init(&huc->delayed_load.timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	huc->delayed_load.timer.function = huc_delayed_load_timer_callback;
}

static void delayed_huc_load_fini(struct intel_huc *huc)
{
	 
	delayed_huc_load_complete(huc);
	i915_sw_fence_fini(&huc->delayed_load.fence);
}

int intel_huc_sanitize(struct intel_huc *huc)
{
	delayed_huc_load_complete(huc);
	intel_uc_fw_sanitize(&huc->fw);
	return 0;
}

static bool vcs_supported(struct intel_gt *gt)
{
	intel_engine_mask_t mask = gt->info.engine_mask;

	 
	GEM_BUG_ON(!gt_is_root(gt) && !gt->info.engine_mask);

	if (gt_is_root(gt))
		mask = INTEL_INFO(gt->i915)->platform_engine_mask;
	else
		mask = gt->info.engine_mask;

	return __ENGINE_INSTANCES_MASK(mask, VCS0, I915_MAX_VCS);
}

void intel_huc_init_early(struct intel_huc *huc)
{
	struct drm_i915_private *i915 = huc_to_gt(huc)->i915;
	struct intel_gt *gt = huc_to_gt(huc);

	intel_uc_fw_init_early(&huc->fw, INTEL_UC_FW_TYPE_HUC, true);

	 
	delayed_huc_load_init(huc);

	if (!vcs_supported(gt)) {
		intel_uc_fw_change_status(&huc->fw, INTEL_UC_FIRMWARE_NOT_SUPPORTED);
		return;
	}

	if (GRAPHICS_VER(i915) >= 11) {
		huc->status[INTEL_HUC_AUTH_BY_GUC].reg = GEN11_HUC_KERNEL_LOAD_INFO;
		huc->status[INTEL_HUC_AUTH_BY_GUC].mask = HUC_LOAD_SUCCESSFUL;
		huc->status[INTEL_HUC_AUTH_BY_GUC].value = HUC_LOAD_SUCCESSFUL;
	} else {
		huc->status[INTEL_HUC_AUTH_BY_GUC].reg = HUC_STATUS2;
		huc->status[INTEL_HUC_AUTH_BY_GUC].mask = HUC_FW_VERIFIED;
		huc->status[INTEL_HUC_AUTH_BY_GUC].value = HUC_FW_VERIFIED;
	}

	if (IS_DG2(i915)) {
		huc->status[INTEL_HUC_AUTH_BY_GSC].reg = GEN11_HUC_KERNEL_LOAD_INFO;
		huc->status[INTEL_HUC_AUTH_BY_GSC].mask = HUC_LOAD_SUCCESSFUL;
		huc->status[INTEL_HUC_AUTH_BY_GSC].value = HUC_LOAD_SUCCESSFUL;
	} else {
		huc->status[INTEL_HUC_AUTH_BY_GSC].reg = HECI_FWSTS(MTL_GSC_HECI1_BASE, 5);
		huc->status[INTEL_HUC_AUTH_BY_GSC].mask = HECI1_FWSTS5_HUC_AUTH_DONE;
		huc->status[INTEL_HUC_AUTH_BY_GSC].value = HECI1_FWSTS5_HUC_AUTH_DONE;
	}
}

#define HUC_LOAD_MODE_STRING(x) (x ? "GSC" : "legacy")
static int check_huc_loading_mode(struct intel_huc *huc)
{
	struct intel_gt *gt = huc_to_gt(huc);
	bool gsc_enabled = huc->fw.has_gsc_headers;

	 
	if (HAS_GUC_DEPRIVILEGE(gt->i915))
		huc->loaded_via_gsc = intel_uncore_read(gt->uncore, GUC_SHIM_CONTROL2) &
				      GSC_LOADS_HUC;

	if (huc->loaded_via_gsc && !gsc_enabled) {
		huc_err(huc, "HW requires a GSC-enabled blob, but we found a legacy one\n");
		return -ENOEXEC;
	}

	 
	if (!huc->loaded_via_gsc && gsc_enabled && !huc->fw.dma_start_offset) {
		huc_err(huc, "HW in DMA mode, but we have an incompatible GSC-enabled blob\n");
		return -ENOEXEC;
	}

	 
	if (huc->loaded_via_gsc) {
		if (IS_DG2(gt->i915)) {
			if (!IS_ENABLED(CONFIG_INTEL_MEI_PXP) ||
			    !IS_ENABLED(CONFIG_INTEL_MEI_GSC)) {
				huc_info(huc, "can't load due to missing mei modules\n");
				return -EIO;
			}
		} else {
			if (!HAS_ENGINE(gt, GSC0)) {
				huc_info(huc, "can't load due to missing GSCCS\n");
				return -EIO;
			}
		}
	}

	huc_dbg(huc, "loaded by GSC = %s\n", str_yes_no(huc->loaded_via_gsc));

	return 0;
}

int intel_huc_init(struct intel_huc *huc)
{
	struct intel_gt *gt = huc_to_gt(huc);
	int err;

	err = check_huc_loading_mode(huc);
	if (err)
		goto out;

	if (HAS_ENGINE(gt, GSC0)) {
		struct i915_vma *vma;

		vma = intel_guc_allocate_vma(&gt->uc.guc, PXP43_HUC_AUTH_INOUT_SIZE * 2);
		if (IS_ERR(vma)) {
			err = PTR_ERR(vma);
			huc_info(huc, "Failed to allocate heci pkt\n");
			goto out;
		}

		huc->heci_pkt = vma;
	}

	err = intel_uc_fw_init(&huc->fw);
	if (err)
		goto out_pkt;

	intel_uc_fw_change_status(&huc->fw, INTEL_UC_FIRMWARE_LOADABLE);

	return 0;

out_pkt:
	if (huc->heci_pkt)
		i915_vma_unpin_and_release(&huc->heci_pkt, 0);
out:
	intel_uc_fw_change_status(&huc->fw, INTEL_UC_FIRMWARE_INIT_FAIL);
	huc_info(huc, "initialization failed %pe\n", ERR_PTR(err));
	return err;
}

void intel_huc_fini(struct intel_huc *huc)
{
	 
	delayed_huc_load_fini(huc);

	if (huc->heci_pkt)
		i915_vma_unpin_and_release(&huc->heci_pkt, 0);

	if (intel_uc_fw_is_loadable(&huc->fw))
		intel_uc_fw_fini(&huc->fw);
}

void intel_huc_suspend(struct intel_huc *huc)
{
	if (!intel_uc_fw_is_loadable(&huc->fw))
		return;

	 
	delayed_huc_load_complete(huc);
}

static const char *auth_mode_string(struct intel_huc *huc,
				    enum intel_huc_authentication_type type)
{
	bool partial = huc->fw.has_gsc_headers && type == INTEL_HUC_AUTH_BY_GUC;

	return partial ? "clear media" : "all workloads";
}

int intel_huc_wait_for_auth_complete(struct intel_huc *huc,
				     enum intel_huc_authentication_type type)
{
	struct intel_gt *gt = huc_to_gt(huc);
	int ret;

	ret = __intel_wait_for_register(gt->uncore,
					huc->status[type].reg,
					huc->status[type].mask,
					huc->status[type].value,
					2, 50, NULL);

	 
	delayed_huc_load_complete(huc);

	if (ret) {
		huc_err(huc, "firmware not verified for %s: %pe\n",
			auth_mode_string(huc, type), ERR_PTR(ret));
		intel_uc_fw_change_status(&huc->fw, INTEL_UC_FIRMWARE_LOAD_FAIL);
		return ret;
	}

	intel_uc_fw_change_status(&huc->fw, INTEL_UC_FIRMWARE_RUNNING);
	huc_info(huc, "authenticated for %s\n", auth_mode_string(huc, type));
	return 0;
}

 
int intel_huc_auth(struct intel_huc *huc, enum intel_huc_authentication_type type)
{
	struct intel_gt *gt = huc_to_gt(huc);
	struct intel_guc *guc = &gt->uc.guc;
	int ret;

	if (!intel_uc_fw_is_loaded(&huc->fw))
		return -ENOEXEC;

	 
	if (intel_huc_is_loaded_by_gsc(huc))
		return -ENODEV;

	if (intel_huc_is_authenticated(huc, type))
		return -EEXIST;

	ret = i915_inject_probe_error(gt->i915, -ENXIO);
	if (ret)
		goto fail;

	switch (type) {
	case INTEL_HUC_AUTH_BY_GUC:
		ret = intel_guc_auth_huc(guc, intel_guc_ggtt_offset(guc, huc->fw.rsa_data));
		break;
	case INTEL_HUC_AUTH_BY_GSC:
		ret = intel_huc_fw_auth_via_gsccs(huc);
		break;
	default:
		MISSING_CASE(type);
		ret = -EINVAL;
	}
	if (ret)
		goto fail;

	 
	ret = intel_huc_wait_for_auth_complete(huc, type);
	if (ret)
		goto fail;

	return 0;

fail:
	huc_probe_error(huc, "%s authentication failed %pe\n",
			auth_mode_string(huc, type), ERR_PTR(ret));
	return ret;
}

bool intel_huc_is_authenticated(struct intel_huc *huc,
				enum intel_huc_authentication_type type)
{
	struct intel_gt *gt = huc_to_gt(huc);
	intel_wakeref_t wakeref;
	u32 status = 0;

	with_intel_runtime_pm(gt->uncore->rpm, wakeref)
		status = intel_uncore_read(gt->uncore, huc->status[type].reg);

	return (status & huc->status[type].mask) == huc->status[type].value;
}

static bool huc_is_fully_authenticated(struct intel_huc *huc)
{
	struct intel_uc_fw *huc_fw = &huc->fw;

	if (!huc_fw->has_gsc_headers)
		return intel_huc_is_authenticated(huc, INTEL_HUC_AUTH_BY_GUC);
	else if (intel_huc_is_loaded_by_gsc(huc) || HAS_ENGINE(huc_to_gt(huc), GSC0))
		return intel_huc_is_authenticated(huc, INTEL_HUC_AUTH_BY_GSC);
	else
		return false;
}

 
int intel_huc_check_status(struct intel_huc *huc)
{
	struct intel_uc_fw *huc_fw = &huc->fw;

	switch (__intel_uc_fw_status(huc_fw)) {
	case INTEL_UC_FIRMWARE_NOT_SUPPORTED:
		return -ENODEV;
	case INTEL_UC_FIRMWARE_DISABLED:
		return -EOPNOTSUPP;
	case INTEL_UC_FIRMWARE_MISSING:
		return -ENOPKG;
	case INTEL_UC_FIRMWARE_ERROR:
		return -ENOEXEC;
	case INTEL_UC_FIRMWARE_INIT_FAIL:
		return -ENOMEM;
	case INTEL_UC_FIRMWARE_LOAD_FAIL:
		return -EIO;
	default:
		break;
	}

	 
	if (huc_is_fully_authenticated(huc))
		return 1;  
	else if (huc_fw->has_gsc_headers && !intel_huc_is_loaded_by_gsc(huc) &&
		 intel_huc_is_authenticated(huc, INTEL_HUC_AUTH_BY_GUC))
		return 2;  
	else
		return 0;
}

static bool huc_has_delayed_load(struct intel_huc *huc)
{
	return intel_huc_is_loaded_by_gsc(huc) &&
	       (huc->delayed_load.status != INTEL_HUC_DELAYED_LOAD_ERROR);
}

void intel_huc_update_auth_status(struct intel_huc *huc)
{
	if (!intel_uc_fw_is_loadable(&huc->fw))
		return;

	if (!huc->fw.has_gsc_headers)
		return;

	if (huc_is_fully_authenticated(huc))
		intel_uc_fw_change_status(&huc->fw,
					  INTEL_UC_FIRMWARE_RUNNING);
	else if (huc_has_delayed_load(huc))
		huc_delayed_load_start(huc);
}

 
void intel_huc_load_status(struct intel_huc *huc, struct drm_printer *p)
{
	struct intel_gt *gt = huc_to_gt(huc);
	intel_wakeref_t wakeref;

	if (!intel_huc_is_supported(huc)) {
		drm_printf(p, "HuC not supported\n");
		return;
	}

	if (!intel_huc_is_wanted(huc)) {
		drm_printf(p, "HuC disabled\n");
		return;
	}

	intel_uc_fw_dump(&huc->fw, p);

	with_intel_runtime_pm(gt->uncore->rpm, wakeref)
		drm_printf(p, "HuC status: 0x%08x\n",
			   intel_uncore_read(gt->uncore, huc->status[INTEL_HUC_AUTH_BY_GUC].reg));
}
