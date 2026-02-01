 

#include <linux/firmware.h>

#include "amdgpu.h"
#include "amdgpu_cs.h"
#include "amdgpu_vcn.h"
#include "amdgpu_pm.h"
#include "soc15.h"
#include "soc15d.h"
#include "soc15_common.h"

#include "vcn/vcn_1_0_offset.h"
#include "vcn/vcn_1_0_sh_mask.h"
#include "mmhub/mmhub_9_1_offset.h"
#include "mmhub/mmhub_9_1_sh_mask.h"

#include "ivsrcid/vcn/irqsrcs_vcn_1_0.h"
#include "jpeg_v1_0.h"
#include "vcn_v1_0.h"

#define mmUVD_RBC_XX_IB_REG_CHECK_1_0		0x05ab
#define mmUVD_RBC_XX_IB_REG_CHECK_1_0_BASE_IDX	1
#define mmUVD_REG_XX_MASK_1_0			0x05ac
#define mmUVD_REG_XX_MASK_1_0_BASE_IDX		1

static int vcn_v1_0_stop(struct amdgpu_device *adev);
static void vcn_v1_0_set_dec_ring_funcs(struct amdgpu_device *adev);
static void vcn_v1_0_set_enc_ring_funcs(struct amdgpu_device *adev);
static void vcn_v1_0_set_irq_funcs(struct amdgpu_device *adev);
static int vcn_v1_0_set_powergating_state(void *handle, enum amd_powergating_state state);
static int vcn_v1_0_pause_dpg_mode(struct amdgpu_device *adev,
				int inst_idx, struct dpg_pause_state *new_state);

static void vcn_v1_0_idle_work_handler(struct work_struct *work);
static void vcn_v1_0_ring_begin_use(struct amdgpu_ring *ring);

 
static int vcn_v1_0_early_init(void *handle)
{
	struct amdgpu_device *adev = (struct amdgpu_device *)handle;

	adev->vcn.num_enc_rings = 2;

	vcn_v1_0_set_dec_ring_funcs(adev);
	vcn_v1_0_set_enc_ring_funcs(adev);
	vcn_v1_0_set_irq_funcs(adev);

	jpeg_v1_0_early_init(handle);

	return amdgpu_vcn_early_init(adev);
}

 
static int vcn_v1_0_sw_init(void *handle)
{
	struct amdgpu_ring *ring;
	int i, r;
	struct amdgpu_device *adev = (struct amdgpu_device *)handle;

	 
	r = amdgpu_irq_add_id(adev, SOC15_IH_CLIENTID_VCN,
			VCN_1_0__SRCID__UVD_SYSTEM_MESSAGE_INTERRUPT, &adev->vcn.inst->irq);
	if (r)
		return r;

	 
	for (i = 0; i < adev->vcn.num_enc_rings; ++i) {
		r = amdgpu_irq_add_id(adev, SOC15_IH_CLIENTID_VCN, i + VCN_1_0__SRCID__UVD_ENC_GENERAL_PURPOSE,
					&adev->vcn.inst->irq);
		if (r)
			return r;
	}

	r = amdgpu_vcn_sw_init(adev);
	if (r)
		return r;

	 
	adev->vcn.idle_work.work.func = vcn_v1_0_idle_work_handler;

	amdgpu_vcn_setup_ucode(adev);

	r = amdgpu_vcn_resume(adev);
	if (r)
		return r;

	ring = &adev->vcn.inst->ring_dec;
	ring->vm_hub = AMDGPU_MMHUB0(0);
	sprintf(ring->name, "vcn_dec");
	r = amdgpu_ring_init(adev, ring, 512, &adev->vcn.inst->irq, 0,
			     AMDGPU_RING_PRIO_DEFAULT, NULL);
	if (r)
		return r;

	adev->vcn.internal.scratch9 = adev->vcn.inst->external.scratch9 =
		SOC15_REG_OFFSET(UVD, 0, mmUVD_SCRATCH9);
	adev->vcn.internal.data0 = adev->vcn.inst->external.data0 =
		SOC15_REG_OFFSET(UVD, 0, mmUVD_GPCOM_VCPU_DATA0);
	adev->vcn.internal.data1 = adev->vcn.inst->external.data1 =
		SOC15_REG_OFFSET(UVD, 0, mmUVD_GPCOM_VCPU_DATA1);
	adev->vcn.internal.cmd = adev->vcn.inst->external.cmd =
		SOC15_REG_OFFSET(UVD, 0, mmUVD_GPCOM_VCPU_CMD);
	adev->vcn.internal.nop = adev->vcn.inst->external.nop =
		SOC15_REG_OFFSET(UVD, 0, mmUVD_NO_OP);

	for (i = 0; i < adev->vcn.num_enc_rings; ++i) {
		enum amdgpu_ring_priority_level hw_prio = amdgpu_vcn_get_enc_ring_prio(i);

		ring = &adev->vcn.inst->ring_enc[i];
		ring->vm_hub = AMDGPU_MMHUB0(0);
		sprintf(ring->name, "vcn_enc%d", i);
		r = amdgpu_ring_init(adev, ring, 512, &adev->vcn.inst->irq, 0,
				     hw_prio, NULL);
		if (r)
			return r;
	}

	adev->vcn.pause_dpg_mode = vcn_v1_0_pause_dpg_mode;

	if (amdgpu_vcnfw_log) {
		volatile struct amdgpu_fw_shared *fw_shared = adev->vcn.inst->fw_shared.cpu_addr;

		fw_shared->present_flag_0 = 0;
		amdgpu_vcn_fwlog_init(adev->vcn.inst);
	}

	r = jpeg_v1_0_sw_init(handle);

	return r;
}

 
static int vcn_v1_0_sw_fini(void *handle)
{
	int r;
	struct amdgpu_device *adev = (struct amdgpu_device *)handle;

	r = amdgpu_vcn_suspend(adev);
	if (r)
		return r;

	jpeg_v1_0_sw_fini(handle);

	r = amdgpu_vcn_sw_fini(adev);

	return r;
}

 
static int vcn_v1_0_hw_init(void *handle)
{
	struct amdgpu_device *adev = (struct amdgpu_device *)handle;
	struct amdgpu_ring *ring = &adev->vcn.inst->ring_dec;
	int i, r;

	r = amdgpu_ring_test_helper(ring);
	if (r)
		goto done;

	for (i = 0; i < adev->vcn.num_enc_rings; ++i) {
		ring = &adev->vcn.inst->ring_enc[i];
		r = amdgpu_ring_test_helper(ring);
		if (r)
			goto done;
	}

	ring = adev->jpeg.inst->ring_dec;
	r = amdgpu_ring_test_helper(ring);
	if (r)
		goto done;

done:
	if (!r)
		DRM_INFO("VCN decode and encode initialized successfully(under %s).\n",
			(adev->pg_flags & AMD_PG_SUPPORT_VCN_DPG)?"DPG Mode":"SPG Mode");

	return r;
}

 
static int vcn_v1_0_hw_fini(void *handle)
{
	struct amdgpu_device *adev = (struct amdgpu_device *)handle;

	cancel_delayed_work_sync(&adev->vcn.idle_work);

	if ((adev->pg_flags & AMD_PG_SUPPORT_VCN_DPG) ||
		(adev->vcn.cur_state != AMD_PG_STATE_GATE &&
		 RREG32_SOC15(VCN, 0, mmUVD_STATUS))) {
		vcn_v1_0_set_powergating_state(adev, AMD_PG_STATE_GATE);
	}

	return 0;
}

 
static int vcn_v1_0_suspend(void *handle)
{
	int r;
	struct amdgpu_device *adev = (struct amdgpu_device *)handle;
	bool idle_work_unexecuted;

	idle_work_unexecuted = cancel_delayed_work_sync(&adev->vcn.idle_work);
	if (idle_work_unexecuted) {
		if (adev->pm.dpm_enabled)
			amdgpu_dpm_enable_uvd(adev, false);
	}

	r = vcn_v1_0_hw_fini(adev);
	if (r)
		return r;

	r = amdgpu_vcn_suspend(adev);

	return r;
}

 
static int vcn_v1_0_resume(void *handle)
{
	int r;
	struct amdgpu_device *adev = (struct amdgpu_device *)handle;

	r = amdgpu_vcn_resume(adev);
	if (r)
		return r;

	r = vcn_v1_0_hw_init(adev);

	return r;
}

 
static void vcn_v1_0_mc_resume_spg_mode(struct amdgpu_device *adev)
{
	uint32_t size = AMDGPU_GPU_PAGE_ALIGN(adev->vcn.fw->size + 4);
	uint32_t offset;

	 
	if (adev->firmware.load_type == AMDGPU_FW_LOAD_PSP) {
		WREG32_SOC15(UVD, 0, mmUVD_LMI_VCPU_CACHE_64BIT_BAR_LOW,
			     (adev->firmware.ucode[AMDGPU_UCODE_ID_VCN].tmr_mc_addr_lo));
		WREG32_SOC15(UVD, 0, mmUVD_LMI_VCPU_CACHE_64BIT_BAR_HIGH,
			     (adev->firmware.ucode[AMDGPU_UCODE_ID_VCN].tmr_mc_addr_hi));
		WREG32_SOC15(UVD, 0, mmUVD_VCPU_CACHE_OFFSET0, 0);
		offset = 0;
	} else {
		WREG32_SOC15(UVD, 0, mmUVD_LMI_VCPU_CACHE_64BIT_BAR_LOW,
			lower_32_bits(adev->vcn.inst->gpu_addr));
		WREG32_SOC15(UVD, 0, mmUVD_LMI_VCPU_CACHE_64BIT_BAR_HIGH,
			upper_32_bits(adev->vcn.inst->gpu_addr));
		offset = size;
		WREG32_SOC15(UVD, 0, mmUVD_VCPU_CACHE_OFFSET0,
			     AMDGPU_UVD_FIRMWARE_OFFSET >> 3);
	}

	WREG32_SOC15(UVD, 0, mmUVD_VCPU_CACHE_SIZE0, size);

	 
	WREG32_SOC15(UVD, 0, mmUVD_LMI_VCPU_CACHE1_64BIT_BAR_LOW,
		     lower_32_bits(adev->vcn.inst->gpu_addr + offset));
	WREG32_SOC15(UVD, 0, mmUVD_LMI_VCPU_CACHE1_64BIT_BAR_HIGH,
		     upper_32_bits(adev->vcn.inst->gpu_addr + offset));
	WREG32_SOC15(UVD, 0, mmUVD_VCPU_CACHE_OFFSET1, 0);
	WREG32_SOC15(UVD, 0, mmUVD_VCPU_CACHE_SIZE1, AMDGPU_VCN_STACK_SIZE);

	 
	WREG32_SOC15(UVD, 0, mmUVD_LMI_VCPU_CACHE2_64BIT_BAR_LOW,
		     lower_32_bits(adev->vcn.inst->gpu_addr + offset + AMDGPU_VCN_STACK_SIZE));
	WREG32_SOC15(UVD, 0, mmUVD_LMI_VCPU_CACHE2_64BIT_BAR_HIGH,
		     upper_32_bits(adev->vcn.inst->gpu_addr + offset + AMDGPU_VCN_STACK_SIZE));
	WREG32_SOC15(UVD, 0, mmUVD_VCPU_CACHE_OFFSET2, 0);
	WREG32_SOC15(UVD, 0, mmUVD_VCPU_CACHE_SIZE2, AMDGPU_VCN_CONTEXT_SIZE);

	WREG32_SOC15(UVD, 0, mmUVD_UDEC_ADDR_CONFIG,
			adev->gfx.config.gb_addr_config);
	WREG32_SOC15(UVD, 0, mmUVD_UDEC_DB_ADDR_CONFIG,
			adev->gfx.config.gb_addr_config);
	WREG32_SOC15(UVD, 0, mmUVD_UDEC_DBW_ADDR_CONFIG,
			adev->gfx.config.gb_addr_config);
	WREG32_SOC15(UVD, 0, mmUVD_UDEC_DBW_UV_ADDR_CONFIG,
			adev->gfx.config.gb_addr_config);
	WREG32_SOC15(UVD, 0, mmUVD_MIF_CURR_ADDR_CONFIG,
			adev->gfx.config.gb_addr_config);
	WREG32_SOC15(UVD, 0, mmUVD_MIF_CURR_UV_ADDR_CONFIG,
			adev->gfx.config.gb_addr_config);
	WREG32_SOC15(UVD, 0, mmUVD_MIF_RECON1_ADDR_CONFIG,
			adev->gfx.config.gb_addr_config);
	WREG32_SOC15(UVD, 0, mmUVD_MIF_RECON1_UV_ADDR_CONFIG,
			adev->gfx.config.gb_addr_config);
	WREG32_SOC15(UVD, 0, mmUVD_MIF_REF_ADDR_CONFIG,
			adev->gfx.config.gb_addr_config);
	WREG32_SOC15(UVD, 0, mmUVD_MIF_REF_UV_ADDR_CONFIG,
			adev->gfx.config.gb_addr_config);
	WREG32_SOC15(UVD, 0, mmUVD_JPEG_ADDR_CONFIG,
			adev->gfx.config.gb_addr_config);
	WREG32_SOC15(UVD, 0, mmUVD_JPEG_UV_ADDR_CONFIG,
			adev->gfx.config.gb_addr_config);
}

