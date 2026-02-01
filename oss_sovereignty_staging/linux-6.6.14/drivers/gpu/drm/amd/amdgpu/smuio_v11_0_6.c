 
#include "amdgpu.h"
#include "smuio_v11_0_6.h"
#include "smuio/smuio_11_0_6_offset.h"
#include "smuio/smuio_11_0_6_sh_mask.h"

static u32 smuio_v11_0_6_get_rom_index_offset(struct amdgpu_device *adev)
{
	return SOC15_REG_OFFSET(SMUIO, 0, mmROM_INDEX);
}

static u32 smuio_v11_0_6_get_rom_data_offset(struct amdgpu_device *adev)
{
	return SOC15_REG_OFFSET(SMUIO, 0, mmROM_DATA);
}

static void smuio_v11_0_6_update_rom_clock_gating(struct amdgpu_device *adev, bool enable)
{
	u32 def, data;

	 
	if (adev->flags & AMD_IS_APU)
		return;

	def = data = RREG32_SOC15(SMUIO, 0, mmCGTT_ROM_CLK_CTRL0);

	if (enable && (adev->cg_flags & AMD_CG_SUPPORT_ROM_MGCG))
		data &= ~(CGTT_ROM_CLK_CTRL0__SOFT_OVERRIDE0_MASK |
			CGTT_ROM_CLK_CTRL0__SOFT_OVERRIDE1_MASK);
	else
		data |= CGTT_ROM_CLK_CTRL0__SOFT_OVERRIDE0_MASK |
			CGTT_ROM_CLK_CTRL0__SOFT_OVERRIDE1_MASK;

	if (def != data)
		WREG32_SOC15(SMUIO, 0, mmCGTT_ROM_CLK_CTRL0, data);
}

static void smuio_v11_0_6_get_clock_gating_state(struct amdgpu_device *adev, u64 *flags)
{
	u32 data;

	 
	if (adev->flags & AMD_IS_APU)
		return;

	data = RREG32_SOC15(SMUIO, 0, mmCGTT_ROM_CLK_CTRL0);
	if (!(data & CGTT_ROM_CLK_CTRL0__SOFT_OVERRIDE0_MASK))
		*flags |= AMD_CG_SUPPORT_ROM_MGCG;
}

const struct amdgpu_smuio_funcs smuio_v11_0_6_funcs = {
	.get_rom_index_offset = smuio_v11_0_6_get_rom_index_offset,
	.get_rom_data_offset = smuio_v11_0_6_get_rom_data_offset,
	.update_rom_clock_gating = smuio_v11_0_6_update_rom_clock_gating,
	.get_clock_gating_state = smuio_v11_0_6_get_clock_gating_state,
};
