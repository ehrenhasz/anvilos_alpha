 
#include "umc_v6_0.h"
#include "amdgpu.h"

static void umc_v6_0_init_registers(struct amdgpu_device *adev)
{
	unsigned i,j;

	for (i = 0; i < 4; i++)
		for (j = 0; j < 4; j++)
			WREG32((i*0x100000 + 0x5010c + j*0x2000)/4, 0x1002);
}

const struct amdgpu_umc_funcs umc_v6_0_funcs = {
	.init_registers = umc_v6_0_init_registers,
};
