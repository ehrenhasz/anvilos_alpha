 
#include "amdgpu.h"
#include "smu7_baco.h"
#include "tonga_baco.h"
#include "fiji_baco.h"
#include "polaris_baco.h"
#include "ci_baco.h"

#include "bif/bif_5_0_d.h"
#include "bif/bif_5_0_sh_mask.h"

#include "smu/smu_7_1_2_d.h"
#include "smu/smu_7_1_2_sh_mask.h"

int smu7_baco_get_capability(struct pp_hwmgr *hwmgr, bool *cap)
{
	struct amdgpu_device *adev = (struct amdgpu_device *)(hwmgr->adev);
	uint32_t reg;

	*cap = false;
	if (!phm_cap_enabled(hwmgr->platform_descriptor.platformCaps, PHM_PlatformCaps_BACO))
		return 0;

	reg = RREG32(mmCC_BIF_BX_FUSESTRAP0);

	if (reg & CC_BIF_BX_FUSESTRAP0__STRAP_BIF_PX_CAPABLE_MASK)
		*cap = true;

	return 0;
}

int smu7_baco_get_state(struct pp_hwmgr *hwmgr, enum BACO_STATE *state)
{
	struct amdgpu_device *adev = (struct amdgpu_device *)(hwmgr->adev);
	uint32_t reg;

	reg = RREG32(mmBACO_CNTL);

	if (reg & BACO_CNTL__BACO_MODE_MASK)
		 
		*state = BACO_STATE_IN;
	else
		*state = BACO_STATE_OUT;
	return 0;
}

int smu7_baco_set_state(struct pp_hwmgr *hwmgr, enum BACO_STATE state)
{
	struct amdgpu_device *adev = (struct amdgpu_device *)(hwmgr->adev);

	switch (adev->asic_type) {
	case CHIP_TOPAZ:
	case CHIP_TONGA:
		return tonga_baco_set_state(hwmgr, state);
	case CHIP_FIJI:
		return fiji_baco_set_state(hwmgr, state);
	case CHIP_POLARIS10:
	case CHIP_POLARIS11:
	case CHIP_POLARIS12:
	case CHIP_VEGAM:
		return polaris_baco_set_state(hwmgr, state);
#ifdef CONFIG_DRM_AMDGPU_CIK
	case CHIP_BONAIRE:
	case CHIP_HAWAII:
		return ci_baco_set_state(hwmgr, state);
#endif
	default:
		return -EINVAL;
	}
}
