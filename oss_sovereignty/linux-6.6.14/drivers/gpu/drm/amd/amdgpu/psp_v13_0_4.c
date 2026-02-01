 
#include "amdgpu.h"
#include "amdgpu_psp.h"
#include "amdgpu_ucode.h"
#include "soc15_common.h"
#include "psp_v13_0_4.h"

#include "mp/mp_13_0_4_offset.h"
#include "mp/mp_13_0_4_sh_mask.h"

MODULE_FIRMWARE("amdgpu/psp_13_0_4_toc.bin");
MODULE_FIRMWARE("amdgpu/psp_13_0_4_ta.bin");

static int psp_v13_0_4_init_microcode(struct psp_context *psp)
{
	struct amdgpu_device *adev = psp->adev;
	char ucode_prefix[30];
	int err = 0;

	amdgpu_ucode_ip_version_decode(adev, MP0_HWIP, ucode_prefix, sizeof(ucode_prefix));

	switch (adev->ip_versions[MP0_HWIP][0]) {
	case IP_VERSION(13, 0, 4):
		err = psp_init_toc_microcode(psp, ucode_prefix);
		if (err)
			return err;
		err = psp_init_ta_microcode(psp, ucode_prefix);
		if (err)
			return err;
		break;
	default:
		BUG();
	}

	return 0;
}

static bool psp_v13_0_4_is_sos_alive(struct psp_context *psp)
{
	struct amdgpu_device *adev = psp->adev;
	uint32_t sol_reg;

	sol_reg = RREG32_SOC15(MP0, 0, regMP0_SMN_C2PMSG_81);

	return sol_reg != 0x0;
}

static int psp_v13_0_4_wait_for_bootloader(struct psp_context *psp)
{
	struct amdgpu_device *adev = psp->adev;

	int ret;
	int retry_loop;

	for (retry_loop = 0; retry_loop < 10; retry_loop++) {
		 
		ret = psp_wait_for(psp,
				   SOC15_REG_OFFSET(MP0, 0, regMP0_SMN_C2PMSG_35),
				   0x80000000,
				   0x80000000,
				   false);

		if (ret == 0)
			return 0;
	}

	return ret;
}

static int psp_v13_0_4_bootloader_load_component(struct psp_context  	*psp,
					       struct psp_bin_desc 	*bin_desc,
					       enum psp_bootloader_cmd  bl_cmd)
{
	int ret;
	uint32_t psp_gfxdrv_command_reg = 0;
	struct amdgpu_device *adev = psp->adev;

	 
	if (psp_v13_0_4_is_sos_alive(psp))
		return 0;

	ret = psp_v13_0_4_wait_for_bootloader(psp);
	if (ret)
		return ret;

	memset(psp->fw_pri_buf, 0, PSP_1_MEG);

	 
	memcpy(psp->fw_pri_buf, bin_desc->start_addr, bin_desc->size_bytes);

	 
	WREG32_SOC15(MP0, 0, regMP0_SMN_C2PMSG_36,
	       (uint32_t)(psp->fw_pri_mc_addr >> 20));
	psp_gfxdrv_command_reg = bl_cmd;
	WREG32_SOC15(MP0, 0, regMP0_SMN_C2PMSG_35,
	       psp_gfxdrv_command_reg);

	ret = psp_v13_0_4_wait_for_bootloader(psp);

	return ret;
}

static int psp_v13_0_4_bootloader_load_kdb(struct psp_context *psp)
{
	return psp_v13_0_4_bootloader_load_component(psp, &psp->kdb, PSP_BL__LOAD_KEY_DATABASE);
}

static int psp_v13_0_4_bootloader_load_spl(struct psp_context *psp)
{
	return psp_v13_0_4_bootloader_load_component(psp, &psp->kdb, PSP_BL__LOAD_TOS_SPL_TABLE);
}

static int psp_v13_0_4_bootloader_load_sysdrv(struct psp_context *psp)
{
	return psp_v13_0_4_bootloader_load_component(psp, &psp->sys, PSP_BL__LOAD_SYSDRV);
}

static int psp_v13_0_4_bootloader_load_soc_drv(struct psp_context *psp)
{
	return psp_v13_0_4_bootloader_load_component(psp, &psp->soc_drv, PSP_BL__LOAD_SOCDRV);
}

