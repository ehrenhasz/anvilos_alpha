 

#include <linux/pci.h>

#include "amdgpu.h"
#include "amdgpu_ih.h"
#include "soc15.h"

#include "oss/osssys_4_2_0_offset.h"
#include "oss/osssys_4_2_0_sh_mask.h"

#include "soc15_common.h"
#include "vega20_ih.h"

#define MAX_REARM_RETRY 10

#define mmIH_CHICKEN_ALDEBARAN			0x18d
#define mmIH_CHICKEN_ALDEBARAN_BASE_IDX		0

#define mmIH_RETRY_INT_CAM_CNTL_ALDEBARAN		0x00ea
#define mmIH_RETRY_INT_CAM_CNTL_ALDEBARAN_BASE_IDX	0
#define IH_RETRY_INT_CAM_CNTL_ALDEBARAN__ENABLE__SHIFT	0x10
#define IH_RETRY_INT_CAM_CNTL_ALDEBARAN__ENABLE_MASK	0x00010000L

static void vega20_ih_set_interrupt_funcs(struct amdgpu_device *adev);

 
static void vega20_ih_init_register_offset(struct amdgpu_device *adev)
{
	struct amdgpu_ih_regs *ih_regs;

	if (adev->irq.ih.ring_size) {
		ih_regs = &adev->irq.ih.ih_regs;
		ih_regs->ih_rb_base = SOC15_REG_OFFSET(OSSSYS, 0, mmIH_RB_BASE);
		ih_regs->ih_rb_base_hi = SOC15_REG_OFFSET(OSSSYS, 0, mmIH_RB_BASE_HI);
		ih_regs->ih_rb_cntl = SOC15_REG_OFFSET(OSSSYS, 0, mmIH_RB_CNTL);
		ih_regs->ih_rb_wptr = SOC15_REG_OFFSET(OSSSYS, 0, mmIH_RB_WPTR);
		ih_regs->ih_rb_rptr = SOC15_REG_OFFSET(OSSSYS, 0, mmIH_RB_RPTR);
		ih_regs->ih_doorbell_rptr = SOC15_REG_OFFSET(OSSSYS, 0, mmIH_DOORBELL_RPTR);
		ih_regs->ih_rb_wptr_addr_lo = SOC15_REG_OFFSET(OSSSYS, 0, mmIH_RB_WPTR_ADDR_LO);
		ih_regs->ih_rb_wptr_addr_hi = SOC15_REG_OFFSET(OSSSYS, 0, mmIH_RB_WPTR_ADDR_HI);
		ih_regs->psp_reg_id = PSP_REG_IH_RB_CNTL;
	}

	if (adev->irq.ih1.ring_size) {
		ih_regs = &adev->irq.ih1.ih_regs;
		ih_regs->ih_rb_base = SOC15_REG_OFFSET(OSSSYS, 0, mmIH_RB_BASE_RING1);
		ih_regs->ih_rb_base_hi = SOC15_REG_OFFSET(OSSSYS, 0, mmIH_RB_BASE_HI_RING1);
		ih_regs->ih_rb_cntl = SOC15_REG_OFFSET(OSSSYS, 0, mmIH_RB_CNTL_RING1);
		ih_regs->ih_rb_wptr = SOC15_REG_OFFSET(OSSSYS, 0, mmIH_RB_WPTR_RING1);
		ih_regs->ih_rb_rptr = SOC15_REG_OFFSET(OSSSYS, 0, mmIH_RB_RPTR_RING1);
		ih_regs->ih_doorbell_rptr = SOC15_REG_OFFSET(OSSSYS, 0, mmIH_DOORBELL_RPTR_RING1);
		ih_regs->psp_reg_id = PSP_REG_IH_RB_CNTL_RING1;
	}

	if (adev->irq.ih2.ring_size) {
		ih_regs = &adev->irq.ih2.ih_regs;
		ih_regs->ih_rb_base = SOC15_REG_OFFSET(OSSSYS, 0, mmIH_RB_BASE_RING2);
		ih_regs->ih_rb_base_hi = SOC15_REG_OFFSET(OSSSYS, 0, mmIH_RB_BASE_HI_RING2);
		ih_regs->ih_rb_cntl = SOC15_REG_OFFSET(OSSSYS, 0, mmIH_RB_CNTL_RING2);
		ih_regs->ih_rb_wptr = SOC15_REG_OFFSET(OSSSYS, 0, mmIH_RB_WPTR_RING2);
		ih_regs->ih_rb_rptr = SOC15_REG_OFFSET(OSSSYS, 0, mmIH_RB_RPTR_RING2);
		ih_regs->ih_doorbell_rptr = SOC15_REG_OFFSET(OSSSYS, 0, mmIH_DOORBELL_RPTR_RING2);
		ih_regs->psp_reg_id = PSP_REG_IH_RB_CNTL_RING2;
	}
}

 
static int vega20_ih_toggle_ring_interrupts(struct amdgpu_device *adev,
					    struct amdgpu_ih_ring *ih,
					    bool enable)
{
	struct amdgpu_ih_regs *ih_regs;
	uint32_t tmp;

	ih_regs = &ih->ih_regs;

	tmp = RREG32(ih_regs->ih_rb_cntl);
	tmp = REG_SET_FIELD(tmp, IH_RB_CNTL, RB_ENABLE, (enable ? 1 : 0));
	tmp = REG_SET_FIELD(tmp, IH_RB_CNTL, RB_GPU_TS_ENABLE, 1);

	 
	if (ih == &adev->irq.ih)
		tmp = REG_SET_FIELD(tmp, IH_RB_CNTL, ENABLE_INTR, (enable ? 1 : 0));
	if (amdgpu_sriov_vf(adev)) {
		if (psp_reg_program(&adev->psp, ih_regs->psp_reg_id, tmp)) {
			dev_err(adev->dev, "PSP program IH_RB_CNTL failed!\n");
			return -ETIMEDOUT;
		}
	} else {
		WREG32(ih_regs->ih_rb_cntl, tmp);
	}

	if (enable) {
		ih->enabled = true;
	} else {
		 
		WREG32(ih_regs->ih_rb_rptr, 0);
		WREG32(ih_regs->ih_rb_wptr, 0);
		ih->enabled = false;
		ih->rptr = 0;
	}

	return 0;
}

 
static int vega20_ih_toggle_interrupts(struct amdgpu_device *adev, bool enable)
{
	struct amdgpu_ih_ring *ih[] = {&adev->irq.ih, &adev->irq.ih1, &adev->irq.ih2};
	int i;
	int r;

	for (i = 0; i < ARRAY_SIZE(ih); i++) {
		if (ih[i]->ring_size) {
			r = vega20_ih_toggle_ring_interrupts(adev, ih[i], enable);
			if (r)
				return r;
		}
	}

	return 0;
}

