 
#include "amdgpu.h"
#include "smuio_v13_0_6.h"
#include "smuio/smuio_13_0_6_offset.h"
#include "smuio/smuio_13_0_6_sh_mask.h"

static u32 smuio_v13_0_6_get_rom_index_offset(struct amdgpu_device *adev)
{
	return SOC15_REG_OFFSET(SMUIO, 0, regROM_INDEX);
}

static u32 smuio_v13_0_6_get_rom_data_offset(struct amdgpu_device *adev)
{
	return SOC15_REG_OFFSET(SMUIO, 0, regROM_DATA);
}

const struct amdgpu_smuio_funcs smuio_v13_0_6_funcs = {
	.get_rom_index_offset = smuio_v13_0_6_get_rom_index_offset,
	.get_rom_data_offset = smuio_v13_0_6_get_rom_data_offset,
};
