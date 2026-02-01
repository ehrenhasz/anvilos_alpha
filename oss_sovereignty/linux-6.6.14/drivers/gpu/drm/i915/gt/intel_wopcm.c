
 

#include "intel_wopcm.h"
#include "i915_drv.h"

 

 
#define GEN11_WOPCM_SIZE		SZ_2M
#define GEN9_WOPCM_SIZE			SZ_1M
#define MAX_WOPCM_SIZE			SZ_8M
 
#define WOPCM_RESERVED_SIZE		SZ_16K

 
#define GUC_WOPCM_RESERVED		SZ_16K
 
#define GUC_WOPCM_STACK_RESERVED	SZ_8K

 
#define GUC_WOPCM_OFFSET_ALIGNMENT	(1UL << GUC_WOPCM_OFFSET_SHIFT)

 
#define BXT_WOPCM_RC6_CTX_RESERVED	(SZ_16K + SZ_8K)
 
#define ICL_WOPCM_HW_CTX_RESERVED	(SZ_32K + SZ_4K)

 
#define GEN9_GUC_FW_RESERVED	SZ_128K
#define GEN9_GUC_WOPCM_OFFSET	(GUC_WOPCM_RESERVED + GEN9_GUC_FW_RESERVED)

static inline struct intel_gt *wopcm_to_gt(struct intel_wopcm *wopcm)
{
	return container_of(wopcm, struct intel_gt, wopcm);
}

 
void intel_wopcm_init_early(struct intel_wopcm *wopcm)
{
	struct intel_gt *gt = wopcm_to_gt(wopcm);
	struct drm_i915_private *i915 = gt->i915;

	if (!HAS_GT_UC(i915))
		return;

	if (GRAPHICS_VER(i915) >= 11)
		wopcm->size = GEN11_WOPCM_SIZE;
	else
		wopcm->size = GEN9_WOPCM_SIZE;

	drm_dbg(&i915->drm, "WOPCM: %uK\n", wopcm->size / 1024);
}

static u32 context_reserved_size(struct drm_i915_private *i915)
{
	if (IS_GEN9_LP(i915))
		return BXT_WOPCM_RC6_CTX_RESERVED;
	else if (GRAPHICS_VER(i915) >= 11)
		return ICL_WOPCM_HW_CTX_RESERVED;
	else
		return 0;
}

static bool gen9_check_dword_gap(struct drm_i915_private *i915,
				 u32 guc_wopcm_base, u32 guc_wopcm_size)
{
	u32 offset;

	 
	offset = guc_wopcm_base + GEN9_GUC_WOPCM_OFFSET;
	if (offset > guc_wopcm_size ||
	    (guc_wopcm_size - offset) < sizeof(u32)) {
		drm_err(&i915->drm,
			"WOPCM: invalid GuC region size: %uK < %uK\n",
			guc_wopcm_size / SZ_1K,
			(u32)(offset + sizeof(u32)) / SZ_1K);
		return false;
	}

	return true;
}

static bool gen9_check_huc_fw_fits(struct drm_i915_private *i915,
				   u32 guc_wopcm_size, u32 huc_fw_size)
{
	 
	if (huc_fw_size > guc_wopcm_size - GUC_WOPCM_RESERVED) {
		drm_err(&i915->drm, "WOPCM: no space for %s: %uK < %uK\n",
			intel_uc_fw_type_repr(INTEL_UC_FW_TYPE_HUC),
			(guc_wopcm_size - GUC_WOPCM_RESERVED) / SZ_1K,
			huc_fw_size / 1024);
		return false;
	}

	return true;
}

static bool check_hw_restrictions(struct drm_i915_private *i915,
				  u32 guc_wopcm_base, u32 guc_wopcm_size,
				  u32 huc_fw_size)
{
	if (GRAPHICS_VER(i915) == 9 && !gen9_check_dword_gap(i915, guc_wopcm_base,
							     guc_wopcm_size))
		return false;

	if (GRAPHICS_VER(i915) == 9 &&
	    !gen9_check_huc_fw_fits(i915, guc_wopcm_size, huc_fw_size))
		return false;

	return true;
}

