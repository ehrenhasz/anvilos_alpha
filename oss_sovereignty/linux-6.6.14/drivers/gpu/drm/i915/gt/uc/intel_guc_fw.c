
 

#include "gt/intel_gt.h"
#include "gt/intel_gt_mcr.h"
#include "gt/intel_gt_regs.h"
#include "gt/intel_rps.h"
#include "intel_guc_fw.h"
#include "intel_guc_print.h"
#include "i915_drv.h"

static void guc_prepare_xfer(struct intel_gt *gt)
{
	struct intel_uncore *uncore = gt->uncore;

	u32 shim_flags = GUC_ENABLE_READ_CACHE_LOGIC |
			 GUC_ENABLE_READ_CACHE_FOR_SRAM_DATA |
			 GUC_ENABLE_READ_CACHE_FOR_WOPCM_DATA |
			 GUC_ENABLE_MIA_CLOCK_GATING;

	if (GRAPHICS_VER_FULL(uncore->i915) < IP_VER(12, 50))
		shim_flags |= GUC_DISABLE_SRAM_INIT_TO_ZEROES |
			      GUC_ENABLE_MIA_CACHING;

	 
	intel_uncore_write(uncore, GUC_SHIM_CONTROL, shim_flags);

	if (IS_GEN9_LP(uncore->i915))
		intel_uncore_write(uncore, GEN9LP_GT_PM_CONFIG, GT_DOORBELL_ENABLE);
	else
		intel_uncore_write(uncore, GEN9_GT_PM_CONFIG, GT_DOORBELL_ENABLE);

	if (GRAPHICS_VER(uncore->i915) == 9) {
		 
		intel_uncore_rmw(uncore, GEN7_MISCCPCTL, 0,
				 GEN8_DOP_CLOCK_GATE_GUC_ENABLE);

		 
		intel_uncore_write(uncore, GUC_ARAT_C6DIS, 0x1FF);
	}
}

static int guc_xfer_rsa_mmio(struct intel_uc_fw *guc_fw,
			     struct intel_uncore *uncore)
{
	u32 rsa[UOS_RSA_SCRATCH_COUNT];
	size_t copied;
	int i;

	copied = intel_uc_fw_copy_rsa(guc_fw, rsa, sizeof(rsa));
	if (copied < sizeof(rsa))
		return -ENOMEM;

	for (i = 0; i < UOS_RSA_SCRATCH_COUNT; i++)
		intel_uncore_write(uncore, UOS_RSA_SCRATCH(i), rsa[i]);

	return 0;
}

static int guc_xfer_rsa_vma(struct intel_uc_fw *guc_fw,
			    struct intel_uncore *uncore)
{
	struct intel_guc *guc = container_of(guc_fw, struct intel_guc, fw);

	intel_uncore_write(uncore, UOS_RSA_SCRATCH(0),
			   intel_guc_ggtt_offset(guc, guc_fw->rsa_data));

	return 0;
}

 
static int guc_xfer_rsa(struct intel_uc_fw *guc_fw,
			struct intel_uncore *uncore)
{
	if (guc_fw->rsa_data)
		return guc_xfer_rsa_vma(guc_fw, uncore);
	else
		return guc_xfer_rsa_mmio(guc_fw, uncore);
}

 
static inline bool guc_load_done(struct intel_uncore *uncore, u32 *status, bool *success)
{
	u32 val = intel_uncore_read(uncore, GUC_STATUS);
	u32 uk_val = REG_FIELD_GET(GS_UKERNEL_MASK, val);
	u32 br_val = REG_FIELD_GET(GS_BOOTROM_MASK, val);

	*status = val;
	switch (uk_val) {
	case INTEL_GUC_LOAD_STATUS_READY:
		*success = true;
		return true;

	case INTEL_GUC_LOAD_STATUS_ERROR_DEVID_BUILD_MISMATCH:
	case INTEL_GUC_LOAD_STATUS_GUC_PREPROD_BUILD_MISMATCH:
	case INTEL_GUC_LOAD_STATUS_ERROR_DEVID_INVALID_GUCTYPE:
	case INTEL_GUC_LOAD_STATUS_HWCONFIG_ERROR:
	case INTEL_GUC_LOAD_STATUS_DPC_ERROR:
	case INTEL_GUC_LOAD_STATUS_EXCEPTION:
	case INTEL_GUC_LOAD_STATUS_INIT_DATA_INVALID:
	case INTEL_GUC_LOAD_STATUS_MPU_DATA_INVALID:
	case INTEL_GUC_LOAD_STATUS_INIT_MMIO_SAVE_RESTORE_INVALID:
		*success = false;
		return true;
	}

	switch (br_val) {
	case INTEL_BOOTROM_STATUS_NO_KEY_FOUND:
	case INTEL_BOOTROM_STATUS_RSA_FAILED:
	case INTEL_BOOTROM_STATUS_PAVPC_FAILED:
	case INTEL_BOOTROM_STATUS_WOPCM_FAILED:
	case INTEL_BOOTROM_STATUS_LOADLOC_FAILED:
	case INTEL_BOOTROM_STATUS_JUMP_FAILED:
	case INTEL_BOOTROM_STATUS_RC6CTXCONFIG_FAILED:
	case INTEL_BOOTROM_STATUS_MPUMAP_INCORRECT:
	case INTEL_BOOTROM_STATUS_EXCEPTION:
	case INTEL_BOOTROM_STATUS_PROD_KEY_CHECK_FAILURE:
		*success = false;
		return true;
	}

	return false;
}

 
#if defined(CONFIG_DRM_I915_DEBUG_GEM)
#define GUC_LOAD_RETRY_LIMIT	20
#else
#define GUC_LOAD_RETRY_LIMIT	3
#endif