static void vcn_v1_0_mc_resume_dpg_mode(struct amdgpu_device *adev)
{
	uint32_t size = AMDGPU_GPU_PAGE_ALIGN(adev->vcn.fw->size + 4);
	uint32_t offset;

	 
	if (adev->firmware.load_type == AMDGPU_FW_LOAD_PSP) {
		WREG32_SOC15_DPG_MODE_1_0(UVD, 0, mmUVD_LMI_VCPU_CACHE_64BIT_BAR_LOW,
			     (adev->firmware.ucode[AMDGPU_UCODE_ID_VCN].tmr_mc_addr_lo),
			     0xFFFFFFFF, 0);
		WREG32_SOC15_DPG_MODE_1_0(UVD, 0, mmUVD_LMI_VCPU_CACHE_64BIT_BAR_HIGH,
			     (adev->firmware.ucode[AMDGPU_UCODE_ID_VCN].tmr_mc_addr_hi),
			     0xFFFFFFFF, 0);
		WREG32_SOC15_DPG_MODE_1_0(UVD, 0, mmUVD_VCPU_CACHE_OFFSET0, 0,
			     0xFFFFFFFF, 0);
		offset = 0;
	} else {
		WREG32_SOC15_DPG_MODE_1_0(UVD, 0, mmUVD_LMI_VCPU_CACHE_64BIT_BAR_LOW,
			lower_32_bits(adev->vcn.inst->gpu_addr), 0xFFFFFFFF, 0);
		WREG32_SOC15_DPG_MODE_1_0(UVD, 0, mmUVD_LMI_VCPU_CACHE_64BIT_BAR_HIGH,
			upper_32_bits(adev->vcn.inst->gpu_addr), 0xFFFFFFFF, 0);
		offset = size;
		WREG32_SOC15_DPG_MODE_1_0(UVD, 0, mmUVD_VCPU_CACHE_OFFSET0,
			     AMDGPU_UVD_FIRMWARE_OFFSET >> 3, 0xFFFFFFFF, 0);
	}

	WREG32_SOC15_DPG_MODE_1_0(UVD, 0, mmUVD_VCPU_CACHE_SIZE0, size, 0xFFFFFFFF, 0);

	 
	WREG32_SOC15_DPG_MODE_1_0(UVD, 0, mmUVD_LMI_VCPU_CACHE1_64BIT_BAR_LOW,
		     lower_32_bits(adev->vcn.inst->gpu_addr + offset), 0xFFFFFFFF, 0);
	WREG32_SOC15_DPG_MODE_1_0(UVD, 0, mmUVD_LMI_VCPU_CACHE1_64BIT_BAR_HIGH,
		     upper_32_bits(adev->vcn.inst->gpu_addr + offset), 0xFFFFFFFF, 0);
	WREG32_SOC15_DPG_MODE_1_0(UVD, 0, mmUVD_VCPU_CACHE_OFFSET1, 0,
			     0xFFFFFFFF, 0);
	WREG32_SOC15_DPG_MODE_1_0(UVD, 0, mmUVD_VCPU_CACHE_SIZE1, AMDGPU_VCN_STACK_SIZE,
			     0xFFFFFFFF, 0);

	 
	WREG32_SOC15_DPG_MODE_1_0(UVD, 0, mmUVD_LMI_VCPU_CACHE2_64BIT_BAR_LOW,
		     lower_32_bits(adev->vcn.inst->gpu_addr + offset + AMDGPU_VCN_STACK_SIZE),
			     0xFFFFFFFF, 0);
	WREG32_SOC15_DPG_MODE_1_0(UVD, 0, mmUVD_LMI_VCPU_CACHE2_64BIT_BAR_HIGH,
		     upper_32_bits(adev->vcn.inst->gpu_addr + offset + AMDGPU_VCN_STACK_SIZE),
			     0xFFFFFFFF, 0);
	WREG32_SOC15_DPG_MODE_1_0(UVD, 0, mmUVD_VCPU_CACHE_OFFSET2, 0, 0xFFFFFFFF, 0);
	WREG32_SOC15_DPG_MODE_1_0(UVD, 0, mmUVD_VCPU_CACHE_SIZE2, AMDGPU_VCN_CONTEXT_SIZE,
			     0xFFFFFFFF, 0);

	 
	WREG32_SOC15_DPG_MODE_1_0(UVD, 0, mmUVD_UDEC_ADDR_CONFIG,
			adev->gfx.config.gb_addr_config, 0xFFFFFFFF, 0);
	WREG32_SOC15_DPG_MODE_1_0(UVD, 0, mmUVD_UDEC_DB_ADDR_CONFIG,
			adev->gfx.config.gb_addr_config, 0xFFFFFFFF, 0);
	WREG32_SOC15_DPG_MODE_1_0(UVD, 0, mmUVD_UDEC_DBW_ADDR_CONFIG,
			adev->gfx.config.gb_addr_config, 0xFFFFFFFF, 0);
	WREG32_SOC15_DPG_MODE_1_0(UVD, 0, mmUVD_UDEC_DBW_UV_ADDR_CONFIG,
		adev->gfx.config.gb_addr_config, 0xFFFFFFFF, 0);
	WREG32_SOC15_DPG_MODE_1_0(UVD, 0, mmUVD_MIF_CURR_ADDR_CONFIG,
		adev->gfx.config.gb_addr_config, 0xFFFFFFFF, 0);
	WREG32_SOC15_DPG_MODE_1_0(UVD, 0, mmUVD_MIF_CURR_UV_ADDR_CONFIG,
		adev->gfx.config.gb_addr_config, 0xFFFFFFFF, 0);
	WREG32_SOC15_DPG_MODE_1_0(UVD, 0, mmUVD_MIF_RECON1_ADDR_CONFIG,
		adev->gfx.config.gb_addr_config, 0xFFFFFFFF, 0);
	WREG32_SOC15_DPG_MODE_1_0(UVD, 0, mmUVD_MIF_RECON1_UV_ADDR_CONFIG,
		adev->gfx.config.gb_addr_config, 0xFFFFFFFF, 0);
	WREG32_SOC15_DPG_MODE_1_0(UVD, 0, mmUVD_MIF_REF_ADDR_CONFIG,
		adev->gfx.config.gb_addr_config, 0xFFFFFFFF, 0);
	WREG32_SOC15_DPG_MODE_1_0(UVD, 0, mmUVD_MIF_REF_UV_ADDR_CONFIG,
		adev->gfx.config.gb_addr_config, 0xFFFFFFFF, 0);
}

 
static void vcn_v1_0_disable_clock_gating(struct amdgpu_device *adev)
{
	uint32_t data;

	 
	data = RREG32_SOC15(VCN, 0, mmJPEG_CGC_CTRL);

	if (adev->cg_flags & AMD_CG_SUPPORT_VCN_MGCG)
		data |= 1 << JPEG_CGC_CTRL__DYN_CLOCK_MODE__SHIFT;
	else
		data &= ~JPEG_CGC_CTRL__DYN_CLOCK_MODE_MASK;

	data |= 1 << JPEG_CGC_CTRL__CLK_GATE_DLY_TIMER__SHIFT;
	data |= 4 << JPEG_CGC_CTRL__CLK_OFF_DELAY__SHIFT;
	WREG32_SOC15(VCN, 0, mmJPEG_CGC_CTRL, data);

	data = RREG32_SOC15(VCN, 0, mmJPEG_CGC_GATE);
	data &= ~(JPEG_CGC_GATE__JPEG_MASK | JPEG_CGC_GATE__JPEG2_MASK);
	WREG32_SOC15(VCN, 0, mmJPEG_CGC_GATE, data);

	 
	data = RREG32_SOC15(VCN, 0, mmUVD_CGC_CTRL);
	if (adev->cg_flags & AMD_CG_SUPPORT_VCN_MGCG)
		data |= 1 << UVD_CGC_CTRL__DYN_CLOCK_MODE__SHIFT;
	else
		data &= ~UVD_CGC_CTRL__DYN_CLOCK_MODE_MASK;

	data |= 1 << UVD_CGC_CTRL__CLK_GATE_DLY_TIMER__SHIFT;
	data |= 4 << UVD_CGC_CTRL__CLK_OFF_DELAY__SHIFT;
	WREG32_SOC15(VCN, 0, mmUVD_CGC_CTRL, data);

	data = RREG32_SOC15(VCN, 0, mmUVD_CGC_GATE);
	data &= ~(UVD_CGC_GATE__SYS_MASK
		| UVD_CGC_GATE__UDEC_MASK
		| UVD_CGC_GATE__MPEG2_MASK
		| UVD_CGC_GATE__REGS_MASK
		| UVD_CGC_GATE__RBC_MASK
		| UVD_CGC_GATE__LMI_MC_MASK
		| UVD_CGC_GATE__LMI_UMC_MASK
		| UVD_CGC_GATE__IDCT_MASK
		| UVD_CGC_GATE__MPRD_MASK
		| UVD_CGC_GATE__MPC_MASK
		| UVD_CGC_GATE__LBSI_MASK
		| UVD_CGC_GATE__LRBBM_MASK
		| UVD_CGC_GATE__UDEC_RE_MASK
		| UVD_CGC_GATE__UDEC_CM_MASK
		| UVD_CGC_GATE__UDEC_IT_MASK
		| UVD_CGC_GATE__UDEC_DB_MASK
		| UVD_CGC_GATE__UDEC_MP_MASK
		| UVD_CGC_GATE__WCB_MASK
		| UVD_CGC_GATE__VCPU_MASK
		| UVD_CGC_GATE__SCPU_MASK);
	WREG32_SOC15(VCN, 0, mmUVD_CGC_GATE, data);

	data = RREG32_SOC15(VCN, 0, mmUVD_CGC_CTRL);
	data &= ~(UVD_CGC_CTRL__UDEC_RE_MODE_MASK
		| UVD_CGC_CTRL__UDEC_CM_MODE_MASK
		| UVD_CGC_CTRL__UDEC_IT_MODE_MASK
		| UVD_CGC_CTRL__UDEC_DB_MODE_MASK
		| UVD_CGC_CTRL__UDEC_MP_MODE_MASK
		| UVD_CGC_CTRL__SYS_MODE_MASK
		| UVD_CGC_CTRL__UDEC_MODE_MASK
		| UVD_CGC_CTRL__MPEG2_MODE_MASK
		| UVD_CGC_CTRL__REGS_MODE_MASK
		| UVD_CGC_CTRL__RBC_MODE_MASK
		| UVD_CGC_CTRL__LMI_MC_MODE_MASK
		| UVD_CGC_CTRL__LMI_UMC_MODE_MASK
		| UVD_CGC_CTRL__IDCT_MODE_MASK
		| UVD_CGC_CTRL__MPRD_MODE_MASK
		| UVD_CGC_CTRL__MPC_MODE_MASK
		| UVD_CGC_CTRL__LBSI_MODE_MASK
		| UVD_CGC_CTRL__LRBBM_MODE_MASK
		| UVD_CGC_CTRL__WCB_MODE_MASK
		| UVD_CGC_CTRL__VCPU_MODE_MASK
		| UVD_CGC_CTRL__SCPU_MODE_MASK);
	WREG32_SOC15(VCN, 0, mmUVD_CGC_CTRL, data);

	 
	data = RREG32_SOC15(VCN, 0, mmUVD_SUVD_CGC_GATE);
	data |= (UVD_SUVD_CGC_GATE__SRE_MASK
		| UVD_SUVD_CGC_GATE__SIT_MASK
		| UVD_SUVD_CGC_GATE__SMP_MASK
		| UVD_SUVD_CGC_GATE__SCM_MASK
		| UVD_SUVD_CGC_GATE__SDB_MASK
		| UVD_SUVD_CGC_GATE__SRE_H264_MASK
		| UVD_SUVD_CGC_GATE__SRE_HEVC_MASK
		| UVD_SUVD_CGC_GATE__SIT_H264_MASK
		| UVD_SUVD_CGC_GATE__SIT_HEVC_MASK
		| UVD_SUVD_CGC_GATE__SCM_H264_MASK
		| UVD_SUVD_CGC_GATE__SCM_HEVC_MASK
		| UVD_SUVD_CGC_GATE__SDB_H264_MASK
		| UVD_SUVD_CGC_GATE__SDB_HEVC_MASK
		| UVD_SUVD_CGC_GATE__SCLR_MASK
		| UVD_SUVD_CGC_GATE__UVD_SC_MASK
		| UVD_SUVD_CGC_GATE__ENT_MASK
		| UVD_SUVD_CGC_GATE__SIT_HEVC_DEC_MASK
		| UVD_SUVD_CGC_GATE__SIT_HEVC_ENC_MASK
		| UVD_SUVD_CGC_GATE__SITE_MASK
		| UVD_SUVD_CGC_GATE__SRE_VP9_MASK
		| UVD_SUVD_CGC_GATE__SCM_VP9_MASK
		| UVD_SUVD_CGC_GATE__SIT_VP9_DEC_MASK
		| UVD_SUVD_CGC_GATE__SDB_VP9_MASK
		| UVD_SUVD_CGC_GATE__IME_HEVC_MASK);
	WREG32_SOC15(VCN, 0, mmUVD_SUVD_CGC_GATE, data);

	data = RREG32_SOC15(VCN, 0, mmUVD_SUVD_CGC_CTRL);
	data &= ~(UVD_SUVD_CGC_CTRL__SRE_MODE_MASK
		| UVD_SUVD_CGC_CTRL__SIT_MODE_MASK
		| UVD_SUVD_CGC_CTRL__SMP_MODE_MASK
		| UVD_SUVD_CGC_CTRL__SCM_MODE_MASK
		| UVD_SUVD_CGC_CTRL__SDB_MODE_MASK
		| UVD_SUVD_CGC_CTRL__SCLR_MODE_MASK
		| UVD_SUVD_CGC_CTRL__UVD_SC_MODE_MASK
		| UVD_SUVD_CGC_CTRL__ENT_MODE_MASK
		| UVD_SUVD_CGC_CTRL__IME_MODE_MASK
		| UVD_SUVD_CGC_CTRL__SITE_MODE_MASK);
	WREG32_SOC15(VCN, 0, mmUVD_SUVD_CGC_CTRL, data);
}

 
static void vcn_v1_0_enable_clock_gating(struct amdgpu_device *adev)
{
	uint32_t data = 0;

	 
	data = RREG32_SOC15(VCN, 0, mmJPEG_CGC_CTRL);
	if (adev->cg_flags & AMD_CG_SUPPORT_VCN_MGCG)
		data |= 1 << JPEG_CGC_CTRL__DYN_CLOCK_MODE__SHIFT;
	else
		data |= 0 << JPEG_CGC_CTRL__DYN_CLOCK_MODE__SHIFT;
	data |= 1 << JPEG_CGC_CTRL__CLK_GATE_DLY_TIMER__SHIFT;
	data |= 4 << JPEG_CGC_CTRL__CLK_OFF_DELAY__SHIFT;
	WREG32_SOC15(VCN, 0, mmJPEG_CGC_CTRL, data);

	data = RREG32_SOC15(VCN, 0, mmJPEG_CGC_GATE);
	data |= (JPEG_CGC_GATE__JPEG_MASK | JPEG_CGC_GATE__JPEG2_MASK);
	WREG32_SOC15(VCN, 0, mmJPEG_CGC_GATE, data);

	 
	data = RREG32_SOC15(VCN, 0, mmUVD_CGC_CTRL);
	if (adev->cg_flags & AMD_CG_SUPPORT_VCN_MGCG)
		data |= 1 << UVD_CGC_CTRL__DYN_CLOCK_MODE__SHIFT;
	else
		data |= 0 << UVD_CGC_CTRL__DYN_CLOCK_MODE__SHIFT;
	data |= 1 << UVD_CGC_CTRL__CLK_GATE_DLY_TIMER__SHIFT;
	data |= 4 << UVD_CGC_CTRL__CLK_OFF_DELAY__SHIFT;
	WREG32_SOC15(VCN, 0, mmUVD_CGC_CTRL, data);

	data = RREG32_SOC15(VCN, 0, mmUVD_CGC_CTRL);
	data |= (UVD_CGC_CTRL__UDEC_RE_MODE_MASK
		| UVD_CGC_CTRL__UDEC_CM_MODE_MASK
		| UVD_CGC_CTRL__UDEC_IT_MODE_MASK
		| UVD_CGC_CTRL__UDEC_DB_MODE_MASK
		| UVD_CGC_CTRL__UDEC_MP_MODE_MASK
		| UVD_CGC_CTRL__SYS_MODE_MASK
		| UVD_CGC_CTRL__UDEC_MODE_MASK
		| UVD_CGC_CTRL__MPEG2_MODE_MASK
		| UVD_CGC_CTRL__REGS_MODE_MASK
		| UVD_CGC_CTRL__RBC_MODE_MASK
		| UVD_CGC_CTRL__LMI_MC_MODE_MASK
		| UVD_CGC_CTRL__LMI_UMC_MODE_MASK
		| UVD_CGC_CTRL__IDCT_MODE_MASK
		| UVD_CGC_CTRL__MPRD_MODE_MASK
		| UVD_CGC_CTRL__MPC_MODE_MASK
		| UVD_CGC_CTRL__LBSI_MODE_MASK
		| UVD_CGC_CTRL__LRBBM_MODE_MASK
		| UVD_CGC_CTRL__WCB_MODE_MASK
		| UVD_CGC_CTRL__VCPU_MODE_MASK
		| UVD_CGC_CTRL__SCPU_MODE_MASK);
	WREG32_SOC15(VCN, 0, mmUVD_CGC_CTRL, data);

	data = RREG32_SOC15(VCN, 0, mmUVD_SUVD_CGC_CTRL);
	data |= (UVD_SUVD_CGC_CTRL__SRE_MODE_MASK
		| UVD_SUVD_CGC_CTRL__SIT_MODE_MASK
		| UVD_SUVD_CGC_CTRL__SMP_MODE_MASK
		| UVD_SUVD_CGC_CTRL__SCM_MODE_MASK
		| UVD_SUVD_CGC_CTRL__SDB_MODE_MASK
		| UVD_SUVD_CGC_CTRL__SCLR_MODE_MASK
		| UVD_SUVD_CGC_CTRL__UVD_SC_MODE_MASK
		| UVD_SUVD_CGC_CTRL__ENT_MODE_MASK
		| UVD_SUVD_CGC_CTRL__IME_MODE_MASK
		| UVD_SUVD_CGC_CTRL__SITE_MODE_MASK);
	WREG32_SOC15(VCN, 0, mmUVD_SUVD_CGC_CTRL, data);
}

