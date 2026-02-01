 

#include "amdgpu.h"
#include "athub_v2_0.h"

#include "athub/athub_2_0_0_offset.h"
#include "athub/athub_2_0_0_sh_mask.h"
#include "athub/athub_2_0_0_default.h"

#include "soc15_common.h"

static void
athub_v2_0_update_medium_grain_clock_gating(struct amdgpu_device *adev,
					    bool enable)
{
	uint32_t def, data;

	if (!(adev->cg_flags & AMD_CG_SUPPORT_MC_MGCG))
		return;

	def = data = RREG32_SOC15(ATHUB, 0, mmATHUB_MISC_CNTL);

	if (enable)
		data |= ATHUB_MISC_CNTL__CG_ENABLE_MASK;
	else
		data &= ~ATHUB_MISC_CNTL__CG_ENABLE_MASK;

	if (def != data)
		WREG32_SOC15(ATHUB, 0, mmATHUB_MISC_CNTL, data);
}

static void
athub_v2_0_update_medium_grain_light_sleep(struct amdgpu_device *adev,
					   bool enable)
{
	uint32_t def, data;

	if (!((adev->cg_flags & AMD_CG_SUPPORT_MC_LS) &&
	       (adev->cg_flags & AMD_CG_SUPPORT_HDP_LS)))
		return;

	def = data = RREG32_SOC15(ATHUB, 0, mmATHUB_MISC_CNTL);

	if (enable)
		data |= ATHUB_MISC_CNTL__CG_MEM_LS_ENABLE_MASK;
	else
		data &= ~ATHUB_MISC_CNTL__CG_MEM_LS_ENABLE_MASK;

	if (def != data)
		WREG32_SOC15(ATHUB, 0, mmATHUB_MISC_CNTL, data);
}

int athub_v2_0_set_clockgating(struct amdgpu_device *adev,
			       enum amd_clockgating_state state)
{
	if (amdgpu_sriov_vf(adev))
		return 0;

	switch (adev->ip_versions[ATHUB_HWIP][0]) {
	case IP_VERSION(1, 3, 1):
	case IP_VERSION(2, 0, 0):
	case IP_VERSION(2, 0, 2):
		athub_v2_0_update_medium_grain_clock_gating(adev,
				state == AMD_CG_STATE_GATE);
		athub_v2_0_update_medium_grain_light_sleep(adev,
				state == AMD_CG_STATE_GATE);
		break;
	default:
		break;
	}

	return 0;
}

void athub_v2_0_get_clockgating(struct amdgpu_device *adev, u64 *flags)
{
	int data;

	 
	data = RREG32_SOC15(ATHUB, 0, mmATHUB_MISC_CNTL);
	if (data & ATHUB_MISC_CNTL__CG_ENABLE_MASK)
		*flags |= AMD_CG_SUPPORT_ATHUB_MGCG;

	 
	if (data & ATHUB_MISC_CNTL__CG_MEM_LS_ENABLE_MASK)
		*flags |= AMD_CG_SUPPORT_ATHUB_LS;
}