static uint32_t vega20_ih_rb_cntl(struct amdgpu_ih_ring *ih, uint32_t ih_rb_cntl)
{
	int rb_bufsz = order_base_2(ih->ring_size / 4);

	ih_rb_cntl = REG_SET_FIELD(ih_rb_cntl, IH_RB_CNTL,
				   MC_SPACE, ih->use_bus_addr ? 1 : 4);
	ih_rb_cntl = REG_SET_FIELD(ih_rb_cntl, IH_RB_CNTL,
				   WPTR_OVERFLOW_CLEAR, 1);
	ih_rb_cntl = REG_SET_FIELD(ih_rb_cntl, IH_RB_CNTL,
				   WPTR_OVERFLOW_ENABLE, 1);
	ih_rb_cntl = REG_SET_FIELD(ih_rb_cntl, IH_RB_CNTL, RB_SIZE, rb_bufsz);
	 
	ih_rb_cntl = REG_SET_FIELD(ih_rb_cntl, IH_RB_CNTL,
				   WPTR_WRITEBACK_ENABLE, 1);
	ih_rb_cntl = REG_SET_FIELD(ih_rb_cntl, IH_RB_CNTL, MC_SNOOP, 1);
	ih_rb_cntl = REG_SET_FIELD(ih_rb_cntl, IH_RB_CNTL, MC_RO, 0);
	ih_rb_cntl = REG_SET_FIELD(ih_rb_cntl, IH_RB_CNTL, MC_VMID, 0);

	return ih_rb_cntl;
}