static void vcn_v1_0_clock_gating_dpg_mode(struct amdgpu_device *adev, uint8_t sram_sel)
{
	uint32_t reg_data = 0;

	 
	if (adev->cg_flags & AMD_CG_SUPPORT_VCN_MGCG)
		reg_data = 1 << JPEG_CGC_CTRL__DYN_CLOCK_MODE__SHIFT;
	else
		reg_data = 0 << JPEG_CGC_CTRL__DYN_CLOCK_MODE__SHIFT;
	reg_data |= 1 << JPEG_CGC_CTRL__CLK_GATE_DLY_TIMER__SHIFT;
	reg_data |= 4 << JPEG_CGC_CTRL__CLK_OFF_DELAY__SHIFT;
	WREG32_SOC15_DPG_MODE_1_0(UVD, 0, mmJPEG_CGC_CTRL, reg_data, 0xFFFFFFFF, sram_sel);

	WREG32_SOC15_DPG_MODE_1_0(UVD, 0, mmJPEG_CGC_GATE, 0, 0xFFFFFFFF, sram_sel);

	 
	if (adev->cg_flags & AMD_CG_SUPPORT_VCN_MGCG)
		reg_data = 1 << UVD_CGC_CTRL__DYN_CLOCK_MODE__SHIFT;
	else
		reg_data = 0 << UVD_CGC_CTRL__DYN_CLOCK_MODE__SHIFT;
	reg_data |= 1 << UVD_CGC_CTRL__CLK_GATE_DLY_TIMER__SHIFT;
	reg_data |= 4 << UVD_CGC_CTRL__CLK_OFF_DELAY__SHIFT;
	reg_data &= ~(UVD_CGC_CTRL__UDEC_RE_MODE_MASK |
		 UVD_CGC_CTRL__UDEC_CM_MODE_MASK |
		 UVD_CGC_CTRL__UDEC_IT_MODE_MASK |
		 UVD_CGC_CTRL__UDEC_DB_MODE_MASK |
		 UVD_CGC_CTRL__UDEC_MP_MODE_MASK |
		 UVD_CGC_CTRL__SYS_MODE_MASK |
		 UVD_CGC_CTRL__UDEC_MODE_MASK |
		 UVD_CGC_CTRL__MPEG2_MODE_MASK |
		 UVD_CGC_CTRL__REGS_MODE_MASK |
		 UVD_CGC_CTRL__RBC_MODE_MASK |
		 UVD_CGC_CTRL__LMI_MC_MODE_MASK |
		 UVD_CGC_CTRL__LMI_UMC_MODE_MASK |
		 UVD_CGC_CTRL__IDCT_MODE_MASK |
		 UVD_CGC_CTRL__MPRD_MODE_MASK |
		 UVD_CGC_CTRL__MPC_MODE_MASK |
		 UVD_CGC_CTRL__LBSI_MODE_MASK |
		 UVD_CGC_CTRL__LRBBM_MODE_MASK |
		 UVD_CGC_CTRL__WCB_MODE_MASK |
		 UVD_CGC_CTRL__VCPU_MODE_MASK |
		 UVD_CGC_CTRL__SCPU_MODE_MASK);
	WREG32_SOC15_DPG_MODE_1_0(UVD, 0, mmUVD_CGC_CTRL, reg_data, 0xFFFFFFFF, sram_sel);

	 
	WREG32_SOC15_DPG_MODE_1_0(UVD, 0, mmUVD_CGC_GATE, 0, 0xFFFFFFFF, sram_sel);

	 
	WREG32_SOC15_DPG_MODE_1_0(UVD, 0, mmUVD_SUVD_CGC_GATE, 1, 0xFFFFFFFF, sram_sel);

	 
	WREG32_SOC15_DPG_MODE_1_0(UVD, 0, mmUVD_SUVD_CGC_CTRL, 0, 0xFFFFFFFF, sram_sel);
}

static void vcn_1_0_disable_static_power_gating(struct amdgpu_device *adev)
{
	uint32_t data = 0;

	if (adev->pg_flags & AMD_PG_SUPPORT_VCN) {
		data = (1 << UVD_PGFSM_CONFIG__UVDM_PWR_CONFIG__SHIFT
			| 1 << UVD_PGFSM_CONFIG__UVDU_PWR_CONFIG__SHIFT
			| 2 << UVD_PGFSM_CONFIG__UVDF_PWR_CONFIG__SHIFT
			| 2 << UVD_PGFSM_CONFIG__UVDC_PWR_CONFIG__SHIFT
			| 2 << UVD_PGFSM_CONFIG__UVDB_PWR_CONFIG__SHIFT
			| 2 << UVD_PGFSM_CONFIG__UVDIL_PWR_CONFIG__SHIFT
			| 2 << UVD_PGFSM_CONFIG__UVDIR_PWR_CONFIG__SHIFT
			| 2 << UVD_PGFSM_CONFIG__UVDTD_PWR_CONFIG__SHIFT
			| 2 << UVD_PGFSM_CONFIG__UVDTE_PWR_CONFIG__SHIFT
			| 2 << UVD_PGFSM_CONFIG__UVDE_PWR_CONFIG__SHIFT
			| 2 << UVD_PGFSM_CONFIG__UVDW_PWR_CONFIG__SHIFT);

		WREG32_SOC15(VCN, 0, mmUVD_PGFSM_CONFIG, data);
		SOC15_WAIT_ON_RREG(VCN, 0, mmUVD_PGFSM_STATUS, UVD_PGFSM_STATUS__UVDM_UVDU_PWR_ON, 0xFFFFFF);
	} else {
		data = (1 << UVD_PGFSM_CONFIG__UVDM_PWR_CONFIG__SHIFT
			| 1 << UVD_PGFSM_CONFIG__UVDU_PWR_CONFIG__SHIFT
			| 1 << UVD_PGFSM_CONFIG__UVDF_PWR_CONFIG__SHIFT
			| 1 << UVD_PGFSM_CONFIG__UVDC_PWR_CONFIG__SHIFT
			| 1 << UVD_PGFSM_CONFIG__UVDB_PWR_CONFIG__SHIFT
			| 1 << UVD_PGFSM_CONFIG__UVDIL_PWR_CONFIG__SHIFT
			| 1 << UVD_PGFSM_CONFIG__UVDIR_PWR_CONFIG__SHIFT
			| 1 << UVD_PGFSM_CONFIG__UVDTD_PWR_CONFIG__SHIFT
			| 1 << UVD_PGFSM_CONFIG__UVDTE_PWR_CONFIG__SHIFT
			| 1 << UVD_PGFSM_CONFIG__UVDE_PWR_CONFIG__SHIFT
			| 1 << UVD_PGFSM_CONFIG__UVDW_PWR_CONFIG__SHIFT);
		WREG32_SOC15(VCN, 0, mmUVD_PGFSM_CONFIG, data);
		SOC15_WAIT_ON_RREG(VCN, 0, mmUVD_PGFSM_STATUS, 0,  0xFFFFFFFF);
	}

	 

	data = RREG32_SOC15(VCN, 0, mmUVD_POWER_STATUS);
	data &= ~0x103;
	if (adev->pg_flags & AMD_PG_SUPPORT_VCN)
		data |= UVD_PGFSM_CONFIG__UVDM_UVDU_PWR_ON | UVD_POWER_STATUS__UVD_PG_EN_MASK;

	WREG32_SOC15(VCN, 0, mmUVD_POWER_STATUS, data);
}