static bool __check_layout(struct intel_gt *gt, u32 wopcm_size,
			   u32 guc_wopcm_base, u32 guc_wopcm_size,
			   u32 guc_fw_size, u32 huc_fw_size)
{
	struct drm_i915_private *i915 = gt->i915;
	const u32 ctx_rsvd = context_reserved_size(i915);
	u32 size;

	size = wopcm_size - ctx_rsvd;
	if (unlikely(range_overflows(guc_wopcm_base, guc_wopcm_size, size))) {
		drm_err(&i915->drm,
			"WOPCM: invalid GuC region layout: %uK + %uK > %uK\n",
			guc_wopcm_base / SZ_1K, guc_wopcm_size / SZ_1K,
			size / SZ_1K);
		return false;
	}

	size = guc_fw_size + GUC_WOPCM_RESERVED + GUC_WOPCM_STACK_RESERVED;
	if (unlikely(guc_wopcm_size < size)) {
		drm_err(&i915->drm, "WOPCM: no space for %s: %uK < %uK\n",
			intel_uc_fw_type_repr(INTEL_UC_FW_TYPE_GUC),
			guc_wopcm_size / SZ_1K, size / SZ_1K);
		return false;
	}

	if (intel_uc_supports_huc(&gt->uc)) {
		size = huc_fw_size + WOPCM_RESERVED_SIZE;
		if (unlikely(guc_wopcm_base < size)) {
			drm_err(&i915->drm, "WOPCM: no space for %s: %uK < %uK\n",
				intel_uc_fw_type_repr(INTEL_UC_FW_TYPE_HUC),
				guc_wopcm_base / SZ_1K, size / SZ_1K);
			return false;
		}
	}

	return check_hw_restrictions(i915, guc_wopcm_base, guc_wopcm_size,
				     huc_fw_size);
}

static bool __wopcm_regs_locked(struct intel_uncore *uncore,
				u32 *guc_wopcm_base, u32 *guc_wopcm_size)
{
	u32 reg_base = intel_uncore_read(uncore, DMA_GUC_WOPCM_OFFSET);
	u32 reg_size = intel_uncore_read(uncore, GUC_WOPCM_SIZE);

	if (!(reg_size & GUC_WOPCM_SIZE_LOCKED) ||
	    !(reg_base & GUC_WOPCM_OFFSET_VALID))
		return false;

	*guc_wopcm_base = reg_base & GUC_WOPCM_OFFSET_MASK;
	*guc_wopcm_size = reg_size & GUC_WOPCM_SIZE_MASK;
	return true;
}

static bool __wopcm_regs_writable(struct intel_uncore *uncore)
{
	if (!HAS_GUC_DEPRIVILEGE(uncore->i915))
		return true;

	return intel_uncore_read(uncore, GUC_SHIM_CONTROL2) & GUC_IS_PRIVILEGED;
}

 
void intel_wopcm_init(struct intel_wopcm *wopcm)
{
	struct intel_gt *gt = wopcm_to_gt(wopcm);
	struct drm_i915_private *i915 = gt->i915;
	u32 guc_fw_size = intel_uc_fw_get_upload_size(&gt->uc.guc.fw);
	u32 huc_fw_size = intel_uc_fw_get_upload_size(&gt->uc.huc.fw);
	u32 ctx_rsvd = context_reserved_size(i915);
	u32 wopcm_size = wopcm->size;
	u32 guc_wopcm_base;
	u32 guc_wopcm_size;

	if (!guc_fw_size)
		return;

	GEM_BUG_ON(!wopcm_size);
	GEM_BUG_ON(wopcm->guc.base);
	GEM_BUG_ON(wopcm->guc.size);
	GEM_BUG_ON(guc_fw_size >= wopcm_size);
	GEM_BUG_ON(huc_fw_size >= wopcm_size);
	GEM_BUG_ON(ctx_rsvd + WOPCM_RESERVED_SIZE >= wopcm_size);

	if (i915_inject_probe_failure(i915))
		return;

	if (__wopcm_regs_locked(gt->uncore, &guc_wopcm_base, &guc_wopcm_size)) {
		drm_dbg(&i915->drm, "GuC WOPCM is already locked [%uK, %uK)\n",
			guc_wopcm_base / SZ_1K, guc_wopcm_size / SZ_1K);
		 
		if (!__wopcm_regs_writable(gt->uncore))
			wopcm_size = MAX_WOPCM_SIZE;

		goto check;
	}

	 
	if (unlikely(i915->media_gt)) {
		drm_err(&i915->drm, "Unlocked WOPCM regs with media GT\n");
		return;
	}

	 
	guc_wopcm_base = huc_fw_size + WOPCM_RESERVED_SIZE;
	guc_wopcm_base = ALIGN(guc_wopcm_base, GUC_WOPCM_OFFSET_ALIGNMENT);

	 
	guc_wopcm_base = min(guc_wopcm_base, wopcm_size - ctx_rsvd);

	 
	guc_wopcm_size = wopcm_size - ctx_rsvd - guc_wopcm_base;
	guc_wopcm_size &= GUC_WOPCM_SIZE_MASK;

	drm_dbg(&i915->drm, "Calculated GuC WOPCM [%uK, %uK)\n",
		guc_wopcm_base / SZ_1K, guc_wopcm_size / SZ_1K);

check:
	if (__check_layout(gt, wopcm_size, guc_wopcm_base, guc_wopcm_size,
			   guc_fw_size, huc_fw_size)) {
		wopcm->guc.base = guc_wopcm_base;
		wopcm->guc.size = guc_wopcm_size;
		GEM_BUG_ON(!wopcm->guc.base);
		GEM_BUG_ON(!wopcm->guc.size);
	}
}
