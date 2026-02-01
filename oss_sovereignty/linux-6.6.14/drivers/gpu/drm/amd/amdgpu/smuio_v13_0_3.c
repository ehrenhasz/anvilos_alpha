 
#include "amdgpu.h"
#include "smuio_v13_0_3.h"
#include "soc15_common.h"
#include "smuio/smuio_13_0_3_offset.h"
#include "smuio/smuio_13_0_3_sh_mask.h"

#define PKG_TYPE_MASK		0x00000003L

 
static u32 smuio_v13_0_3_get_die_id(struct amdgpu_device *adev)
{
	u32 data, die_id;

	data = RREG32_SOC15(SMUIO, 0, regSMUIO_MCM_CONFIG);
	die_id = REG_GET_FIELD(data, SMUIO_MCM_CONFIG, DIE_ID);

	return die_id;
}

 
static u32 smuio_v13_0_3_get_socket_id(struct amdgpu_device *adev)
{
	u32 data, socket_id;

	data = RREG32_SOC15(SMUIO, 0, regSMUIO_MCM_CONFIG);
	socket_id = REG_GET_FIELD(data, SMUIO_MCM_CONFIG, SOCKET_ID);

	return socket_id;
}

 

static enum amdgpu_pkg_type smuio_v13_0_3_get_pkg_type(struct amdgpu_device *adev)
{
	enum amdgpu_pkg_type pkg_type;
	u32 data;

	data = RREG32_SOC15(SMUIO, 0, regSMUIO_MCM_CONFIG);
	data = REG_GET_FIELD(data, SMUIO_MCM_CONFIG, PKG_TYPE);
	 
	switch (data & PKG_TYPE_MASK) {
	case 0x2:
		pkg_type = AMDGPU_PKG_TYPE_APU;
		break;
	default:
		pkg_type = AMDGPU_PKG_TYPE_UNKNOWN;
		break;
	}

	return pkg_type;
}


const struct amdgpu_smuio_funcs smuio_v13_0_3_funcs = {
	.get_die_id = smuio_v13_0_3_get_die_id,
	.get_socket_id = smuio_v13_0_3_get_socket_id,
	.get_pkg_type = smuio_v13_0_3_get_pkg_type,
};