static int guc_wait_ucode(struct intel_guc *guc)
{
	struct intel_gt *gt = guc_to_gt(guc);
	struct intel_uncore *uncore = gt->uncore;
	ktime_t before, after, delta;
	bool success;
	u32 status;
	int ret, count;
	u64 delta_ms;
	u32 before_freq;

	 
	before_freq = intel_rps_read_actual_frequency(&uncore->gt->rps);
	before = ktime_get();
	for (count = 0; count < GUC_LOAD_RETRY_LIMIT; count++) {
		ret = wait_for(guc_load_done(uncore, &status, &success), 1000);
		if (!ret || !success)
			break;

		guc_dbg(guc, "load still in progress, count = %d, freq = %dMHz, status = 0x%08X [0x%02X/%02X]\n",
			count, intel_rps_read_actual_frequency(&uncore->gt->rps), status,
			REG_FIELD_GET(GS_BOOTROM_MASK, status),
			REG_FIELD_GET(GS_UKERNEL_MASK, status));
	}
	after = ktime_get();
	delta = ktime_sub(after, before);
	delta_ms = ktime_to_ms(delta);
	if (ret || !success) {
		u32 ukernel = REG_FIELD_GET(GS_UKERNEL_MASK, status);
		u32 bootrom = REG_FIELD_GET(GS_BOOTROM_MASK, status);

		guc_info(guc, "load failed: status = 0x%08X, time = %lldms, freq = %dMHz, ret = %d\n",
			 status, delta_ms, intel_rps_read_actual_frequency(&uncore->gt->rps), ret);
		guc_info(guc, "load failed: status: Reset = %d, BootROM = 0x%02X, UKernel = 0x%02X, MIA = 0x%02X, Auth = 0x%02X\n",
			 REG_FIELD_GET(GS_MIA_IN_RESET, status),
			 bootrom, ukernel,
			 REG_FIELD_GET(GS_MIA_MASK, status),
			 REG_FIELD_GET(GS_AUTH_STATUS_MASK, status));

		switch (bootrom) {
		case INTEL_BOOTROM_STATUS_NO_KEY_FOUND:
			guc_info(guc, "invalid key requested, header = 0x%08X\n",
				 intel_uncore_read(uncore, GUC_HEADER_INFO));
			ret = -ENOEXEC;
			break;

		case INTEL_BOOTROM_STATUS_RSA_FAILED:
			guc_info(guc, "firmware signature verification failed\n");
			ret = -ENOEXEC;
			break;

		case INTEL_BOOTROM_STATUS_PROD_KEY_CHECK_FAILURE:
			guc_info(guc, "firmware production part check failure\n");
			ret = -ENOEXEC;
			break;
		}

		switch (ukernel) {
		case INTEL_GUC_LOAD_STATUS_EXCEPTION:
			guc_info(guc, "firmware exception. EIP: %#x\n",
				 intel_uncore_read(uncore, SOFT_SCRATCH(13)));
			ret = -ENXIO;
			break;

		case INTEL_GUC_LOAD_STATUS_INIT_MMIO_SAVE_RESTORE_INVALID:
			guc_info(guc, "illegal register in save/restore workaround list\n");
			ret = -EPERM;
			break;

		case INTEL_GUC_LOAD_STATUS_HWCONFIG_START:
			guc_info(guc, "still extracting hwconfig table.\n");
			ret = -ETIMEDOUT;
			break;
		}

		 
		if (ret == 0)
			ret = -ENXIO;
	} else if (delta_ms > 200) {
		guc_warn(guc, "excessive init time: %lldms! [status = 0x%08X, count = %d, ret = %d]\n",
			 delta_ms, status, count, ret);
		guc_warn(guc, "excessive init time: [freq = %dMHz, before = %dMHz, perf_limit_reasons = 0x%08X]\n",
			 intel_rps_read_actual_frequency(&uncore->gt->rps), before_freq,
			 intel_uncore_read(uncore, intel_gt_perf_limit_reasons_reg(gt)));
	} else {
		guc_dbg(guc, "init took %lldms, freq = %dMHz, before = %dMHz, status = 0x%08X, count = %d, ret = %d\n",
			delta_ms, intel_rps_read_actual_frequency(&uncore->gt->rps),
			before_freq, status, count, ret);
	}

	return ret;
}

 
int intel_guc_fw_upload(struct intel_guc *guc)
{
	struct intel_gt *gt = guc_to_gt(guc);
	struct intel_uncore *uncore = gt->uncore;
	int ret;

	guc_prepare_xfer(gt);

	 
	ret = guc_xfer_rsa(&guc->fw, uncore);
	if (ret)
		goto out;

	 
	ret = intel_uc_fw_upload(&guc->fw, 0x2000, UOS_MOVE);
	if (ret)
		goto out;

	ret = guc_wait_ucode(guc);
	if (ret)
		goto out;

	intel_uc_fw_change_status(&guc->fw, INTEL_UC_FIRMWARE_RUNNING);
	return 0;

out:
	intel_uc_fw_change_status(&guc->fw, INTEL_UC_FIRMWARE_LOAD_FAIL);
	return ret;
}