static void vcn_1_0_enable_static_power_gating(struct amdgpu_device *adev)
{
	uint32_t data = 0;

	if (adev->pg_flags & AMD_PG_SUPPORT_VCN) {
		 
		data = RREG32_SOC15(VCN, 0, mmUVD_POWER_STATUS);
		data &= ~UVD_POWER_STATUS__UVD_POWER_STATUS_MASK;
		data |= UVD_POWER_STATUS__UVD_POWER_STATUS_TILES_OFF;
		WREG32_SOC15(VCN, 0, mmUVD_POWER_STATUS, data);


		data = (2 << UVD_PGFSM_CONFIG__UVDM_PWR_CONFIG__SHIFT
			| 2 << UVD_PGFSM_CONFIG__UVDU_PWR_CONFIG__SHIFT
			| 2 << UVD_PGFSM_CONFIG__UVDF_PWR_CONFIG__SHIFT
			| 2 << UVD_PGFSM_CONFIG__UVDC_PWR_CONFIG__SHIFT
			| 2 << UVD_PGFSM_CONFIG__UVDB_PWR_CONFIG__SHIFT
			| 2 << UVD_PGFSM_CONFIG__UVDIL_PWR_CONFIG__SHIFT
			| 2 << UVD_PGFSM_CONFIG__UVDIR_PWR_CONFIG__SHIFT
			| 2 << UVD_PGFSM_CONFIG__UVDTD_PWR_CONFIG__SHIFT
			| 2 << UVD_PGFSM_CONFIG__UVDTE_PWR_CONFIG__SHIFT
			| 2 << UVD_PGFSM_CONFIG__UVDE_PWR_CONFIG__SHIFT
			| 2 << UVD_PGFSM_CONFIG__UVDW_PWR_CONFIG__SHIFT);

		WREG32_SOC15(VCN, 0, mmUVD_PGFSM_CONFIG, data);

		data = (2 << UVD_PGFSM_STATUS__UVDM_PWR_STATUS__SHIFT
			| 2 << UVD_PGFSM_STATUS__UVDU_PWR_STATUS__SHIFT
			| 2 << UVD_PGFSM_STATUS__UVDF_PWR_STATUS__SHIFT
			| 2 << UVD_PGFSM_STATUS__UVDC_PWR_STATUS__SHIFT
			| 2 << UVD_PGFSM_STATUS__UVDB_PWR_STATUS__SHIFT
			| 2 << UVD_PGFSM_STATUS__UVDIL_PWR_STATUS__SHIFT
			| 2 << UVD_PGFSM_STATUS__UVDIR_PWR_STATUS__SHIFT
			| 2 << UVD_PGFSM_STATUS__UVDTD_PWR_STATUS__SHIFT
			| 2 << UVD_PGFSM_STATUS__UVDTE_PWR_STATUS__SHIFT
			| 2 << UVD_PGFSM_STATUS__UVDE_PWR_STATUS__SHIFT
			| 2 << UVD_PGFSM_STATUS__UVDW_PWR_STATUS__SHIFT);
		SOC15_WAIT_ON_RREG(VCN, 0, mmUVD_PGFSM_STATUS, data, 0xFFFFFFFF);
	}
}

 
static int vcn_v1_0_start_spg_mode(struct amdgpu_device *adev)
{
	struct amdgpu_ring *ring = &adev->vcn.inst->ring_dec;
	uint32_t rb_bufsz, tmp;
	uint32_t lmi_swap_cntl;
	int i, j, r;

	 
	lmi_swap_cntl = 0;

	vcn_1_0_disable_static_power_gating(adev);

	tmp = RREG32_SOC15(UVD, 0, mmUVD_STATUS) | UVD_STATUS__UVD_BUSY;
	WREG32_SOC15(UVD, 0, mmUVD_STATUS, tmp);

	 
	vcn_v1_0_disable_clock_gating(adev);

	 
	WREG32_P(SOC15_REG_OFFSET(UVD, 0, mmUVD_MASTINT_EN), 0,
			~UVD_MASTINT_EN__VCPU_EN_MASK);

	 
	tmp = RREG32_SOC15(UVD, 0, mmUVD_LMI_CTRL);
	WREG32_SOC15(UVD, 0, mmUVD_LMI_CTRL, tmp		|
		UVD_LMI_CTRL__WRITE_CLEAN_TIMER_EN_MASK	|
		UVD_LMI_CTRL__MASK_MC_URGENT_MASK			|
		UVD_LMI_CTRL__DATA_COHERENCY_EN_MASK		|
		UVD_LMI_CTRL__VCPU_DATA_COHERENCY_EN_MASK);

#ifdef __BIG_ENDIAN
	 
	lmi_swap_cntl = 0xa;
#endif
	WREG32_SOC15(UVD, 0, mmUVD_LMI_SWAP_CNTL, lmi_swap_cntl);

	tmp = RREG32_SOC15(UVD, 0, mmUVD_MPC_CNTL);
	tmp &= ~UVD_MPC_CNTL__REPLACEMENT_MODE_MASK;
	tmp |= 0x2 << UVD_MPC_CNTL__REPLACEMENT_MODE__SHIFT;
	WREG32_SOC15(UVD, 0, mmUVD_MPC_CNTL, tmp);

	WREG32_SOC15(UVD, 0, mmUVD_MPC_SET_MUXA0,
		((0x1 << UVD_MPC_SET_MUXA0__VARA_1__SHIFT) |
		(0x2 << UVD_MPC_SET_MUXA0__VARA_2__SHIFT) |
		(0x3 << UVD_MPC_SET_MUXA0__VARA_3__SHIFT) |
		(0x4 << UVD_MPC_SET_MUXA0__VARA_4__SHIFT)));

	WREG32_SOC15(UVD, 0, mmUVD_MPC_SET_MUXB0,
		((0x1 << UVD_MPC_SET_MUXB0__VARB_1__SHIFT) |
		(0x2 << UVD_MPC_SET_MUXB0__VARB_2__SHIFT) |
		(0x3 << UVD_MPC_SET_MUXB0__VARB_3__SHIFT) |
		(0x4 << UVD_MPC_SET_MUXB0__VARB_4__SHIFT)));

	WREG32_SOC15(UVD, 0, mmUVD_MPC_SET_MUX,
		((0x0 << UVD_MPC_SET_MUX__SET_0__SHIFT) |
		(0x1 << UVD_MPC_SET_MUX__SET_1__SHIFT) |
		(0x2 << UVD_MPC_SET_MUX__SET_2__SHIFT)));

	vcn_v1_0_mc_resume_spg_mode(adev);

	WREG32_SOC15(UVD, 0, mmUVD_REG_XX_MASK_1_0, 0x10);
	WREG32_SOC15(UVD, 0, mmUVD_RBC_XX_IB_REG_CHECK_1_0,
		RREG32_SOC15(UVD, 0, mmUVD_RBC_XX_IB_REG_CHECK_1_0) | 0x3);

	 
	WREG32_SOC15(UVD, 0, mmUVD_VCPU_CNTL, UVD_VCPU_CNTL__CLK_EN_MASK);

	 
	WREG32_P(SOC15_REG_OFFSET(UVD, 0, mmUVD_SOFT_RESET), 0,
			~UVD_SOFT_RESET__VCPU_SOFT_RESET_MASK);

	 
	WREG32_P(SOC15_REG_OFFSET(UVD, 0, mmUVD_LMI_CTRL2), 0,
			~UVD_LMI_CTRL2__STALL_ARB_UMC_MASK);

	tmp = RREG32_SOC15(UVD, 0, mmUVD_SOFT_RESET);
	tmp &= ~UVD_SOFT_RESET__LMI_SOFT_RESET_MASK;
	tmp &= ~UVD_SOFT_RESET__LMI_UMC_SOFT_RESET_MASK;
	WREG32_SOC15(UVD, 0, mmUVD_SOFT_RESET, tmp);

	for (i = 0; i < 10; ++i) {
		uint32_t status;

		for (j = 0; j < 100; ++j) {
			status = RREG32_SOC15(UVD, 0, mmUVD_STATUS);
			if (status & UVD_STATUS__IDLE)
				break;
			mdelay(10);
		}
		r = 0;
		if (status & UVD_STATUS__IDLE)
			break;

		DRM_ERROR("VCN decode not responding, trying to reset the VCPU!!!\n");
		WREG32_P(SOC15_REG_OFFSET(UVD, 0, mmUVD_SOFT_RESET),
				UVD_SOFT_RESET__VCPU_SOFT_RESET_MASK,
				~UVD_SOFT_RESET__VCPU_SOFT_RESET_MASK);
		mdelay(10);
		WREG32_P(SOC15_REG_OFFSET(UVD, 0, mmUVD_SOFT_RESET), 0,
				~UVD_SOFT_RESET__VCPU_SOFT_RESET_MASK);
		mdelay(10);
		r = -1;
	}

	if (r) {
		DRM_ERROR("VCN decode not responding, giving up!!!\n");
		return r;
	}
	 
	WREG32_P(SOC15_REG_OFFSET(UVD, 0, mmUVD_MASTINT_EN),
		UVD_MASTINT_EN__VCPU_EN_MASK, ~UVD_MASTINT_EN__VCPU_EN_MASK);

	 
	WREG32_P(SOC15_REG_OFFSET(UVD, 0, mmUVD_SYS_INT_EN),
		UVD_SYS_INT_EN__UVD_JRBC_EN_MASK,
		~UVD_SYS_INT_EN__UVD_JRBC_EN_MASK);

	 
	tmp = RREG32_SOC15(UVD, 0, mmUVD_STATUS) & ~UVD_STATUS__UVD_BUSY;
	WREG32_SOC15(UVD, 0, mmUVD_STATUS, tmp);

	 
	rb_bufsz = order_base_2(ring->ring_size);
	tmp = REG_SET_FIELD(0, UVD_RBC_RB_CNTL, RB_BUFSZ, rb_bufsz);
	tmp = REG_SET_FIELD(tmp, UVD_RBC_RB_CNTL, RB_BLKSZ, 1);
	tmp = REG_SET_FIELD(tmp, UVD_RBC_RB_CNTL, RB_NO_FETCH, 1);
	tmp = REG_SET_FIELD(tmp, UVD_RBC_RB_CNTL, RB_NO_UPDATE, 1);
	tmp = REG_SET_FIELD(tmp, UVD_RBC_RB_CNTL, RB_RPTR_WR_EN, 1);
	WREG32_SOC15(UVD, 0, mmUVD_RBC_RB_CNTL, tmp);

	 
	WREG32_SOC15(UVD, 0, mmUVD_RBC_RB_WPTR_CNTL, 0);

	 
	WREG32_SOC15(UVD, 0, mmUVD_RBC_RB_RPTR_ADDR,
			(upper_32_bits(ring->gpu_addr) >> 2));

	 
	WREG32_SOC15(UVD, 0, mmUVD_LMI_RBC_RB_64BIT_BAR_LOW,
			lower_32_bits(ring->gpu_addr));
	WREG32_SOC15(UVD, 0, mmUVD_LMI_RBC_RB_64BIT_BAR_HIGH,
			upper_32_bits(ring->gpu_addr));

	 
	WREG32_SOC15(UVD, 0, mmUVD_RBC_RB_RPTR, 0);

	WREG32_SOC15(UVD, 0, mmUVD_SCRATCH2, 0);

	ring->wptr = RREG32_SOC15(UVD, 0, mmUVD_RBC_RB_RPTR);
	WREG32_SOC15(UVD, 0, mmUVD_RBC_RB_WPTR,
			lower_32_bits(ring->wptr));

	WREG32_P(SOC15_REG_OFFSET(UVD, 0, mmUVD_RBC_RB_CNTL), 0,
			~UVD_RBC_RB_CNTL__RB_NO_FETCH_MASK);

	ring = &adev->vcn.inst->ring_enc[0];
	WREG32_SOC15(UVD, 0, mmUVD_RB_RPTR, lower_32_bits(ring->wptr));
	WREG32_SOC15(UVD, 0, mmUVD_RB_WPTR, lower_32_bits(ring->wptr));
	WREG32_SOC15(UVD, 0, mmUVD_RB_BASE_LO, ring->gpu_addr);
	WREG32_SOC15(UVD, 0, mmUVD_RB_BASE_HI, upper_32_bits(ring->gpu_addr));
	WREG32_SOC15(UVD, 0, mmUVD_RB_SIZE, ring->ring_size / 4);

	ring = &adev->vcn.inst->ring_enc[1];
	WREG32_SOC15(UVD, 0, mmUVD_RB_RPTR2, lower_32_bits(ring->wptr));
	WREG32_SOC15(UVD, 0, mmUVD_RB_WPTR2, lower_32_bits(ring->wptr));
	WREG32_SOC15(UVD, 0, mmUVD_RB_BASE_LO2, ring->gpu_addr);
	WREG32_SOC15(UVD, 0, mmUVD_RB_BASE_HI2, upper_32_bits(ring->gpu_addr));
	WREG32_SOC15(UVD, 0, mmUVD_RB_SIZE2, ring->ring_size / 4);

	jpeg_v1_0_start(adev, 0);

	return 0;
}