static uint32_t vega20_ih_doorbell_rptr(struct amdgpu_ih_ring *ih)
{
	u32 ih_doorbell_rtpr = 0;

	if (ih->use_doorbell) {
		ih_doorbell_rtpr = REG_SET_FIELD(ih_doorbell_rtpr,
						 IH_DOORBELL_RPTR, OFFSET,
						 ih->doorbell_index);
		ih_doorbell_rtpr = REG_SET_FIELD(ih_doorbell_rtpr,
						 IH_DOORBELL_RPTR,
						 ENABLE, 1);
	} else {
		ih_doorbell_rtpr = REG_SET_FIELD(ih_doorbell_rtpr,
						 IH_DOORBELL_RPTR,
						 ENABLE, 0);
	}
	return ih_doorbell_rtpr;
}

 
static int vega20_ih_enable_ring(struct amdgpu_device *adev,
				 struct amdgpu_ih_ring *ih)
{
	struct amdgpu_ih_regs *ih_regs;
	uint32_t tmp;

	ih_regs = &ih->ih_regs;

	 
	WREG32(ih_regs->ih_rb_base, ih->gpu_addr >> 8);
	WREG32(ih_regs->ih_rb_base_hi, (ih->gpu_addr >> 40) & 0xff);

	tmp = RREG32(ih_regs->ih_rb_cntl);
	tmp = vega20_ih_rb_cntl(ih, tmp);
	if (ih == &adev->irq.ih)
		tmp = REG_SET_FIELD(tmp, IH_RB_CNTL, RPTR_REARM, !!adev->irq.msi_enabled);
	if (ih == &adev->irq.ih1)
		tmp = REG_SET_FIELD(tmp, IH_RB_CNTL, RB_FULL_DRAIN_ENABLE, 1);
	if (amdgpu_sriov_vf(adev)) {
		if (psp_reg_program(&adev->psp, ih_regs->psp_reg_id, tmp)) {
			dev_err(adev->dev, "PSP program IH_RB_CNTL failed!\n");
			return -ETIMEDOUT;
		}
	} else {
		WREG32(ih_regs->ih_rb_cntl, tmp);
	}

	if (ih == &adev->irq.ih) {
		 
		WREG32(ih_regs->ih_rb_wptr_addr_lo, lower_32_bits(ih->wptr_addr));
		WREG32(ih_regs->ih_rb_wptr_addr_hi, upper_32_bits(ih->wptr_addr) & 0xFFFF);
	}

	 
	WREG32(ih_regs->ih_rb_wptr, 0);
	WREG32(ih_regs->ih_rb_rptr, 0);

	WREG32(ih_regs->ih_doorbell_rptr, vega20_ih_doorbell_rptr(ih));

	return 0;
}