static int psp_v13_0_4_bootloader_load_intf_drv(struct psp_context *psp)
{
	return psp_v13_0_4_bootloader_load_component(psp, &psp->intf_drv, PSP_BL__LOAD_INTFDRV);
}

static int psp_v13_0_4_bootloader_load_dbg_drv(struct psp_context *psp)
{
	return psp_v13_0_4_bootloader_load_component(psp, &psp->dbg_drv, PSP_BL__LOAD_DBGDRV);
}

static int psp_v13_0_4_bootloader_load_sos(struct psp_context *psp)
{
	int ret;
	unsigned int psp_gfxdrv_command_reg = 0;
	struct amdgpu_device *adev = psp->adev;

	 
	if (psp_v13_0_4_is_sos_alive(psp))
		return 0;

	ret = psp_v13_0_4_wait_for_bootloader(psp);
	if (ret)
		return ret;

	memset(psp->fw_pri_buf, 0, PSP_1_MEG);

	 
	memcpy(psp->fw_pri_buf, psp->sos.start_addr, psp->sos.size_bytes);

	 
	WREG32_SOC15(MP0, 0, regMP0_SMN_C2PMSG_36,
	       (uint32_t)(psp->fw_pri_mc_addr >> 20));
	psp_gfxdrv_command_reg = PSP_BL__LOAD_SOSDRV;
	WREG32_SOC15(MP0, 0, regMP0_SMN_C2PMSG_35,
	       psp_gfxdrv_command_reg);

	 
	mdelay(20);
	ret = psp_wait_for(psp, SOC15_REG_OFFSET(MP0, 0, regMP0_SMN_C2PMSG_81),
			   RREG32_SOC15(MP0, 0, regMP0_SMN_C2PMSG_81),
			   0, true);

	return ret;
}

static int psp_v13_0_4_ring_stop(struct psp_context *psp,
			       enum psp_ring_type ring_type)
{
	int ret = 0;
	struct amdgpu_device *adev = psp->adev;

	if (amdgpu_sriov_vf(adev)) {
		 
		WREG32_SOC15(MP0, 0, regMP0_SMN_C2PMSG_101,
			     GFX_CTRL_CMD_ID_DESTROY_GPCOM_RING);
		 
		mdelay(20);
		 
		ret = psp_wait_for(psp, SOC15_REG_OFFSET(MP0, 0, regMP0_SMN_C2PMSG_101),
				   0x80000000, 0x80000000, false);
	} else {
		 
		WREG32_SOC15(MP0, 0, regMP0_SMN_C2PMSG_64,
			     GFX_CTRL_CMD_ID_DESTROY_RINGS);
		 
		mdelay(20);
		 
		ret = psp_wait_for(psp, SOC15_REG_OFFSET(MP0, 0, regMP0_SMN_C2PMSG_64),
				   0x80000000, 0x80000000, false);
	}

	return ret;
}

static int psp_v13_0_4_ring_create(struct psp_context *psp,
				 enum psp_ring_type ring_type)
{
	int ret = 0;
	unsigned int psp_ring_reg = 0;
	struct psp_ring *ring = &psp->km_ring;
	struct amdgpu_device *adev = psp->adev;