static int vcn_v1_0_start_dpg_mode(struct amdgpu_device *adev)
{
	struct amdgpu_ring *ring = &adev->vcn.inst->ring_dec;
	uint32_t rb_bufsz, tmp;
	uint32_t lmi_swap_cntl;

	 
	lmi_swap_cntl = 0;

	vcn_1_0_enable_static_power_gating(adev);

	 
	tmp = RREG32_SOC15(UVD, 0, mmUVD_POWER_STATUS);
	tmp |= UVD_POWER_STATUS__UVD_PG_MODE_MASK;
	tmp |= UVD_POWER_STATUS__UVD_PG_EN_MASK;
	WREG32_SOC15(UVD, 0, mmUVD_POWER_STATUS, tmp);

	 
	vcn_v1_0_clock_gating_dpg_mode(adev, 0);

	 
	tmp = (0xFF << UVD_VCPU_CNTL__PRB_TIMEOUT_VAL__SHIFT);
	tmp |= UVD_VCPU_CNTL__CLK_EN_MASK;
	tmp |= UVD_VCPU_CNTL__MIF_WR_LOW_THRESHOLD_BP_MASK;
	WREG32_SOC15_DPG_MODE_1_0(UVD, 0, mmUVD_VCPU_CNTL, tmp, 0xFFFFFFFF, 0);

	 
	WREG32_SOC15_DPG_MODE_1_0(UVD, 0, mmUVD_MASTINT_EN,
			0, UVD_MASTINT_EN__VCPU_EN_MASK, 0);

	 
	WREG32_SOC15_DPG_MODE_1_0(UVD, 0, mmUVD_LMI_CTRL,
		(8 << UVD_LMI_CTRL__WRITE_CLEAN_TIMER__SHIFT) |
		UVD_LMI_CTRL__WRITE_CLEAN_TIMER_EN_MASK |
		UVD_LMI_CTRL__DATA_COHERENCY_EN_MASK |
		UVD_LMI_CTRL__VCPU_DATA_COHERENCY_EN_MASK |
		UVD_LMI_CTRL__REQ_MODE_MASK |
		UVD_LMI_CTRL__CRC_RESET_MASK |
		UVD_LMI_CTRL__MASK_MC_URGENT_MASK |
		0x00100000L, 0xFFFFFFFF, 0);

#ifdef __BIG_ENDIAN
	 
	lmi_swap_cntl = 0xa;
#endif
	WREG32_SOC15_DPG_MODE_1_0(UVD, 0, mmUVD_LMI_SWAP_CNTL, lmi_swap_cntl, 0xFFFFFFFF, 0);

	WREG32_SOC15_DPG_MODE_1_0(UVD, 0, mmUVD_MPC_CNTL,
		0x2 << UVD_MPC_CNTL__REPLACEMENT_MODE__SHIFT, 0xFFFFFFFF, 0);

	WREG32_SOC15_DPG_MODE_1_0(UVD, 0, mmUVD_MPC_SET_MUXA0,
		((0x1 << UVD_MPC_SET_MUXA0__VARA_1__SHIFT) |
		 (0x2 << UVD_MPC_SET_MUXA0__VARA_2__SHIFT) |
		 (0x3 << UVD_MPC_SET_MUXA0__VARA_3__SHIFT) |
		 (0x4 << UVD_MPC_SET_MUXA0__VARA_4__SHIFT)), 0xFFFFFFFF, 0);

	WREG32_SOC15_DPG_MODE_1_0(UVD, 0, mmUVD_MPC_SET_MUXB0,
		((0x1 << UVD_MPC_SET_MUXB0__VARB_1__SHIFT) |
		 (0x2 << UVD_MPC_SET_MUXB0__VARB_2__SHIFT) |
		 (0x3 << UVD_MPC_SET_MUXB0__VARB_3__SHIFT) |
		 (0x4 << UVD_MPC_SET_MUXB0__VARB_4__SHIFT)), 0xFFFFFFFF, 0);

	WREG32_SOC15_DPG_MODE_1_0(UVD, 0, mmUVD_MPC_SET_MUX,
		((0x0 << UVD_MPC_SET_MUX__SET_0__SHIFT) |
		 (0x1 << UVD_MPC_SET_MUX__SET_1__SHIFT) |
		 (0x2 << UVD_MPC_SET_MUX__SET_2__SHIFT)), 0xFFFFFFFF, 0);

	vcn_v1_0_mc_resume_dpg_mode(adev);

	WREG32_SOC15_DPG_MODE_1_0(UVD, 0, mmUVD_REG_XX_MASK, 0x10, 0xFFFFFFFF, 0);
	WREG32_SOC15_DPG_MODE_1_0(UVD, 0, mmUVD_RBC_XX_IB_REG_CHECK, 0x3, 0xFFFFFFFF, 0);

	 
	WREG32_SOC15_DPG_MODE_1_0(UVD, 0, mmUVD_SOFT_RESET, 0, 0xFFFFFFFF, 0);

	 
	WREG32_SOC15_DPG_MODE_1_0(UVD, 0, mmUVD_LMI_CTRL2,
		0x1F << UVD_LMI_CTRL2__RE_OFLD_MIF_WR_REQ_NUM__SHIFT,
		0xFFFFFFFF, 0);

	 
	WREG32_SOC15_DPG_MODE_1_0(UVD, 0, mmUVD_MASTINT_EN,
			UVD_MASTINT_EN__VCPU_EN_MASK, UVD_MASTINT_EN__VCPU_EN_MASK, 0);

	vcn_v1_0_clock_gating_dpg_mode(adev, 1);
	 
	WREG32_SOC15_DPG_MODE_1_0(UVD, 0, mmUVD_LMI_CTRL,
		(8 << UVD_LMI_CTRL__WRITE_CLEAN_TIMER__SHIFT) |
		UVD_LMI_CTRL__WRITE_CLEAN_TIMER_EN_MASK |
		UVD_LMI_CTRL__DATA_COHERENCY_EN_MASK |
		UVD_LMI_CTRL__VCPU_DATA_COHERENCY_EN_MASK |
		UVD_LMI_CTRL__REQ_MODE_MASK |
		UVD_LMI_CTRL__CRC_RESET_MASK |
		UVD_LMI_CTRL__MASK_MC_URGENT_MASK |
		0x00100000L, 0xFFFFFFFF, 1);

	tmp = adev->gfx.config.gb_addr_config;
	 
	WREG32_SOC15_DPG_MODE_1_0(UVD, 0, mmUVD_JPEG_ADDR_CONFIG, tmp, 0xFFFFFFFF, 1);
	WREG32_SOC15_DPG_MODE_1_0(UVD, 0, mmUVD_JPEG_UV_ADDR_CONFIG, tmp, 0xFFFFFFFF, 1);

	 
	WREG32_SOC15_DPG_MODE_1_0(UVD, 0, mmUVD_SYS_INT_EN,
									UVD_SYS_INT_EN__UVD_JRBC_EN_MASK, 0xFFFFFFFF, 1);

	 
	rb_bufsz = order_base_2(ring->ring_size);
	tmp = REG_SET_FIELD(0, UVD_RBC_RB_CNTL, RB_BUFSZ, rb_bufsz);
	tmp = REG_SET_FIELD(tmp, UVD_RBC_RB_CNTL, RB_BLKSZ, 1);
	tmp = REG_SET_FIELD(tmp, UVD_RBC_RB_CNTL, RB_NO_FETCH, 1);
	tmp = REG_SET_FIELD(tmp, UVD_RBC_RB_CNTL, RB_NO_UPDATE, 1);
	tmp = REG_SET_FIELD(tmp, UVD_RBC_RB_CNTL, RB_RPTR_WR_EN, 1);
	WREG32_SOC15(UVD, 0, mmUVD_RBC_RB_CNTL, tmp);

	 
	WREG32_SOC15(UVD, 0, mmUVD_RBC_RB_WPTR_CNTL, 0);

	 
	WREG32_SOC15(UVD, 0, mmUVD_RBC_RB_RPTR_ADDR,
								(upper_32_bits(ring->gpu_addr) >> 2));

	 
	WREG32_SOC15(UVD, 0, mmUVD_LMI_RBC_RB_64BIT_BAR_LOW,
								lower_32_bits(ring->gpu_addr));
	WREG32_SOC15(UVD, 0, mmUVD_LMI_RBC_RB_64BIT_BAR_HIGH,
								upper_32_bits(ring->gpu_addr));

	 
	WREG32_SOC15(UVD, 0, mmUVD_RBC_RB_RPTR, 0);

	WREG32_SOC15(UVD, 0, mmUVD_SCRATCH2, 0);

	ring->wptr = RREG32_SOC15(UVD, 0, mmUVD_RBC_RB_RPTR);
	WREG32_SOC15(UVD, 0, mmUVD_RBC_RB_WPTR,
								lower_32_bits(ring->wptr));

	WREG32_P(SOC15_REG_OFFSET(UVD, 0, mmUVD_RBC_RB_CNTL), 0,
			~UVD_RBC_RB_CNTL__RB_NO_FETCH_MASK);

	jpeg_v1_0_start(adev, 1);

	return 0;
}

static int vcn_v1_0_start(struct amdgpu_device *adev)
{
	return (adev->pg_flags & AMD_PG_SUPPORT_VCN_DPG) ?
		vcn_v1_0_start_dpg_mode(adev) : vcn_v1_0_start_spg_mode(adev);
}

 
static int vcn_v1_0_stop_spg_mode(struct amdgpu_device *adev)
{
	int tmp;

	SOC15_WAIT_ON_RREG(UVD, 0, mmUVD_STATUS, UVD_STATUS__IDLE, 0x7);

	tmp = UVD_LMI_STATUS__VCPU_LMI_WRITE_CLEAN_MASK |
		UVD_LMI_STATUS__READ_CLEAN_MASK |
		UVD_LMI_STATUS__WRITE_CLEAN_MASK |
		UVD_LMI_STATUS__WRITE_CLEAN_RAW_MASK;
	SOC15_WAIT_ON_RREG(UVD, 0, mmUVD_LMI_STATUS, tmp, tmp);

	 
	WREG32_P(SOC15_REG_OFFSET(UVD, 0, mmUVD_LMI_CTRL2),
		UVD_LMI_CTRL2__STALL_ARB_UMC_MASK,
		~UVD_LMI_CTRL2__STALL_ARB_UMC_MASK);

	tmp = UVD_LMI_STATUS__UMC_READ_CLEAN_RAW_MASK |
		UVD_LMI_STATUS__UMC_WRITE_CLEAN_RAW_MASK;
	SOC15_WAIT_ON_RREG(UVD, 0, mmUVD_LMI_STATUS, tmp, tmp);

	 
	WREG32_P(SOC15_REG_OFFSET(UVD, 0, mmUVD_VCPU_CNTL), 0,
		~UVD_VCPU_CNTL__CLK_EN_MASK);

	 
	WREG32_P(SOC15_REG_OFFSET(UVD, 0, mmUVD_SOFT_RESET),
		UVD_SOFT_RESET__LMI_UMC_SOFT_RESET_MASK,
		~UVD_SOFT_RESET__LMI_UMC_SOFT_RESET_MASK);

	WREG32_P(SOC15_REG_OFFSET(UVD, 0, mmUVD_SOFT_RESET),
		UVD_SOFT_RESET__LMI_SOFT_RESET_MASK,
		~UVD_SOFT_RESET__LMI_SOFT_RESET_MASK);

	 
	WREG32_P(SOC15_REG_OFFSET(UVD, 0, mmUVD_SOFT_RESET),
		UVD_SOFT_RESET__VCPU_SOFT_RESET_MASK,
		~UVD_SOFT_RESET__VCPU_SOFT_RESET_MASK);

	WREG32_SOC15(UVD, 0, mmUVD_STATUS, 0);

	vcn_v1_0_enable_clock_gating(adev);
	vcn_1_0_enable_static_power_gating(adev);
	return 0;
}

