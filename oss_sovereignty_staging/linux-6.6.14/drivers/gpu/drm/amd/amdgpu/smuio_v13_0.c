 
#include "amdgpu.h"
#include "smuio_v13_0.h"
#include "smuio/smuio_13_0_2_offset.h"
#include "smuio/smuio_13_0_2_sh_mask.h"

#define SMUIO_MCM_CONFIG__HOST_GPU_XGMI_MASK	0x00000001L

static u32 smuio_v13_0_get_rom_index_offset(struct amdgpu_device *adev)
{
	return SOC15_REG_OFFSET(SMUIO, 0, regROM_INDEX);
}

static u32 smuio_v13_0_get_rom_data_offset(struct amdgpu_device *adev)
{
	return SOC15_REG_OFFSET(SMUIO, 0, regROM_DATA);
}

static void smuio_v13_0_update_rom_clock_gating(struct amdgpu_device *adev, bool enable)
{
	u32 def, data;

	 
	if (adev->flags & AMD_IS_APU)
		return;

	def = data = RREG32_SOC15(SMUIO, 0, regCGTT_ROM_CLK_CTRL0);

	if (enable && (adev->cg_flags & AMD_CG_SUPPORT_ROM_MGCG))
		data &= ~(CGTT_ROM_CLK_CTRL0__SOFT_OVERRIDE0_MASK |
			CGTT_ROM_CLK_CTRL0__SOFT_OVERRIDE1_MASK);
	else
		data |= CGTT_ROM_CLK_CTRL0__SOFT_OVERRIDE0_MASK |
			CGTT_ROM_CLK_CTRL0__SOFT_OVERRIDE1_MASK;

	if (def != data)
		WREG32_SOC15(SMUIO, 0, regCGTT_ROM_CLK_CTRL0, data);
}

static void smuio_v13_0_get_clock_gating_state(struct amdgpu_device *adev, u64 *flags)
{
	u32 data;

	 
	if (adev->flags & AMD_IS_APU)
		return;

	data = RREG32_SOC15(SMUIO, 0, regCGTT_ROM_CLK_CTRL0);
	if (!(data & CGTT_ROM_CLK_CTRL0__SOFT_OVERRIDE0_MASK))
		*flags |= AMD_CG_SUPPORT_ROM_MGCG;
}

 
static u32 smuio_v13_0_get_die_id(struct amdgpu_device *adev)
{
	u32 data, die_id;

	data = RREG32_SOC15(SMUIO, 0, regSMUIO_MCM_CONFIG);
	die_id = REG_GET_FIELD(data, SMUIO_MCM_CONFIG, DIE_ID);

	return die_id;
}

 
static u32 smuio_v13_0_get_socket_id(struct amdgpu_device *adev)
{
	u32 data, socket_id;

	data = RREG32_SOC15(SMUIO, 0, regSMUIO_MCM_CONFIG);
	socket_id = REG_GET_FIELD(data, SMUIO_MCM_CONFIG, SOCKET_ID);

	return socket_id;
}

 
static bool smuio_v13_0_is_host_gpu_xgmi_supported(struct amdgpu_device *adev)
{
	u32 data;

	data = RREG32_SOC15(SMUIO, 0, regSMUIO_MCM_CONFIG);
	data = REG_GET_FIELD(data, SMUIO_MCM_CONFIG, TOPOLOGY_ID);
	 
	data &= SMUIO_MCM_CONFIG__HOST_GPU_XGMI_MASK;

	return data ? true : false;
}

const struct amdgpu_smuio_funcs smuio_v13_0_funcs = {
	.get_rom_index_offset = smuio_v13_0_get_rom_index_offset,
	.get_rom_data_offset = smuio_v13_0_get_rom_data_offset,
	.get_die_id = smuio_v13_0_get_die_id,
	.get_socket_id = smuio_v13_0_get_socket_id,
	.is_host_gpu_xgmi_supported = smuio_v13_0_is_host_gpu_xgmi_supported,
	.update_rom_clock_gating = smuio_v13_0_update_rom_clock_gating,
	.get_clock_gating_state = smuio_v13_0_get_clock_gating_state,
};