	if (amdgpu_sriov_vf(adev)) {
		ret = psp_v13_0_4_ring_stop(psp, ring_type);
		if (ret) {
			DRM_ERROR("psp_v13_0_ring_stop_sriov failed!\n");
			return ret;
		}

		 
		psp_ring_reg = lower_32_bits(ring->ring_mem_mc_addr);
		WREG32_SOC15(MP0, 0, regMP0_SMN_C2PMSG_102, psp_ring_reg);
		 
		psp_ring_reg = upper_32_bits(ring->ring_mem_mc_addr);
		WREG32_SOC15(MP0, 0, regMP0_SMN_C2PMSG_103, psp_ring_reg);

		 
		WREG32_SOC15(MP0, 0, regMP0_SMN_C2PMSG_101,
			     GFX_CTRL_CMD_ID_INIT_GPCOM_RING);

		 
		mdelay(20);

		 
		ret = psp_wait_for(psp, SOC15_REG_OFFSET(MP0, 0, regMP0_SMN_C2PMSG_101),
				   0x80000000, 0x8000FFFF, false);

	} else {
		 
		ret = psp_wait_for(psp, SOC15_REG_OFFSET(MP0, 0, regMP0_SMN_C2PMSG_64),
				   0x80000000, 0x80000000, false);
		if (ret) {
			DRM_ERROR("Failed to wait for trust OS ready for ring creation\n");
			return ret;
		}

		 
		psp_ring_reg = lower_32_bits(ring->ring_mem_mc_addr);
		WREG32_SOC15(MP0, 0, regMP0_SMN_C2PMSG_69, psp_ring_reg);
		 
		psp_ring_reg = upper_32_bits(ring->ring_mem_mc_addr);
		WREG32_SOC15(MP0, 0, regMP0_SMN_C2PMSG_70, psp_ring_reg);
		 
		psp_ring_reg = ring->ring_size;
		WREG32_SOC15(MP0, 0, regMP0_SMN_C2PMSG_71, psp_ring_reg);
		 
		psp_ring_reg = ring_type;
		psp_ring_reg = psp_ring_reg << 16;
		WREG32_SOC15(MP0, 0, regMP0_SMN_C2PMSG_64, psp_ring_reg);

		 
		mdelay(20);

		 
		ret = psp_wait_for(psp, SOC15_REG_OFFSET(MP0, 0, regMP0_SMN_C2PMSG_64),
				   0x80000000, 0x8000FFFF, false);
	}

	return ret;
}

static int psp_v13_0_4_ring_destroy(struct psp_context *psp,
				  enum psp_ring_type ring_type)
{
	int ret = 0;
	struct psp_ring *ring = &psp->km_ring;
	struct amdgpu_device *adev = psp->adev;

	ret = psp_v13_0_4_ring_stop(psp, ring_type);
	if (ret)
		DRM_ERROR("Fail to stop psp ring\n");

	amdgpu_bo_free_kernel(&adev->firmware.rbuf,
			      &ring->ring_mem_mc_addr,
			      (void **)&ring->ring_mem);

	return ret;
}

static uint32_t psp_v13_0_4_ring_get_wptr(struct psp_context *psp)
{
	uint32_t data;
	struct amdgpu_device *adev = psp->adev;

	if (amdgpu_sriov_vf(adev))
		data = RREG32_SOC15(MP0, 0, regMP0_SMN_C2PMSG_102);
	else
		data = RREG32_SOC15(MP0, 0, regMP0_SMN_C2PMSG_67);

	return data;
}

static void psp_v13_0_4_ring_set_wptr(struct psp_context *psp, uint32_t value)
{
	struct amdgpu_device *adev = psp->adev;

	if (amdgpu_sriov_vf(adev)) {
		WREG32_SOC15(MP0, 0, regMP0_SMN_C2PMSG_102, value);
		WREG32_SOC15(MP0, 0, regMP0_SMN_C2PMSG_101,
			     GFX_CTRL_CMD_ID_CONSUME_CMD);
	} else
		WREG32_SOC15(MP0, 0, regMP0_SMN_C2PMSG_67, value);
}

static const struct psp_funcs psp_v13_0_4_funcs = {
	.init_microcode = psp_v13_0_4_init_microcode,
	.bootloader_load_kdb = psp_v13_0_4_bootloader_load_kdb,
	.bootloader_load_spl = psp_v13_0_4_bootloader_load_spl,
	.bootloader_load_sysdrv = psp_v13_0_4_bootloader_load_sysdrv,
	.bootloader_load_soc_drv = psp_v13_0_4_bootloader_load_soc_drv,
	.bootloader_load_intf_drv = psp_v13_0_4_bootloader_load_intf_drv,
	.bootloader_load_dbg_drv = psp_v13_0_4_bootloader_load_dbg_drv,
	.bootloader_load_sos = psp_v13_0_4_bootloader_load_sos,
	.ring_create = psp_v13_0_4_ring_create,
	.ring_stop = psp_v13_0_4_ring_stop,
	.ring_destroy = psp_v13_0_4_ring_destroy,
	.ring_get_wptr = psp_v13_0_4_ring_get_wptr,
	.ring_set_wptr = psp_v13_0_4_ring_set_wptr,
};

void psp_v13_0_4_set_psp_funcs(struct psp_context *psp)
{
	psp->funcs = &psp_v13_0_4_funcs;
}