static int vcn_v1_0_stop_dpg_mode(struct amdgpu_device *adev)
{
	uint32_t tmp;

	 
	SOC15_WAIT_ON_RREG(UVD, 0, mmUVD_POWER_STATUS,
			UVD_POWER_STATUS__UVD_POWER_STATUS_TILES_OFF,
			UVD_POWER_STATUS__UVD_POWER_STATUS_MASK);

	 
	tmp = RREG32_SOC15(UVD, 0, mmUVD_RB_WPTR);
	SOC15_WAIT_ON_RREG(UVD, 0, mmUVD_RB_RPTR, tmp, 0xFFFFFFFF);

	tmp = RREG32_SOC15(UVD, 0, mmUVD_RB_WPTR2);
	SOC15_WAIT_ON_RREG(UVD, 0, mmUVD_RB_RPTR2, tmp, 0xFFFFFFFF);

	tmp = RREG32_SOC15(UVD, 0, mmUVD_JRBC_RB_WPTR);
	SOC15_WAIT_ON_RREG(UVD, 0, mmUVD_JRBC_RB_RPTR, tmp, 0xFFFFFFFF);

	tmp = RREG32_SOC15(UVD, 0, mmUVD_RBC_RB_WPTR) & 0x7FFFFFFF;
	SOC15_WAIT_ON_RREG(UVD, 0, mmUVD_RBC_RB_RPTR, tmp, 0xFFFFFFFF);

	SOC15_WAIT_ON_RREG(UVD, 0, mmUVD_POWER_STATUS,
		UVD_POWER_STATUS__UVD_POWER_STATUS_TILES_OFF,
		UVD_POWER_STATUS__UVD_POWER_STATUS_MASK);

	 
	WREG32_P(SOC15_REG_OFFSET(UVD, 0, mmUVD_POWER_STATUS), 0,
			~UVD_POWER_STATUS__UVD_PG_MODE_MASK);

	return 0;
}

static int vcn_v1_0_stop(struct amdgpu_device *adev)
{
	int r;

	if (adev->pg_flags & AMD_PG_SUPPORT_VCN_DPG)
		r = vcn_v1_0_stop_dpg_mode(adev);
	else
		r = vcn_v1_0_stop_spg_mode(adev);

	return r;
}

static int vcn_v1_0_pause_dpg_mode(struct amdgpu_device *adev,
				int inst_idx, struct dpg_pause_state *new_state)
{
	int ret_code;
	uint32_t reg_data = 0;
	uint32_t reg_data2 = 0;
	struct amdgpu_ring *ring;

	 
	if (adev->vcn.inst[inst_idx].pause_state.fw_based != new_state->fw_based) {
		DRM_DEBUG("dpg pause state changed %d:%d -> %d:%d",
			adev->vcn.inst[inst_idx].pause_state.fw_based,
			adev->vcn.inst[inst_idx].pause_state.jpeg,
			new_state->fw_based, new_state->jpeg);

		reg_data = RREG32_SOC15(UVD, 0, mmUVD_DPG_PAUSE) &
			(~UVD_DPG_PAUSE__NJ_PAUSE_DPG_ACK_MASK);

		if (new_state->fw_based == VCN_DPG_STATE__PAUSE) {
			ret_code = 0;

			if (!(reg_data & UVD_DPG_PAUSE__JPEG_PAUSE_DPG_ACK_MASK))
				ret_code = SOC15_WAIT_ON_RREG(UVD, 0, mmUVD_POWER_STATUS,
						   UVD_POWER_STATUS__UVD_POWER_STATUS_TILES_OFF,
						   UVD_POWER_STATUS__UVD_POWER_STATUS_MASK);

			if (!ret_code) {
				 
				reg_data |= UVD_DPG_PAUSE__NJ_PAUSE_DPG_REQ_MASK;
				WREG32_SOC15(UVD, 0, mmUVD_DPG_PAUSE, reg_data);
				SOC15_WAIT_ON_RREG(UVD, 0, mmUVD_DPG_PAUSE,
						   UVD_DPG_PAUSE__NJ_PAUSE_DPG_ACK_MASK,
						   UVD_DPG_PAUSE__NJ_PAUSE_DPG_ACK_MASK);

				 
				ring = &adev->vcn.inst->ring_enc[0];
				WREG32_SOC15(UVD, 0, mmUVD_RB_BASE_LO, ring->gpu_addr);
				WREG32_SOC15(UVD, 0, mmUVD_RB_BASE_HI, upper_32_bits(ring->gpu_addr));
				WREG32_SOC15(UVD, 0, mmUVD_RB_SIZE, ring->ring_size / 4);
				WREG32_SOC15(UVD, 0, mmUVD_RB_RPTR, lower_32_bits(ring->wptr));
				WREG32_SOC15(UVD, 0, mmUVD_RB_WPTR, lower_32_bits(ring->wptr));

				ring = &adev->vcn.inst->ring_enc[1];
				WREG32_SOC15(UVD, 0, mmUVD_RB_BASE_LO2, ring->gpu_addr);
				WREG32_SOC15(UVD, 0, mmUVD_RB_BASE_HI2, upper_32_bits(ring->gpu_addr));
				WREG32_SOC15(UVD, 0, mmUVD_RB_SIZE2, ring->ring_size / 4);
				WREG32_SOC15(UVD, 0, mmUVD_RB_RPTR2, lower_32_bits(ring->wptr));
				WREG32_SOC15(UVD, 0, mmUVD_RB_WPTR2, lower_32_bits(ring->wptr));

				ring = &adev->vcn.inst->ring_dec;
				WREG32_SOC15(UVD, 0, mmUVD_RBC_RB_WPTR,
						   RREG32_SOC15(UVD, 0, mmUVD_SCRATCH2) & 0x7FFFFFFF);
				SOC15_WAIT_ON_RREG(UVD, 0, mmUVD_POWER_STATUS,
						   UVD_PGFSM_CONFIG__UVDM_UVDU_PWR_ON,
						   UVD_POWER_STATUS__UVD_POWER_STATUS_MASK);
			}
		} else {
			 
			reg_data &= ~UVD_DPG_PAUSE__NJ_PAUSE_DPG_REQ_MASK;
			WREG32_SOC15(UVD, 0, mmUVD_DPG_PAUSE, reg_data);
		}
		adev->vcn.inst[inst_idx].pause_state.fw_based = new_state->fw_based;
	}

	 
	if (adev->vcn.inst[inst_idx].pause_state.jpeg != new_state->jpeg) {
		DRM_DEBUG("dpg pause state changed %d:%d -> %d:%d",
			adev->vcn.inst[inst_idx].pause_state.fw_based,
			adev->vcn.inst[inst_idx].pause_state.jpeg,
			new_state->fw_based, new_state->jpeg);

		reg_data = RREG32_SOC15(UVD, 0, mmUVD_DPG_PAUSE) &
			(~UVD_DPG_PAUSE__JPEG_PAUSE_DPG_ACK_MASK);

		if (new_state->jpeg == VCN_DPG_STATE__PAUSE) {
			ret_code = 0;

			if (!(reg_data & UVD_DPG_PAUSE__NJ_PAUSE_DPG_ACK_MASK))
				ret_code = SOC15_WAIT_ON_RREG(UVD, 0, mmUVD_POWER_STATUS,
						   UVD_POWER_STATUS__UVD_POWER_STATUS_TILES_OFF,
						   UVD_POWER_STATUS__UVD_POWER_STATUS_MASK);

			if (!ret_code) {
				 
				reg_data2 = RREG32_SOC15(UVD, 0, mmUVD_POWER_STATUS);
				reg_data2 |= UVD_POWER_STATUS__JRBC_SNOOP_DIS_MASK;
				WREG32_SOC15(UVD, 0, mmUVD_POWER_STATUS, reg_data2);

				 
				reg_data |= UVD_DPG_PAUSE__JPEG_PAUSE_DPG_REQ_MASK;
				WREG32_SOC15(UVD, 0, mmUVD_DPG_PAUSE, reg_data);
				SOC15_WAIT_ON_RREG(UVD, 0, mmUVD_DPG_PAUSE,
							UVD_DPG_PAUSE__JPEG_PAUSE_DPG_ACK_MASK,
							UVD_DPG_PAUSE__JPEG_PAUSE_DPG_ACK_MASK);

				 
				ring = adev->jpeg.inst->ring_dec;
				WREG32_SOC15(UVD, 0, mmUVD_LMI_JRBC_RB_VMID, 0);
				WREG32_SOC15(UVD, 0, mmUVD_JRBC_RB_CNTL,
							UVD_JRBC_RB_CNTL__RB_NO_FETCH_MASK |
							UVD_JRBC_RB_CNTL__RB_RPTR_WR_EN_MASK);
				WREG32_SOC15(UVD, 0, mmUVD_LMI_JRBC_RB_64BIT_BAR_LOW,
							lower_32_bits(ring->gpu_addr));
				WREG32_SOC15(UVD, 0, mmUVD_LMI_JRBC_RB_64BIT_BAR_HIGH,
							upper_32_bits(ring->gpu_addr));
				WREG32_SOC15(UVD, 0, mmUVD_JRBC_RB_RPTR, ring->wptr);
				WREG32_SOC15(UVD, 0, mmUVD_JRBC_RB_WPTR, ring->wptr);
				WREG32_SOC15(UVD, 0, mmUVD_JRBC_RB_CNTL,
							UVD_JRBC_RB_CNTL__RB_RPTR_WR_EN_MASK);

				ring = &adev->vcn.inst->ring_dec;
				WREG32_SOC15(UVD, 0, mmUVD_RBC_RB_WPTR,
						   RREG32_SOC15(UVD, 0, mmUVD_SCRATCH2) & 0x7FFFFFFF);
				SOC15_WAIT_ON_RREG(UVD, 0, mmUVD_POWER_STATUS,
						   UVD_PGFSM_CONFIG__UVDM_UVDU_PWR_ON,
						   UVD_POWER_STATUS__UVD_POWER_STATUS_MASK);
			}
		} else {
			 
			reg_data &= ~UVD_DPG_PAUSE__JPEG_PAUSE_DPG_REQ_MASK;
			WREG32_SOC15(UVD, 0, mmUVD_DPG_PAUSE, reg_data);
		}
		adev->vcn.inst[inst_idx].pause_state.jpeg = new_state->jpeg;
	}

	return 0;
}

static bool vcn_v1_0_is_idle(void *handle)
{
	struct amdgpu_device *adev = (struct amdgpu_device *)handle;

	return (RREG32_SOC15(VCN, 0, mmUVD_STATUS) == UVD_STATUS__IDLE);
}

static int vcn_v1_0_wait_for_idle(void *handle)
{
	struct amdgpu_device *adev = (struct amdgpu_device *)handle;
	int ret;

	ret = SOC15_WAIT_ON_RREG(VCN, 0, mmUVD_STATUS, UVD_STATUS__IDLE,
		UVD_STATUS__IDLE);

	return ret;
}

static int vcn_v1_0_set_clockgating_state(void *handle,
					  enum amd_clockgating_state state)
{
	struct amdgpu_device *adev = (struct amdgpu_device *)handle;
	bool enable = (state == AMD_CG_STATE_GATE);