static uint32_t vega20_setup_retry_doorbell(u32 doorbell_index)
{
	u32 val = 0;

	val = REG_SET_FIELD(val, IH_DOORBELL_RPTR, OFFSET, doorbell_index);
	val = REG_SET_FIELD(val, IH_DOORBELL_RPTR, ENABLE, 1);

	return val;
}

 
static int vega20_ih_irq_init(struct amdgpu_device *adev)
{
	struct amdgpu_ih_ring *ih[] = {&adev->irq.ih, &adev->irq.ih1, &adev->irq.ih2};
	u32 ih_chicken;
	int ret;
	int i;

	 
	ret = vega20_ih_toggle_interrupts(adev, false);
	if (ret)
		return ret;

	adev->nbio.funcs->ih_control(adev);

	if ((adev->ip_versions[OSSSYS_HWIP][0] == IP_VERSION(4, 2, 1)) &&
	    adev->firmware.load_type == AMDGPU_FW_LOAD_DIRECT) {
		ih_chicken = RREG32_SOC15(OSSSYS, 0, mmIH_CHICKEN);
		if (adev->irq.ih.use_bus_addr) {
			ih_chicken = REG_SET_FIELD(ih_chicken, IH_CHICKEN,
						   MC_SPACE_GPA_ENABLE, 1);
		}
		WREG32_SOC15(OSSSYS, 0, mmIH_CHICKEN, ih_chicken);
	}

	 
	if ((adev->ip_versions[OSSSYS_HWIP][0] == IP_VERSION(4, 4, 0)) ||
	    (adev->ip_versions[OSSSYS_HWIP][0] == IP_VERSION(4, 4, 2))) {
		ih_chicken = RREG32_SOC15(OSSSYS, 0, mmIH_CHICKEN_ALDEBARAN);
		if (adev->irq.ih.use_bus_addr) {
			ih_chicken = REG_SET_FIELD(ih_chicken, IH_CHICKEN,
						   MC_SPACE_GPA_ENABLE, 1);
		}
		WREG32_SOC15(OSSSYS, 0, mmIH_CHICKEN_ALDEBARAN, ih_chicken);
	}

	for (i = 0; i < ARRAY_SIZE(ih); i++) {
		if (ih[i]->ring_size) {
			ret = vega20_ih_enable_ring(adev, ih[i]);
			if (ret)
				return ret;
		}
	}

	if (!amdgpu_sriov_vf(adev))
		adev->nbio.funcs->ih_doorbell_range(adev, adev->irq.ih.use_doorbell,
						    adev->irq.ih.doorbell_index);

	pci_set_master(adev->pdev);

	 
	adev->irq.retry_cam_doorbell_index = (adev->doorbell_index.ih + 3) << 1;
	WREG32_SOC15(OSSSYS, 0, mmIH_DOORBELL_RETRY_CAM,
		vega20_setup_retry_doorbell(adev->irq.retry_cam_doorbell_index));

	 
	if (adev->ip_versions[OSSSYS_HWIP][0] == IP_VERSION(4, 4, 0) ||
	    adev->ip_versions[OSSSYS_HWIP][0] == IP_VERSION(4, 4, 2))
		WREG32_FIELD15(OSSSYS, 0, IH_RETRY_INT_CAM_CNTL_ALDEBARAN,
			       ENABLE, 1);
	else
		WREG32_FIELD15(OSSSYS, 0, IH_RETRY_INT_CAM_CNTL, ENABLE, 1);

	adev->irq.retry_cam_enabled = true;

	 
	ret = vega20_ih_toggle_interrupts(adev, true);
	if (ret)
		return ret;

	if (adev->irq.ih_soft.ring_size)
		adev->irq.ih_soft.enabled = true;

	return 0;
}

 
static void vega20_ih_irq_disable(struct amdgpu_device *adev)
{
	vega20_ih_toggle_interrupts(adev, false);

	 
	mdelay(1);
}

 
static u32 vega20_ih_get_wptr(struct amdgpu_device *adev,
			      struct amdgpu_ih_ring *ih)
{
	u32 wptr, tmp;
	struct amdgpu_ih_regs *ih_regs;

	if (ih == &adev->irq.ih || ih == &adev->irq.ih_soft) {
		 
		wptr = le32_to_cpu(*ih->wptr_cpu);

		if (!REG_GET_FIELD(wptr, IH_RB_WPTR, RB_OVERFLOW))
			goto out;
	}

	ih_regs = &ih->ih_regs;

	 
	wptr = RREG32_NO_KIQ(ih_regs->ih_rb_wptr);
	if (!REG_GET_FIELD(wptr, IH_RB_WPTR, RB_OVERFLOW))
		goto out;

	wptr = REG_SET_FIELD(wptr, IH_RB_WPTR, RB_OVERFLOW, 0);

	 
	tmp = (wptr + 32) & ih->ptr_mask;
	dev_warn(adev->dev, "IH ring buffer overflow "
		 "(0x%08X, 0x%08X, 0x%08X)\n",
		 wptr, ih->rptr, tmp);
	ih->rptr = tmp;

	tmp = RREG32_NO_KIQ(ih_regs->ih_rb_cntl);
	tmp = REG_SET_FIELD(tmp, IH_RB_CNTL, WPTR_OVERFLOW_CLEAR, 1);
	WREG32_NO_KIQ(ih_regs->ih_rb_cntl, tmp);

out:
	return (wptr & ih->ptr_mask);
}

 
static void vega20_ih_irq_rearm(struct amdgpu_device *adev,
			       struct amdgpu_ih_ring *ih)
{
	uint32_t v = 0;
	uint32_t i = 0;
	struct amdgpu_ih_regs *ih_regs;

