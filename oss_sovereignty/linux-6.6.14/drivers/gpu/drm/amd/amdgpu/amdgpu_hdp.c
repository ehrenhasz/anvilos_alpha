 
#include "amdgpu.h"
#include "amdgpu_ras.h"

int amdgpu_hdp_ras_sw_init(struct amdgpu_device *adev)
{
	int err;
	struct amdgpu_hdp_ras *ras;

	if (!adev->hdp.ras)
		return 0;

	ras = adev->hdp.ras;
	err = amdgpu_ras_register_ras_block(adev, &ras->ras_block);
	if (err) {
		dev_err(adev->dev, "Failed to register hdp ras block!\n");
		return err;
	}

	strcpy(ras->ras_block.ras_comm.name, "hdp");
	ras->ras_block.ras_comm.block = AMDGPU_RAS_BLOCK__HDP;
	ras->ras_block.ras_comm.type = AMDGPU_RAS_ERROR__MULTI_UNCORRECTABLE;
	adev->hdp.ras_if = &ras->ras_block.ras_comm;

	 
	return 0;
}