	if (enable) {
		 
		if (!vcn_v1_0_is_idle(handle))
			return -EBUSY;
		vcn_v1_0_enable_clock_gating(adev);
	} else {
		 
		vcn_v1_0_disable_clock_gating(adev);
	}
	return 0;
}

 
static uint64_t vcn_v1_0_dec_ring_get_rptr(struct amdgpu_ring *ring)
{
	struct amdgpu_device *adev = ring->adev;

	return RREG32_SOC15(UVD, 0, mmUVD_RBC_RB_RPTR);
}

 
static uint64_t vcn_v1_0_dec_ring_get_wptr(struct amdgpu_ring *ring)
{
	struct amdgpu_device *adev = ring->adev;

	return RREG32_SOC15(UVD, 0, mmUVD_RBC_RB_WPTR);
}

 
static void vcn_v1_0_dec_ring_set_wptr(struct amdgpu_ring *ring)
{
	struct amdgpu_device *adev = ring->adev;

	if (adev->pg_flags & AMD_PG_SUPPORT_VCN_DPG)
		WREG32_SOC15(UVD, 0, mmUVD_SCRATCH2,
			lower_32_bits(ring->wptr) | 0x80000000);

	WREG32_SOC15(UVD, 0, mmUVD_RBC_RB_WPTR, lower_32_bits(ring->wptr));
}

 
static void vcn_v1_0_dec_ring_insert_start(struct amdgpu_ring *ring)
{
	struct amdgpu_device *adev = ring->adev;

	amdgpu_ring_write(ring,
		PACKET0(SOC15_REG_OFFSET(UVD, 0, mmUVD_GPCOM_VCPU_DATA0), 0));
	amdgpu_ring_write(ring, 0);
	amdgpu_ring_write(ring,
		PACKET0(SOC15_REG_OFFSET(UVD, 0, mmUVD_GPCOM_VCPU_CMD), 0));
	amdgpu_ring_write(ring, VCN_DEC_CMD_PACKET_START << 1);
}

 
static void vcn_v1_0_dec_ring_insert_end(struct amdgpu_ring *ring)
{
	struct amdgpu_device *adev = ring->adev;

	amdgpu_ring_write(ring,
		PACKET0(SOC15_REG_OFFSET(UVD, 0, mmUVD_GPCOM_VCPU_CMD), 0));
	amdgpu_ring_write(ring, VCN_DEC_CMD_PACKET_END << 1);
}

 
static void vcn_v1_0_dec_ring_emit_fence(struct amdgpu_ring *ring, u64 addr, u64 seq,
				     unsigned flags)
{
	struct amdgpu_device *adev = ring->adev;

	WARN_ON(flags & AMDGPU_FENCE_FLAG_64BIT);

	amdgpu_ring_write(ring,
		PACKET0(SOC15_REG_OFFSET(UVD, 0, mmUVD_CONTEXT_ID), 0));
	amdgpu_ring_write(ring, seq);
	amdgpu_ring_write(ring,
		PACKET0(SOC15_REG_OFFSET(UVD, 0, mmUVD_GPCOM_VCPU_DATA0), 0));
	amdgpu_ring_write(ring, addr & 0xffffffff);
	amdgpu_ring_write(ring,
		PACKET0(SOC15_REG_OFFSET(UVD, 0, mmUVD_GPCOM_VCPU_DATA1), 0));
	amdgpu_ring_write(ring, upper_32_bits(addr) & 0xff);
	amdgpu_ring_write(ring,
		PACKET0(SOC15_REG_OFFSET(UVD, 0, mmUVD_GPCOM_VCPU_CMD), 0));
	amdgpu_ring_write(ring, VCN_DEC_CMD_FENCE << 1);

	amdgpu_ring_write(ring,
		PACKET0(SOC15_REG_OFFSET(UVD, 0, mmUVD_GPCOM_VCPU_DATA0), 0));
	amdgpu_ring_write(ring, 0);
	amdgpu_ring_write(ring,
		PACKET0(SOC15_REG_OFFSET(UVD, 0, mmUVD_GPCOM_VCPU_DATA1), 0));
	amdgpu_ring_write(ring, 0);
	amdgpu_ring_write(ring,
		PACKET0(SOC15_REG_OFFSET(UVD, 0, mmUVD_GPCOM_VCPU_CMD), 0));
	amdgpu_ring_write(ring, VCN_DEC_CMD_TRAP << 1);
}

 
static void vcn_v1_0_dec_ring_emit_ib(struct amdgpu_ring *ring,
					struct amdgpu_job *job,
					struct amdgpu_ib *ib,
					uint32_t flags)
{
	struct amdgpu_device *adev = ring->adev;
	unsigned vmid = AMDGPU_JOB_GET_VMID(job);

	amdgpu_ring_write(ring,
		PACKET0(SOC15_REG_OFFSET(UVD, 0, mmUVD_LMI_RBC_IB_VMID), 0));
	amdgpu_ring_write(ring, vmid);

	amdgpu_ring_write(ring,
		PACKET0(SOC15_REG_OFFSET(UVD, 0, mmUVD_LMI_RBC_IB_64BIT_BAR_LOW), 0));
	amdgpu_ring_write(ring, lower_32_bits(ib->gpu_addr));
	amdgpu_ring_write(ring,
		PACKET0(SOC15_REG_OFFSET(UVD, 0, mmUVD_LMI_RBC_IB_64BIT_BAR_HIGH), 0));
	amdgpu_ring_write(ring, upper_32_bits(ib->gpu_addr));
	amdgpu_ring_write(ring,
		PACKET0(SOC15_REG_OFFSET(UVD, 0, mmUVD_RBC_IB_SIZE), 0));
	amdgpu_ring_write(ring, ib->length_dw);
}

static void vcn_v1_0_dec_ring_emit_reg_wait(struct amdgpu_ring *ring,
					    uint32_t reg, uint32_t val,
					    uint32_t mask)
{
	struct amdgpu_device *adev = ring->adev;

	amdgpu_ring_write(ring,
		PACKET0(SOC15_REG_OFFSET(UVD, 0, mmUVD_GPCOM_VCPU_DATA0), 0));
	amdgpu_ring_write(ring, reg << 2);
	amdgpu_ring_write(ring,
		PACKET0(SOC15_REG_OFFSET(UVD, 0, mmUVD_GPCOM_VCPU_DATA1), 0));
	amdgpu_ring_write(ring, val);
	amdgpu_ring_write(ring,
		PACKET0(SOC15_REG_OFFSET(UVD, 0, mmUVD_GP_SCRATCH8), 0));
	amdgpu_ring_write(ring, mask);
	amdgpu_ring_write(ring,
		PACKET0(SOC15_REG_OFFSET(UVD, 0, mmUVD_GPCOM_VCPU_CMD), 0));
	amdgpu_ring_write(ring, VCN_DEC_CMD_REG_READ_COND_WAIT << 1);
}

static void vcn_v1_0_dec_ring_emit_vm_flush(struct amdgpu_ring *ring,
					    unsigned vmid, uint64_t pd_addr)
{
	struct amdgpu_vmhub *hub = &ring->adev->vmhub[ring->vm_hub];
	uint32_t data0, data1, mask;

	pd_addr = amdgpu_gmc_emit_flush_gpu_tlb(ring, vmid, pd_addr);

	 
	data0 = hub->ctx0_ptb_addr_lo32 + vmid * hub->ctx_addr_distance;
	data1 = lower_32_bits(pd_addr);
	mask = 0xffffffff;
	vcn_v1_0_dec_ring_emit_reg_wait(ring, data0, data1, mask);
}

static void vcn_v1_0_dec_ring_emit_wreg(struct amdgpu_ring *ring,
					uint32_t reg, uint32_t val)
{
	struct amdgpu_device *adev = ring->adev;

	amdgpu_ring_write(ring,
		PACKET0(SOC15_REG_OFFSET(UVD, 0, mmUVD_GPCOM_VCPU_DATA0), 0));
	amdgpu_ring_write(ring, reg << 2);
	amdgpu_ring_write(ring,
		PACKET0(SOC15_REG_OFFSET(UVD, 0, mmUVD_GPCOM_VCPU_DATA1), 0));
	amdgpu_ring_write(ring, val);
	amdgpu_ring_write(ring,
		PACKET0(SOC15_REG_OFFSET(UVD, 0, mmUVD_GPCOM_VCPU_CMD), 0));
	amdgpu_ring_write(ring, VCN_DEC_CMD_WRITE_REG << 1);
}

 
static uint64_t vcn_v1_0_enc_ring_get_rptr(struct amdgpu_ring *ring)
{
	struct amdgpu_device *adev = ring->adev;

	if (ring == &adev->vcn.inst->ring_enc[0])
		return RREG32_SOC15(UVD, 0, mmUVD_RB_RPTR);
	else
		return RREG32_SOC15(UVD, 0, mmUVD_RB_RPTR2);
}

  
static uint64_t vcn_v1_0_enc_ring_get_wptr(struct amdgpu_ring *ring)
{
	struct amdgpu_device *adev = ring->adev;

	if (ring == &adev->vcn.inst->ring_enc[0])
		return RREG32_SOC15(UVD, 0, mmUVD_RB_WPTR);
	else
		return RREG32_SOC15(UVD, 0, mmUVD_RB_WPTR2);
}

  
static void vcn_v1_0_enc_ring_set_wptr(struct amdgpu_ring *ring)
{
	struct amdgpu_device *adev = ring->adev;

	if (ring == &adev->vcn.inst->ring_enc[0])
		WREG32_SOC15(UVD, 0, mmUVD_RB_WPTR,
			lower_32_bits(ring->wptr));
	else
		WREG32_SOC15(UVD, 0, mmUVD_RB_WPTR2,
			lower_32_bits(ring->wptr));
}

 
static void vcn_v1_0_enc_ring_emit_fence(struct amdgpu_ring *ring, u64 addr,
			u64 seq, unsigned flags)
{
	WARN_ON(flags & AMDGPU_FENCE_FLAG_64BIT);

	amdgpu_ring_write(ring, VCN_ENC_CMD_FENCE);
	amdgpu_ring_write(ring, addr);
	amdgpu_ring_write(ring, upper_32_bits(addr));
	amdgpu_ring_write(ring, seq);
	amdgpu_ring_write(ring, VCN_ENC_CMD_TRAP);
}

static void vcn_v1_0_enc_ring_insert_end(struct amdgpu_ring *ring)
{
	amdgpu_ring_write(ring, VCN_ENC_CMD_END);
}

 
static void vcn_v1_0_enc_ring_emit_ib(struct amdgpu_ring *ring,
					struct amdgpu_job *job,
					struct amdgpu_ib *ib,
					uint32_t flags)
{
	unsigned vmid = AMDGPU_JOB_GET_VMID(job);

	amdgpu_ring_write(ring, VCN_ENC_CMD_IB);
	amdgpu_ring_write(ring, vmid);
	amdgpu_ring_write(ring, lower_32_bits(ib->gpu_addr));
	amdgpu_ring_write(ring, upper_32_bits(ib->gpu_addr));
	amdgpu_ring_write(ring, ib->length_dw);
}

static void vcn_v1_0_enc_ring_emit_reg_wait(struct amdgpu_ring *ring,
					    uint32_t reg, uint32_t val,
					    uint32_t mask)
{
	amdgpu_ring_write(ring, VCN_ENC_CMD_REG_WAIT);
	amdgpu_ring_write(ring, reg << 2);
	amdgpu_ring_write(ring, mask);
	amdgpu_ring_write(ring, val);
}

static void vcn_v1_0_enc_ring_emit_vm_flush(struct amdgpu_ring *ring,
					    unsigned int vmid, uint64_t pd_addr)
{
	struct amdgpu_vmhub *hub = &ring->adev->vmhub[ring->vm_hub];

	pd_addr = amdgpu_gmc_emit_flush_gpu_tlb(ring, vmid, pd_addr);

	 
	vcn_v1_0_enc_ring_emit_reg_wait(ring, hub->ctx0_ptb_addr_lo32 +
					vmid * hub->ctx_addr_distance,
					lower_32_bits(pd_addr), 0xffffffff);
}

static void vcn_v1_0_enc_ring_emit_wreg(struct amdgpu_ring *ring,
					uint32_t reg, uint32_t val)
{
	amdgpu_ring_write(ring, VCN_ENC_CMD_REG_WRITE);
	amdgpu_ring_write(ring,	reg << 2);
	amdgpu_ring_write(ring, val);
}

static int vcn_v1_0_set_interrupt_state(struct amdgpu_device *adev,
					struct amdgpu_irq_src *source,
					unsigned type,
					enum amdgpu_interrupt_state state)
{
	return 0;
}

static int vcn_v1_0_process_interrupt(struct amdgpu_device *adev,
				      struct amdgpu_irq_src *source,
				      struct amdgpu_iv_entry *entry)
{
	DRM_DEBUG("IH: VCN TRAP\n");

	switch (entry->src_id) {
	case 124:
		amdgpu_fence_process(&adev->vcn.inst->ring_dec);
		break;
	case 119:
		amdgpu_fence_process(&adev->vcn.inst->ring_enc[0]);
		break;
	case 120:
		amdgpu_fence_process(&adev->vcn.inst->ring_enc[1]);
		break;
	default:
		DRM_ERROR("Unhandled interrupt: %d %d\n",
			  entry->src_id, entry->src_data[0]);
		break;
	}

	return 0;
}

static void vcn_v1_0_dec_ring_insert_nop(struct amdgpu_ring *ring, uint32_t count)
{
	struct amdgpu_device *adev = ring->adev;
	int i;

	WARN_ON(ring->wptr % 2 || count % 2);

	for (i = 0; i < count / 2; i++) {
		amdgpu_ring_write(ring, PACKET0(SOC15_REG_OFFSET(UVD, 0, mmUVD_NO_OP), 0));
		amdgpu_ring_write(ring, 0);
	}
}

static int vcn_v1_0_set_powergating_state(void *handle,
					  enum amd_powergating_state state)
{
	 
	int ret;
	struct amdgpu_device *adev = (struct amdgpu_device *)handle;

	if (state == adev->vcn.cur_state)
		return 0;

	if (state == AMD_PG_STATE_GATE)
		ret = vcn_v1_0_stop(adev);
	else
		ret = vcn_v1_0_start(adev);

