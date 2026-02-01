 
#include "amdgpu.h"
#include "amdgpu_atombios.h"
#include "hdp_v6_0.h"

#include "hdp/hdp_6_0_0_offset.h"
#include "hdp/hdp_6_0_0_sh_mask.h"
#include <uapi/linux/kfd_ioctl.h>

static void hdp_v6_0_flush_hdp(struct amdgpu_device *adev,
				struct amdgpu_ring *ring)
{
	if (!ring || !ring->funcs->emit_wreg)
		WREG32_NO_KIQ((adev->rmmio_remap.reg_offset + KFD_MMIO_REMAP_HDP_MEM_FLUSH_CNTL) >> 2, 0);
	else
		amdgpu_ring_emit_wreg(ring, (adev->rmmio_remap.reg_offset + KFD_MMIO_REMAP_HDP_MEM_FLUSH_CNTL) >> 2, 0);
}

static void hdp_v6_0_update_clock_gating(struct amdgpu_device *adev,
					 bool enable)
{
	uint32_t hdp_clk_cntl, hdp_clk_cntl1;
	uint32_t hdp_mem_pwr_cntl;

	if (!(adev->cg_flags & (AMD_CG_SUPPORT_HDP_LS |
				AMD_CG_SUPPORT_HDP_DS |
				AMD_CG_SUPPORT_HDP_SD)))
		return;

	hdp_clk_cntl = hdp_clk_cntl1 = RREG32_SOC15(HDP, 0,regHDP_CLK_CNTL);
	hdp_mem_pwr_cntl = RREG32_SOC15(HDP, 0, regHDP_MEM_POWER_CTRL);

	 
	hdp_clk_cntl = REG_SET_FIELD(hdp_clk_cntl, HDP_CLK_CNTL,
				     RC_MEM_CLK_SOFT_OVERRIDE, 1);
	WREG32_SOC15(HDP, 0, regHDP_CLK_CNTL, hdp_clk_cntl);

	 
	hdp_mem_pwr_cntl = REG_SET_FIELD(hdp_mem_pwr_cntl, HDP_MEM_POWER_CTRL,
					 ATOMIC_MEM_POWER_CTRL_EN, 0);
	hdp_mem_pwr_cntl = REG_SET_FIELD(hdp_mem_pwr_cntl, HDP_MEM_POWER_CTRL,
					 ATOMIC_MEM_POWER_LS_EN, 0);
	hdp_mem_pwr_cntl = REG_SET_FIELD(hdp_mem_pwr_cntl, HDP_MEM_POWER_CTRL,
					 ATOMIC_MEM_POWER_DS_EN, 0);
	hdp_mem_pwr_cntl = REG_SET_FIELD(hdp_mem_pwr_cntl, HDP_MEM_POWER_CTRL,
					 ATOMIC_MEM_POWER_SD_EN, 0);
	hdp_mem_pwr_cntl = REG_SET_FIELD(hdp_mem_pwr_cntl, HDP_MEM_POWER_CTRL,
					 RC_MEM_POWER_CTRL_EN, 0);
	hdp_mem_pwr_cntl = REG_SET_FIELD(hdp_mem_pwr_cntl, HDP_MEM_POWER_CTRL,
					 RC_MEM_POWER_LS_EN, 0);
	hdp_mem_pwr_cntl = REG_SET_FIELD(hdp_mem_pwr_cntl, HDP_MEM_POWER_CTRL,
					 RC_MEM_POWER_DS_EN, 0);
	hdp_mem_pwr_cntl = REG_SET_FIELD(hdp_mem_pwr_cntl, HDP_MEM_POWER_CTRL,
					 RC_MEM_POWER_SD_EN, 0);
	WREG32_SOC15(HDP, 0, regHDP_MEM_POWER_CTRL, hdp_mem_pwr_cntl);

	 
	if (enable) {
		 
		if (adev->cg_flags & AMD_CG_SUPPORT_HDP_SD) {
			hdp_mem_pwr_cntl = REG_SET_FIELD(hdp_mem_pwr_cntl,
							 HDP_MEM_POWER_CTRL,
							 ATOMIC_MEM_POWER_SD_EN, 1);
			hdp_mem_pwr_cntl = REG_SET_FIELD(hdp_mem_pwr_cntl,
							 HDP_MEM_POWER_CTRL,
							 RC_MEM_POWER_SD_EN, 1);
		} else if (adev->cg_flags & AMD_CG_SUPPORT_HDP_LS) {
			hdp_mem_pwr_cntl = REG_SET_FIELD(hdp_mem_pwr_cntl,
							 HDP_MEM_POWER_CTRL,
							 ATOMIC_MEM_POWER_LS_EN, 1);
			hdp_mem_pwr_cntl = REG_SET_FIELD(hdp_mem_pwr_cntl,
							 HDP_MEM_POWER_CTRL,
							 RC_MEM_POWER_LS_EN, 1);
		} else if (adev->cg_flags & AMD_CG_SUPPORT_HDP_DS) {
			hdp_mem_pwr_cntl = REG_SET_FIELD(hdp_mem_pwr_cntl,
							 HDP_MEM_POWER_CTRL,
							 ATOMIC_MEM_POWER_DS_EN, 1);
			hdp_mem_pwr_cntl = REG_SET_FIELD(hdp_mem_pwr_cntl,
							 HDP_MEM_POWER_CTRL,
							 RC_MEM_POWER_DS_EN, 1);
		}

		 
		if (adev->cg_flags & (AMD_CG_SUPPORT_HDP_LS | AMD_CG_SUPPORT_HDP_DS |
				      AMD_CG_SUPPORT_HDP_SD)) {
			hdp_mem_pwr_cntl = REG_SET_FIELD(hdp_mem_pwr_cntl, HDP_MEM_POWER_CTRL,
							 ATOMIC_MEM_POWER_CTRL_EN, 1);
			hdp_mem_pwr_cntl = REG_SET_FIELD(hdp_mem_pwr_cntl, HDP_MEM_POWER_CTRL,
							 RC_MEM_POWER_CTRL_EN, 1);
			WREG32_SOC15(HDP, 0, regHDP_MEM_POWER_CTRL, hdp_mem_pwr_cntl);
		}
	}

	 
	hdp_clk_cntl = REG_SET_FIELD(hdp_clk_cntl, HDP_CLK_CNTL,
				     RC_MEM_CLK_SOFT_OVERRIDE, 0);
	WREG32_SOC15(HDP, 0, regHDP_CLK_CNTL, hdp_clk_cntl);
}

static void hdp_v6_0_get_clockgating_state(struct amdgpu_device *adev,
					    u64 *flags)
{
	uint32_t tmp;

	 
	tmp = RREG32_SOC15(HDP, 0, regHDP_MEM_POWER_CTRL);
	if (tmp & HDP_MEM_POWER_CTRL__ATOMIC_MEM_POWER_LS_EN_MASK)
		*flags |= AMD_CG_SUPPORT_HDP_LS;
	else if (tmp & HDP_MEM_POWER_CTRL__ATOMIC_MEM_POWER_DS_EN_MASK)
		*flags |= AMD_CG_SUPPORT_HDP_DS;
	else if (tmp & HDP_MEM_POWER_CTRL__ATOMIC_MEM_POWER_SD_EN_MASK)
		*flags |= AMD_CG_SUPPORT_HDP_SD;
}

const struct amdgpu_hdp_funcs hdp_v6_0_funcs = {
	.flush_hdp = hdp_v6_0_flush_hdp,
	.update_clock_gating = hdp_v6_0_update_clock_gating,
	.get_clock_gating_state = hdp_v6_0_get_clockgating_state,
};