	ih_regs = &ih->ih_regs;

	 
	for (i = 0; i < MAX_REARM_RETRY; i++) {
		v = RREG32_NO_KIQ(ih_regs->ih_rb_rptr);
		if ((v < ih->ring_size) && (v != ih->rptr))
			WDOORBELL32(ih->doorbell_index, ih->rptr);
		else
			break;
	}
}

 
static void vega20_ih_set_rptr(struct amdgpu_device *adev,
			       struct amdgpu_ih_ring *ih)
{
	struct amdgpu_ih_regs *ih_regs;

	if (ih == &adev->irq.ih_soft)
		return;

	if (ih->use_doorbell) {
		 
		*ih->rptr_cpu = ih->rptr;
		WDOORBELL32(ih->doorbell_index, ih->rptr);

		if (amdgpu_sriov_vf(adev))
			vega20_ih_irq_rearm(adev, ih);
	} else {
		ih_regs = &ih->ih_regs;
		WREG32(ih_regs->ih_rb_rptr, ih->rptr);
	}
}

 
static int vega20_ih_self_irq(struct amdgpu_device *adev,
			      struct amdgpu_irq_src *source,
			      struct amdgpu_iv_entry *entry)
{
	switch (entry->ring_id) {
	case 1:
		schedule_work(&adev->irq.ih1_work);
		break;
	case 2:
		schedule_work(&adev->irq.ih2_work);
		break;
	default:
		break;
	}
	return 0;
}

static const struct amdgpu_irq_src_funcs vega20_ih_self_irq_funcs = {
	.process = vega20_ih_self_irq,
};

static void vega20_ih_set_self_irq_funcs(struct amdgpu_device *adev)
{
	adev->irq.self_irq.num_types = 0;
	adev->irq.self_irq.funcs = &vega20_ih_self_irq_funcs;
}

static int vega20_ih_early_init(void *handle)
{
	struct amdgpu_device *adev = (struct amdgpu_device *)handle;

	vega20_ih_set_interrupt_funcs(adev);
	vega20_ih_set_self_irq_funcs(adev);
	return 0;
}

static int vega20_ih_sw_init(void *handle)
{
	struct amdgpu_device *adev = (struct amdgpu_device *)handle;
	bool use_bus_addr = true;
	int r;

	r = amdgpu_irq_add_id(adev, SOC15_IH_CLIENTID_IH, 0,
			      &adev->irq.self_irq);
	if (r)
		return r;

	if ((adev->flags & AMD_IS_APU) &&
	    (adev->ip_versions[OSSSYS_HWIP][0] == IP_VERSION(4, 4, 2)))
		use_bus_addr = false;

	r = amdgpu_ih_ring_init(adev, &adev->irq.ih, IH_RING_SIZE, use_bus_addr);
	if (r)
		return r;

	adev->irq.ih.use_doorbell = true;
	adev->irq.ih.doorbell_index = adev->doorbell_index.ih << 1;

	r = amdgpu_ih_ring_init(adev, &adev->irq.ih1, PAGE_SIZE, use_bus_addr);
	if (r)
		return r;

	adev->irq.ih1.use_doorbell = true;
	adev->irq.ih1.doorbell_index = (adev->doorbell_index.ih + 1) << 1;

	if (adev->ip_versions[OSSSYS_HWIP][0] != IP_VERSION(4, 4, 2)) {
		r = amdgpu_ih_ring_init(adev, &adev->irq.ih2, PAGE_SIZE, true);
		if (r)
			return r;

		adev->irq.ih2.use_doorbell = true;
		adev->irq.ih2.doorbell_index = (adev->doorbell_index.ih + 2) << 1;
	}

	 
	vega20_ih_init_register_offset(adev);

	r = amdgpu_ih_ring_init(adev, &adev->irq.ih_soft, IH_SW_RING_SIZE, use_bus_addr);
	if (r)
		return r;

	r = amdgpu_irq_init(adev);

	return r;
}

static int vega20_ih_sw_fini(void *handle)
{
	struct amdgpu_device *adev = (struct amdgpu_device *)handle;

	amdgpu_irq_fini_sw(adev);

	return 0;
}

static int vega20_ih_hw_init(void *handle)
{
	int r;
	struct amdgpu_device *adev = (struct amdgpu_device *)handle;

	r = vega20_ih_irq_init(adev);
	if (r)
		return r;

	return 0;
}

static int vega20_ih_hw_fini(void *handle)
{
	struct amdgpu_device *adev = (struct amdgpu_device *)handle;

	vega20_ih_irq_disable(adev);

	return 0;
}