	if (!ret)
		adev->vcn.cur_state = state;
	return ret;
}

static void vcn_v1_0_idle_work_handler(struct work_struct *work)
{
	struct amdgpu_device *adev =
		container_of(work, struct amdgpu_device, vcn.idle_work.work);
	unsigned int fences = 0, i;

	for (i = 0; i < adev->vcn.num_enc_rings; ++i)
		fences += amdgpu_fence_count_emitted(&adev->vcn.inst->ring_enc[i]);

	if (adev->pg_flags & AMD_PG_SUPPORT_VCN_DPG) {
		struct dpg_pause_state new_state;

		if (fences)
			new_state.fw_based = VCN_DPG_STATE__PAUSE;
		else
			new_state.fw_based = VCN_DPG_STATE__UNPAUSE;

		if (amdgpu_fence_count_emitted(adev->jpeg.inst->ring_dec))
			new_state.jpeg = VCN_DPG_STATE__PAUSE;
		else
			new_state.jpeg = VCN_DPG_STATE__UNPAUSE;

		adev->vcn.pause_dpg_mode(adev, 0, &new_state);
	}

	fences += amdgpu_fence_count_emitted(adev->jpeg.inst->ring_dec);
	fences += amdgpu_fence_count_emitted(&adev->vcn.inst->ring_dec);

	if (fences == 0) {
		amdgpu_gfx_off_ctrl(adev, true);
		if (adev->pm.dpm_enabled)
			amdgpu_dpm_enable_uvd(adev, false);
		else
			amdgpu_device_ip_set_powergating_state(adev, AMD_IP_BLOCK_TYPE_VCN,
			       AMD_PG_STATE_GATE);
	} else {
		schedule_delayed_work(&adev->vcn.idle_work, VCN_IDLE_TIMEOUT);
	}
}

static void vcn_v1_0_ring_begin_use(struct amdgpu_ring *ring)
{
	struct	amdgpu_device *adev = ring->adev;
	bool set_clocks = !cancel_delayed_work_sync(&adev->vcn.idle_work);

	mutex_lock(&adev->vcn.vcn1_jpeg1_workaround);

	if (amdgpu_fence_wait_empty(ring->adev->jpeg.inst->ring_dec))
		DRM_ERROR("VCN dec: jpeg dec ring may not be empty\n");

	vcn_v1_0_set_pg_for_begin_use(ring, set_clocks);

}

void vcn_v1_0_set_pg_for_begin_use(struct amdgpu_ring *ring, bool set_clocks)
{
	struct amdgpu_device *adev = ring->adev;

	if (set_clocks) {
		amdgpu_gfx_off_ctrl(adev, false);
		if (adev->pm.dpm_enabled)
			amdgpu_dpm_enable_uvd(adev, true);
		else
			amdgpu_device_ip_set_powergating_state(adev, AMD_IP_BLOCK_TYPE_VCN,
			       AMD_PG_STATE_UNGATE);
	}

	if (adev->pg_flags & AMD_PG_SUPPORT_VCN_DPG) {
		struct dpg_pause_state new_state;
		unsigned int fences = 0, i;

		for (i = 0; i < adev->vcn.num_enc_rings; ++i)
			fences += amdgpu_fence_count_emitted(&adev->vcn.inst->ring_enc[i]);

		if (fences)
			new_state.fw_based = VCN_DPG_STATE__PAUSE;
		else
			new_state.fw_based = VCN_DPG_STATE__UNPAUSE;

		if (amdgpu_fence_count_emitted(adev->jpeg.inst->ring_dec))
			new_state.jpeg = VCN_DPG_STATE__PAUSE;
		else
			new_state.jpeg = VCN_DPG_STATE__UNPAUSE;

		if (ring->funcs->type == AMDGPU_RING_TYPE_VCN_ENC)
			new_state.fw_based = VCN_DPG_STATE__PAUSE;
		else if (ring->funcs->type == AMDGPU_RING_TYPE_VCN_JPEG)
			new_state.jpeg = VCN_DPG_STATE__PAUSE;

		adev->vcn.pause_dpg_mode(adev, 0, &new_state);
	}
}

void vcn_v1_0_ring_end_use(struct amdgpu_ring *ring)
{
	schedule_delayed_work(&ring->adev->vcn.idle_work, VCN_IDLE_TIMEOUT);
	mutex_unlock(&ring->adev->vcn.vcn1_jpeg1_workaround);
}

static const struct amd_ip_funcs vcn_v1_0_ip_funcs = {
	.name = "vcn_v1_0",
	.early_init = vcn_v1_0_early_init,
	.late_init = NULL,
	.sw_init = vcn_v1_0_sw_init,
	.sw_fini = vcn_v1_0_sw_fini,
	.hw_init = vcn_v1_0_hw_init,
	.hw_fini = vcn_v1_0_hw_fini,
	.suspend = vcn_v1_0_suspend,
	.resume = vcn_v1_0_resume,
	.is_idle = vcn_v1_0_is_idle,
	.wait_for_idle = vcn_v1_0_wait_for_idle,
	.check_soft_reset = NULL  ,
	.pre_soft_reset = NULL  ,
	.soft_reset = NULL  ,
	.post_soft_reset = NULL  ,
	.set_clockgating_state = vcn_v1_0_set_clockgating_state,
	.set_powergating_state = vcn_v1_0_set_powergating_state,
};

 
static int vcn_v1_0_validate_bo(struct amdgpu_cs_parser *parser,
				struct amdgpu_job *job,
				uint64_t addr)
{
	struct ttm_operation_ctx ctx = { false, false };
	struct amdgpu_fpriv *fpriv = parser->filp->driver_priv;
	struct amdgpu_vm *vm = &fpriv->vm;
	struct amdgpu_bo_va_mapping *mapping;
	struct amdgpu_bo *bo;
	int r;

	addr &= AMDGPU_GMC_HOLE_MASK;
	if (addr & 0x7) {
		DRM_ERROR("VCN messages must be 8 byte aligned!\n");
		return -EINVAL;
	}

	mapping = amdgpu_vm_bo_lookup_mapping(vm, addr/AMDGPU_GPU_PAGE_SIZE);
	if (!mapping || !mapping->bo_va || !mapping->bo_va->base.bo)
		return -EINVAL;

	bo = mapping->bo_va->base.bo;
	if (!(bo->flags & AMDGPU_GEM_CREATE_ENCRYPTED))
		return 0;

	amdgpu_bo_placement_from_domain(bo, AMDGPU_GEM_DOMAIN_VRAM);
	r = ttm_bo_validate(&bo->tbo, &bo->placement, &ctx);
	if (r) {
		DRM_ERROR("Failed to validate the VCN message BO (%d)!\n", r);
		return r;
	}

	return r;
}

static int vcn_v1_0_ring_patch_cs_in_place(struct amdgpu_cs_parser *p,
					   struct amdgpu_job *job,
					   struct amdgpu_ib *ib)
{
	uint32_t msg_lo = 0, msg_hi = 0;
	int i, r;

	if (!(ib->flags & AMDGPU_IB_FLAGS_SECURE))
		return 0;

	for (i = 0; i < ib->length_dw; i += 2) {
		uint32_t reg = amdgpu_ib_get_value(ib, i);
		uint32_t val = amdgpu_ib_get_value(ib, i + 1);

		if (reg == PACKET0(p->adev->vcn.internal.data0, 0)) {
			msg_lo = val;
		} else if (reg == PACKET0(p->adev->vcn.internal.data1, 0)) {
			msg_hi = val;
		} else if (reg == PACKET0(p->adev->vcn.internal.cmd, 0)) {
			r = vcn_v1_0_validate_bo(p, job,
						 ((u64)msg_hi) << 32 | msg_lo);
			if (r)
				return r;
		}
	}

	return 0;
}

static const struct amdgpu_ring_funcs vcn_v1_0_dec_ring_vm_funcs = {
	.type = AMDGPU_RING_TYPE_VCN_DEC,
	.align_mask = 0xf,
	.support_64bit_ptrs = false,
	.no_user_fence = true,
	.secure_submission_supported = true,
	.get_rptr = vcn_v1_0_dec_ring_get_rptr,
	.get_wptr = vcn_v1_0_dec_ring_get_wptr,
	.set_wptr = vcn_v1_0_dec_ring_set_wptr,
	.patch_cs_in_place = vcn_v1_0_ring_patch_cs_in_place,
	.emit_frame_size =
		6 + 6 +  
		SOC15_FLUSH_GPU_TLB_NUM_WREG * 6 +
		SOC15_FLUSH_GPU_TLB_NUM_REG_WAIT * 8 +
		8 +  
		14 + 14 +  
		6,
	.emit_ib_size = 8,  
	.emit_ib = vcn_v1_0_dec_ring_emit_ib,
	.emit_fence = vcn_v1_0_dec_ring_emit_fence,
	.emit_vm_flush = vcn_v1_0_dec_ring_emit_vm_flush,
	.test_ring = amdgpu_vcn_dec_ring_test_ring,
	.test_ib = amdgpu_vcn_dec_ring_test_ib,
	.insert_nop = vcn_v1_0_dec_ring_insert_nop,
	.insert_start = vcn_v1_0_dec_ring_insert_start,
	.insert_end = vcn_v1_0_dec_ring_insert_end,
	.pad_ib = amdgpu_ring_generic_pad_ib,
	.begin_use = vcn_v1_0_ring_begin_use,
	.end_use = vcn_v1_0_ring_end_use,
	.emit_wreg = vcn_v1_0_dec_ring_emit_wreg,
	.emit_reg_wait = vcn_v1_0_dec_ring_emit_reg_wait,
	.emit_reg_write_reg_wait = amdgpu_ring_emit_reg_write_reg_wait_helper,
};

static const struct amdgpu_ring_funcs vcn_v1_0_enc_ring_vm_funcs = {
	.type = AMDGPU_RING_TYPE_VCN_ENC,
	.align_mask = 0x3f,
	.nop = VCN_ENC_CMD_NO_OP,
	.support_64bit_ptrs = false,
	.no_user_fence = true,
	.get_rptr = vcn_v1_0_enc_ring_get_rptr,
	.get_wptr = vcn_v1_0_enc_ring_get_wptr,
	.set_wptr = vcn_v1_0_enc_ring_set_wptr,
	.emit_frame_size =
		SOC15_FLUSH_GPU_TLB_NUM_WREG * 3 +
		SOC15_FLUSH_GPU_TLB_NUM_REG_WAIT * 4 +
		4 +  
		5 + 5 +  
		1,  
	.emit_ib_size = 5,  
	.emit_ib = vcn_v1_0_enc_ring_emit_ib,
	.emit_fence = vcn_v1_0_enc_ring_emit_fence,
	.emit_vm_flush = vcn_v1_0_enc_ring_emit_vm_flush,
	.test_ring = amdgpu_vcn_enc_ring_test_ring,
	.test_ib = amdgpu_vcn_enc_ring_test_ib,
	.insert_nop = amdgpu_ring_insert_nop,
	.insert_end = vcn_v1_0_enc_ring_insert_end,
	.pad_ib = amdgpu_ring_generic_pad_ib,
	.begin_use = vcn_v1_0_ring_begin_use,
	.end_use = vcn_v1_0_ring_end_use,
	.emit_wreg = vcn_v1_0_enc_ring_emit_wreg,
	.emit_reg_wait = vcn_v1_0_enc_ring_emit_reg_wait,
	.emit_reg_write_reg_wait = amdgpu_ring_emit_reg_write_reg_wait_helper,
};

static void vcn_v1_0_set_dec_ring_funcs(struct amdgpu_device *adev)
{
	adev->vcn.inst->ring_dec.funcs = &vcn_v1_0_dec_ring_vm_funcs;
	DRM_INFO("VCN decode is enabled in VM mode\n");
}

static void vcn_v1_0_set_enc_ring_funcs(struct amdgpu_device *adev)
{
	int i;

	for (i = 0; i < adev->vcn.num_enc_rings; ++i)
		adev->vcn.inst->ring_enc[i].funcs = &vcn_v1_0_enc_ring_vm_funcs;

	DRM_INFO("VCN encode is enabled in VM mode\n");
}

static const struct amdgpu_irq_src_funcs vcn_v1_0_irq_funcs = {
	.set = vcn_v1_0_set_interrupt_state,
	.process = vcn_v1_0_process_interrupt,
};

static void vcn_v1_0_set_irq_funcs(struct amdgpu_device *adev)
{
	adev->vcn.inst->irq.num_types = adev->vcn.num_enc_rings + 2;
	adev->vcn.inst->irq.funcs = &vcn_v1_0_irq_funcs;
}

const struct amdgpu_ip_block_version vcn_v1_0_ip_block = {
		.type = AMD_IP_BLOCK_TYPE_VCN,
		.major = 1,
		.minor = 0,
		.rev = 0,
		.funcs = &vcn_v1_0_ip_funcs,
};
