 
#include "amdgpu.h"
#include "soc15.h"
#include "soc15_hw_ip.h"
#include "soc15_common.h"
#include "vega20_inc.h"
#include "vega20_ppsmc.h"
#include "vega20_baco.h"
#include "vega20_smumgr.h"

#include "amdgpu_ras.h"

static const struct soc15_baco_cmd_entry clean_baco_tbl[] = {
	{CMD_WRITE, SOC15_REG_ENTRY(NBIF, 0, mmBIOS_SCRATCH_6), 0, 0, 0, 0},
	{CMD_WRITE, SOC15_REG_ENTRY(NBIF, 0, mmBIOS_SCRATCH_7), 0, 0, 0, 0},
};

int vega20_baco_get_capability(struct pp_hwmgr *hwmgr, bool *cap)
{
	struct amdgpu_device *adev = (struct amdgpu_device *)(hwmgr->adev);
	uint32_t reg;

	*cap = false;
	if (!phm_cap_enabled(hwmgr->platform_descriptor.platformCaps, PHM_PlatformCaps_BACO))
		return 0;

	if (((RREG32(0x17569) & 0x20000000) >> 29) == 0x1) {
		reg = RREG32_SOC15(NBIF, 0, mmRCC_BIF_STRAP0);

		if (reg & RCC_BIF_STRAP0__STRAP_PX_CAPABLE_MASK)
			*cap = true;
	}

	return 0;
}

int vega20_baco_get_state(struct pp_hwmgr *hwmgr, enum BACO_STATE *state)
{
	struct amdgpu_device *adev = (struct amdgpu_device *)(hwmgr->adev);
	uint32_t reg;

	reg = RREG32_SOC15(NBIF, 0, mmBACO_CNTL);

	if (reg & BACO_CNTL__BACO_MODE_MASK)
		 
		*state = BACO_STATE_IN;
	else
		*state = BACO_STATE_OUT;
	return 0;
}

int vega20_baco_set_state(struct pp_hwmgr *hwmgr, enum BACO_STATE state)
{
	struct amdgpu_device *adev = (struct amdgpu_device *)(hwmgr->adev);
	struct amdgpu_ras *ras = amdgpu_ras_get_context(adev);
	enum BACO_STATE cur_state;
	uint32_t data;

	vega20_baco_get_state(hwmgr, &cur_state);

	if (cur_state == state)
		 
		return 0;

	if (state == BACO_STATE_IN) {
		if (!ras || !adev->ras_enabled) {
			data = RREG32_SOC15(THM, 0, mmTHM_BACO_CNTL);
			data |= 0x80000000;
			WREG32_SOC15(THM, 0, mmTHM_BACO_CNTL, data);

			if (smum_send_msg_to_smc_with_parameter(hwmgr,
					PPSMC_MSG_EnterBaco, 0, NULL))
				return -EINVAL;
		} else {
			if (smum_send_msg_to_smc_with_parameter(hwmgr,
					PPSMC_MSG_EnterBaco, 1, NULL))
				return -EINVAL;
		}

	} else if (state == BACO_STATE_OUT) {
		if (smum_send_msg_to_smc(hwmgr, PPSMC_MSG_ExitBaco, NULL))
			return -EINVAL;
		if (!soc15_baco_program_registers(hwmgr, clean_baco_tbl,
						     ARRAY_SIZE(clean_baco_tbl)))
			return -EINVAL;
	}

	return 0;
}

int vega20_baco_apply_vdci_flush_workaround(struct pp_hwmgr *hwmgr)
{
	int ret = 0;

	ret = vega20_set_pptable_driver_address(hwmgr);
	if (ret)
		return ret;

	return smum_send_msg_to_smc(hwmgr, PPSMC_MSG_BacoWorkAroundFlushVDCI, NULL);
}
