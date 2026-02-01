 

#include "amdgpu.h"
#include "athub_v3_0.h"
#include "athub/athub_3_0_0_offset.h"
#include "athub/athub_3_0_0_sh_mask.h"
#include "navi10_enum.h"
#include "soc15_common.h"

#define regATHUB_MISC_CNTL_V3_0_1			0x00d7
#define regATHUB_MISC_CNTL_V3_0_1_BASE_IDX		0


static uint32_t athub_v3_0_get_cg_cntl(struct amdgpu_device *adev)
{
	uint32_t data;

	switch (adev->ip_versions[ATHUB_HWIP][0]) {
	case IP_VERSION(3, 0, 1):
		data = RREG32_SOC15(ATHUB, 0, regATHUB_MISC_CNTL_V3_0_1);
		break;
	default:
		data = RREG32_SOC15(ATHUB, 0, regATHUB_MISC_CNTL);
		break;
	}
	return data;
}

static void athub_v3_0_set_cg_cntl(struct amdgpu_device *adev, uint32_t data)
{
	switch (adev->ip_versions[ATHUB_HWIP][0]) {
	case IP_VERSION(3, 0, 1):
		WREG32_SOC15(ATHUB, 0, regATHUB_MISC_CNTL_V3_0_1, data);
		break;
	default:
		WREG32_SOC15(ATHUB, 0, regATHUB_MISC_CNTL, data);
		break;
	}
}

static void
athub_v3_0_update_medium_grain_clock_gating(struct amdgpu_device *adev,
					    bool enable)
{
	uint32_t def, data;

	def = data = athub_v3_0_get_cg_cntl(adev);

	if (enable && (adev->cg_flags & AMD_CG_SUPPORT_ATHUB_MGCG))
		data |= ATHUB_MISC_CNTL__CG_ENABLE_MASK;
	else
		data &= ~ATHUB_MISC_CNTL__CG_ENABLE_MASK;

	if (def != data)
		athub_v3_0_set_cg_cntl(adev, data);
}

static void
athub_v3_0_update_medium_grain_light_sleep(struct amdgpu_device *adev,
					   bool enable)
{
	uint32_t def, data;

	def = data = athub_v3_0_get_cg_cntl(adev);

	if (enable && (adev->cg_flags & AMD_CG_SUPPORT_ATHUB_LS))
		data |= ATHUB_MISC_CNTL__CG_MEM_LS_ENABLE_MASK;
	else
		data &= ~ATHUB_MISC_CNTL__CG_MEM_LS_ENABLE_MASK;

	if (def != data)
		athub_v3_0_set_cg_cntl(adev, data);
}

int athub_v3_0_set_clockgating(struct amdgpu_device *adev,
			       enum amd_clockgating_state state)
{
	if (amdgpu_sriov_vf(adev))
		return 0;

	switch (adev->ip_versions[ATHUB_HWIP][0]) {
	case IP_VERSION(3, 0, 0):
	case IP_VERSION(3, 0, 1):
	case IP_VERSION(3, 0, 2):
		athub_v3_0_update_medium_grain_clock_gating(adev,
				state == AMD_CG_STATE_GATE);
		athub_v3_0_update_medium_grain_light_sleep(adev,
				state == AMD_CG_STATE_GATE);
		break;
	default:
		break;
	}

	return 0;
}

void athub_v3_0_get_clockgating(struct amdgpu_device *adev, u64 *flags)
{
	int data;

	 
	data = athub_v3_0_get_cg_cntl(adev);
	if (data & ATHUB_MISC_CNTL__CG_ENABLE_MASK)
		*flags |= AMD_CG_SUPPORT_ATHUB_MGCG;

	 
	if (data & ATHUB_MISC_CNTL__CG_MEM_LS_ENABLE_MASK)
		*flags |= AMD_CG_SUPPORT_ATHUB_LS;
}
