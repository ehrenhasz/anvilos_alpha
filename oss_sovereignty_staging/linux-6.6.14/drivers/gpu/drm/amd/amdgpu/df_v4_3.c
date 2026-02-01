 
#include "amdgpu.h"
#include "df_v4_3.h"

#include "df/df_4_3_offset.h"
#include "df/df_4_3_sh_mask.h"

static bool df_v4_3_query_ras_poison_mode(struct amdgpu_device *adev)
{
	uint32_t hw_assert_msklo, hw_assert_mskhi;
	uint32_t v0, v1, v28, v31;

	hw_assert_msklo = RREG32_SOC15(DF, 0,
				regDF_CS_UMC_AON0_HardwareAssertMaskLow);
	hw_assert_mskhi = RREG32_SOC15(DF, 0,
				regDF_NCS_PG0_HardwareAssertMaskHigh);

	v0 = REG_GET_FIELD(hw_assert_msklo,
		DF_CS_UMC_AON0_HardwareAssertMaskLow, HWAssertMsk0);
	v1 = REG_GET_FIELD(hw_assert_msklo,
		DF_CS_UMC_AON0_HardwareAssertMaskLow, HWAssertMsk1);
	v28 = REG_GET_FIELD(hw_assert_mskhi,
		DF_NCS_PG0_HardwareAssertMaskHigh, HWAssertMsk28);
	v31 = REG_GET_FIELD(hw_assert_mskhi,
		DF_NCS_PG0_HardwareAssertMaskHigh, HWAssertMsk31);

	if (v0 && v1 && v28 && v31)
		return true;
	else if (!v0 && !v1 && !v28 && !v31)
		return false;
	else {
		dev_warn(adev->dev, "DF poison setting is inconsistent(%d:%d:%d:%d)!\n",
				v0, v1, v28, v31);
		return false;
	}
}

const struct amdgpu_df_funcs df_v4_3_funcs = {
	.query_ras_poison_mode = df_v4_3_query_ras_poison_mode,
};
