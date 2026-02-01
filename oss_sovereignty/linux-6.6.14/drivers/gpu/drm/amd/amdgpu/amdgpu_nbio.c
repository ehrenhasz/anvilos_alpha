 

#include "amdgpu.h"
#include "amdgpu_ras.h"

int amdgpu_nbio_ras_sw_init(struct amdgpu_device *adev)
{
	int err;
	struct amdgpu_nbio_ras *ras;

	if (!adev->nbio.ras)
		return 0;

	ras = adev->nbio.ras;
	err = amdgpu_ras_register_ras_block(adev, &ras->ras_block);
	if (err) {
		dev_err(adev->dev, "Failed to register pcie_bif ras block!\n");
		return err;
	}

	strcpy(ras->ras_block.ras_comm.name, "pcie_bif");
	ras->ras_block.ras_comm.block = AMDGPU_RAS_BLOCK__PCIE_BIF;
	ras->ras_block.ras_comm.type = AMDGPU_RAS_ERROR__MULTI_UNCORRECTABLE;
	adev->nbio.ras_if = &ras->ras_block.ras_comm;

	return 0;
}

u64 amdgpu_nbio_get_pcie_replay_count(struct amdgpu_device *adev)
{
	if (adev->nbio.funcs && adev->nbio.funcs->get_pcie_replay_count)
		return adev->nbio.funcs->get_pcie_replay_count(adev);

	return 0;
}

void amdgpu_nbio_get_pcie_usage(struct amdgpu_device *adev, uint64_t *count0,
				uint64_t *count1)
{
	if (adev->nbio.funcs->get_pcie_usage)
		adev->nbio.funcs->get_pcie_usage(adev, count0, count1);

}

int amdgpu_nbio_ras_late_init(struct amdgpu_device *adev, struct ras_common_if *ras_block)
{
	int r;
	r = amdgpu_ras_block_late_init(adev, ras_block);
	if (r)
		return r;

	if (amdgpu_ras_is_supported(adev, ras_block->block)) {
		r = amdgpu_irq_get(adev, &adev->nbio.ras_controller_irq, 0);
		if (r)
			goto late_fini;
		r = amdgpu_irq_get(adev, &adev->nbio.ras_err_event_athub_irq, 0);
		if (r)
			goto late_fini;
	}

	return 0;
late_fini:
	amdgpu_ras_block_late_fini(adev, ras_block);
	return r;
}