static int vega20_ih_suspend(void *handle)
{
	struct amdgpu_device *adev = (struct amdgpu_device *)handle;

	return vega20_ih_hw_fini(adev);
}

static int vega20_ih_resume(void *handle)
{
	struct amdgpu_device *adev = (struct amdgpu_device *)handle;

	return vega20_ih_hw_init(adev);
}

static bool vega20_ih_is_idle(void *handle)
{
	 
	return true;
}

static int vega20_ih_wait_for_idle(void *handle)
{
	 
	return -ETIMEDOUT;
}

static int vega20_ih_soft_reset(void *handle)
{
	 

	return 0;
}

static void vega20_ih_update_clockgating_state(struct amdgpu_device *adev,
					       bool enable)
{
	uint32_t data, def, field_val;

	if (adev->cg_flags & AMD_CG_SUPPORT_IH_CG) {
		def = data = RREG32_SOC15(OSSSYS, 0, mmIH_CLK_CTRL);
		field_val = enable ? 0 : 1;
		data = REG_SET_FIELD(data, IH_CLK_CTRL,
				     IH_RETRY_INT_CAM_MEM_CLK_SOFT_OVERRIDE, field_val);
		data = REG_SET_FIELD(data, IH_CLK_CTRL,
				     IH_BUFFER_MEM_CLK_SOFT_OVERRIDE, field_val);
		data = REG_SET_FIELD(data, IH_CLK_CTRL,
				     DBUS_MUX_CLK_SOFT_OVERRIDE, field_val);
		data = REG_SET_FIELD(data, IH_CLK_CTRL,
				     OSSSYS_SHARE_CLK_SOFT_OVERRIDE, field_val);
		data = REG_SET_FIELD(data, IH_CLK_CTRL,
				     LIMIT_SMN_CLK_SOFT_OVERRIDE, field_val);
		data = REG_SET_FIELD(data, IH_CLK_CTRL,
				     DYN_CLK_SOFT_OVERRIDE, field_val);
		data = REG_SET_FIELD(data, IH_CLK_CTRL,
				     REG_CLK_SOFT_OVERRIDE, field_val);
		if (def != data)
			WREG32_SOC15(OSSSYS, 0, mmIH_CLK_CTRL, data);
	}
}

static int vega20_ih_set_clockgating_state(void *handle,
					  enum amd_clockgating_state state)
{
	struct amdgpu_device *adev = (struct amdgpu_device *)handle;

	vega20_ih_update_clockgating_state(adev,
				state == AMD_CG_STATE_GATE);
	return 0;

}

static int vega20_ih_set_powergating_state(void *handle,
					  enum amd_powergating_state state)
{
	return 0;
}

const struct amd_ip_funcs vega20_ih_ip_funcs = {
	.name = "vega20_ih",
	.early_init = vega20_ih_early_init,
	.late_init = NULL,
	.sw_init = vega20_ih_sw_init,
	.sw_fini = vega20_ih_sw_fini,
	.hw_init = vega20_ih_hw_init,
	.hw_fini = vega20_ih_hw_fini,
	.suspend = vega20_ih_suspend,
	.resume = vega20_ih_resume,
	.is_idle = vega20_ih_is_idle,
	.wait_for_idle = vega20_ih_wait_for_idle,
	.soft_reset = vega20_ih_soft_reset,
	.set_clockgating_state = vega20_ih_set_clockgating_state,
	.set_powergating_state = vega20_ih_set_powergating_state,
};

static const struct amdgpu_ih_funcs vega20_ih_funcs = {
	.get_wptr = vega20_ih_get_wptr,
	.decode_iv = amdgpu_ih_decode_iv_helper,
	.decode_iv_ts = amdgpu_ih_decode_iv_ts_helper,
	.set_rptr = vega20_ih_set_rptr
};

static void vega20_ih_set_interrupt_funcs(struct amdgpu_device *adev)
{
	adev->irq.ih_funcs = &vega20_ih_funcs;
}

const struct amdgpu_ip_block_version vega20_ih_ip_block = {
	.type = AMD_IP_BLOCK_TYPE_IH,
	.major = 4,
	.minor = 2,
	.rev = 0,
	.funcs = &vega20_ih_ip_funcs,
};
