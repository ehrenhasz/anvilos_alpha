 

#include <linux/pci.h>

#include "amdgpu.h"
#include "amdgpu_ih.h"
#include "vid.h"

#include "oss/oss_3_0_d.h"
#include "oss/oss_3_0_sh_mask.h"

#include "bif/bif_5_1_d.h"
#include "bif/bif_5_1_sh_mask.h"

 

static void tonga_ih_set_interrupt_funcs(struct amdgpu_device *adev);

 
static void tonga_ih_enable_interrupts(struct amdgpu_device *adev)
{
	u32 ih_rb_cntl = RREG32(mmIH_RB_CNTL);

	ih_rb_cntl = REG_SET_FIELD(ih_rb_cntl, IH_RB_CNTL, RB_ENABLE, 1);
	ih_rb_cntl = REG_SET_FIELD(ih_rb_cntl, IH_RB_CNTL, ENABLE_INTR, 1);
	WREG32(mmIH_RB_CNTL, ih_rb_cntl);
	adev->irq.ih.enabled = true;
}

 
static void tonga_ih_disable_interrupts(struct amdgpu_device *adev)
{
	u32 ih_rb_cntl = RREG32(mmIH_RB_CNTL);

	ih_rb_cntl = REG_SET_FIELD(ih_rb_cntl, IH_RB_CNTL, RB_ENABLE, 0);
	ih_rb_cntl = REG_SET_FIELD(ih_rb_cntl, IH_RB_CNTL, ENABLE_INTR, 0);
	WREG32(mmIH_RB_CNTL, ih_rb_cntl);
	 
	WREG32(mmIH_RB_RPTR, 0);
	WREG32(mmIH_RB_WPTR, 0);
	adev->irq.ih.enabled = false;
	adev->irq.ih.rptr = 0;
}

 
static int tonga_ih_irq_init(struct amdgpu_device *adev)
{
	u32 interrupt_cntl, ih_rb_cntl, ih_doorbell_rtpr;
	struct amdgpu_ih_ring *ih = &adev->irq.ih;
	int rb_bufsz;

	 
	tonga_ih_disable_interrupts(adev);

	 
	WREG32(mmINTERRUPT_CNTL2, adev->dummy_page_addr >> 8);
	interrupt_cntl = RREG32(mmINTERRUPT_CNTL);
	 
	interrupt_cntl = REG_SET_FIELD(interrupt_cntl, INTERRUPT_CNTL, IH_DUMMY_RD_OVERRIDE, 0);
	 
	interrupt_cntl = REG_SET_FIELD(interrupt_cntl, INTERRUPT_CNTL, IH_REQ_NONSNOOP_EN, 0);
	WREG32(mmINTERRUPT_CNTL, interrupt_cntl);

	 
	WREG32(mmIH_RB_BASE, ih->gpu_addr >> 8);

	rb_bufsz = order_base_2(adev->irq.ih.ring_size / 4);
	ih_rb_cntl = REG_SET_FIELD(0, IH_RB_CNTL, WPTR_OVERFLOW_CLEAR, 1);
	ih_rb_cntl = REG_SET_FIELD(ih_rb_cntl, IH_RB_CNTL, RB_SIZE, rb_bufsz);
	 
	ih_rb_cntl = REG_SET_FIELD(ih_rb_cntl, IH_RB_CNTL, WPTR_WRITEBACK_ENABLE, 1);
	ih_rb_cntl = REG_SET_FIELD(ih_rb_cntl, IH_RB_CNTL, MC_VMID, 0);

	if (adev->irq.msi_enabled)
		ih_rb_cntl = REG_SET_FIELD(ih_rb_cntl, IH_RB_CNTL, RPTR_REARM, 1);

	WREG32(mmIH_RB_CNTL, ih_rb_cntl);

	 
	WREG32(mmIH_RB_WPTR_ADDR_LO, lower_32_bits(ih->wptr_addr));
	WREG32(mmIH_RB_WPTR_ADDR_HI, upper_32_bits(ih->wptr_addr) & 0xFF);

	 
	WREG32(mmIH_RB_RPTR, 0);
	WREG32(mmIH_RB_WPTR, 0);

	ih_doorbell_rtpr = RREG32(mmIH_DOORBELL_RPTR);
	if (adev->irq.ih.use_doorbell) {
		ih_doorbell_rtpr = REG_SET_FIELD(ih_doorbell_rtpr, IH_DOORBELL_RPTR,
						 OFFSET, adev->irq.ih.doorbell_index);
		ih_doorbell_rtpr = REG_SET_FIELD(ih_doorbell_rtpr, IH_DOORBELL_RPTR,
						 ENABLE, 1);
	} else {
		ih_doorbell_rtpr = REG_SET_FIELD(ih_doorbell_rtpr, IH_DOORBELL_RPTR,
						 ENABLE, 0);
	}
	WREG32(mmIH_DOORBELL_RPTR, ih_doorbell_rtpr);

	pci_set_master(adev->pdev);

	 
	tonga_ih_enable_interrupts(adev);

	return 0;
}

 
static void tonga_ih_irq_disable(struct amdgpu_device *adev)
{
	tonga_ih_disable_interrupts(adev);

	 
	mdelay(1);
}

 
static u32 tonga_ih_get_wptr(struct amdgpu_device *adev,
			     struct amdgpu_ih_ring *ih)
{
	u32 wptr, tmp;

	wptr = le32_to_cpu(*ih->wptr_cpu);

	if (!REG_GET_FIELD(wptr, IH_RB_WPTR, RB_OVERFLOW))
		goto out;

	 
	wptr = RREG32(mmIH_RB_WPTR);

	if (!REG_GET_FIELD(wptr, IH_RB_WPTR, RB_OVERFLOW))
		goto out;

	wptr = REG_SET_FIELD(wptr, IH_RB_WPTR, RB_OVERFLOW, 0);

	 

	dev_warn(adev->dev, "IH ring buffer overflow (0x%08X, 0x%08X, 0x%08X)\n",
		wptr, ih->rptr, (wptr + 16) & ih->ptr_mask);
	ih->rptr = (wptr + 16) & ih->ptr_mask;
	tmp = RREG32(mmIH_RB_CNTL);
	tmp = REG_SET_FIELD(tmp, IH_RB_CNTL, WPTR_OVERFLOW_CLEAR, 1);
	WREG32(mmIH_RB_CNTL, tmp);

out:
	return (wptr & ih->ptr_mask);
}

 
static void tonga_ih_decode_iv(struct amdgpu_device *adev,
			       struct amdgpu_ih_ring *ih,
			       struct amdgpu_iv_entry *entry)
{
	 
	u32 ring_index = ih->rptr >> 2;
	uint32_t dw[4];

	dw[0] = le32_to_cpu(ih->ring[ring_index + 0]);
	dw[1] = le32_to_cpu(ih->ring[ring_index + 1]);
	dw[2] = le32_to_cpu(ih->ring[ring_index + 2]);
	dw[3] = le32_to_cpu(ih->ring[ring_index + 3]);

	entry->client_id = AMDGPU_IRQ_CLIENTID_LEGACY;
	entry->src_id = dw[0] & 0xff;
	entry->src_data[0] = dw[1] & 0xfffffff;
	entry->ring_id = dw[2] & 0xff;
	entry->vmid = (dw[2] >> 8) & 0xff;
	entry->pasid = (dw[2] >> 16) & 0xffff;

	 
	ih->rptr += 16;
}

 
static void tonga_ih_set_rptr(struct amdgpu_device *adev,
			      struct amdgpu_ih_ring *ih)
{
	if (ih->use_doorbell) {
		 
		*ih->rptr_cpu = ih->rptr;
		WDOORBELL32(ih->doorbell_index, ih->rptr);
	} else {
		WREG32(mmIH_RB_RPTR, ih->rptr);
	}
}

static int tonga_ih_early_init(void *handle)
{
	struct amdgpu_device *adev = (struct amdgpu_device *)handle;
	int ret;

	ret = amdgpu_irq_add_domain(adev);
	if (ret)
		return ret;

	tonga_ih_set_interrupt_funcs(adev);

	return 0;
}

static int tonga_ih_sw_init(void *handle)
{
	int r;
	struct amdgpu_device *adev = (struct amdgpu_device *)handle;

	r = amdgpu_ih_ring_init(adev, &adev->irq.ih, 64 * 1024, true);
	if (r)
		return r;

	adev->irq.ih.use_doorbell = true;
	adev->irq.ih.doorbell_index = adev->doorbell_index.ih;

	r = amdgpu_irq_init(adev);

	return r;
}

static int tonga_ih_sw_fini(void *handle)
{
	struct amdgpu_device *adev = (struct amdgpu_device *)handle;

	amdgpu_irq_fini_sw(adev);
	amdgpu_irq_remove_domain(adev);

	return 0;
}

static int tonga_ih_hw_init(void *handle)
{
	int r;
	struct amdgpu_device *adev = (struct amdgpu_device *)handle;

	r = tonga_ih_irq_init(adev);
	if (r)
		return r;

	return 0;
}

static int tonga_ih_hw_fini(void *handle)
{
	struct amdgpu_device *adev = (struct amdgpu_device *)handle;

	tonga_ih_irq_disable(adev);

	return 0;
}

static int tonga_ih_suspend(void *handle)
{
	struct amdgpu_device *adev = (struct amdgpu_device *)handle;

	return tonga_ih_hw_fini(adev);
}

static int tonga_ih_resume(void *handle)
{
	struct amdgpu_device *adev = (struct amdgpu_device *)handle;

	return tonga_ih_hw_init(adev);
}

static bool tonga_ih_is_idle(void *handle)
{
	struct amdgpu_device *adev = (struct amdgpu_device *)handle;
	u32 tmp = RREG32(mmSRBM_STATUS);

	if (REG_GET_FIELD(tmp, SRBM_STATUS, IH_BUSY))
		return false;

	return true;
}

static int tonga_ih_wait_for_idle(void *handle)
{
	unsigned i;
	u32 tmp;
	struct amdgpu_device *adev = (struct amdgpu_device *)handle;

	for (i = 0; i < adev->usec_timeout; i++) {
		 
		tmp = RREG32(mmSRBM_STATUS);
		if (!REG_GET_FIELD(tmp, SRBM_STATUS, IH_BUSY))
			return 0;
		udelay(1);
	}
	return -ETIMEDOUT;
}

static bool tonga_ih_check_soft_reset(void *handle)
{
	struct amdgpu_device *adev = (struct amdgpu_device *)handle;
	u32 srbm_soft_reset = 0;
	u32 tmp = RREG32(mmSRBM_STATUS);

	if (tmp & SRBM_STATUS__IH_BUSY_MASK)
		srbm_soft_reset = REG_SET_FIELD(srbm_soft_reset, SRBM_SOFT_RESET,
						SOFT_RESET_IH, 1);

	if (srbm_soft_reset) {
		adev->irq.srbm_soft_reset = srbm_soft_reset;
		return true;
	} else {
		adev->irq.srbm_soft_reset = 0;
		return false;
	}
}

static int tonga_ih_pre_soft_reset(void *handle)
{
	struct amdgpu_device *adev = (struct amdgpu_device *)handle;

	if (!adev->irq.srbm_soft_reset)
		return 0;

	return tonga_ih_hw_fini(adev);
}

static int tonga_ih_post_soft_reset(void *handle)
{
	struct amdgpu_device *adev = (struct amdgpu_device *)handle;

	if (!adev->irq.srbm_soft_reset)
		return 0;

	return tonga_ih_hw_init(adev);
}

static int tonga_ih_soft_reset(void *handle)
{
	struct amdgpu_device *adev = (struct amdgpu_device *)handle;
	u32 srbm_soft_reset;

	if (!adev->irq.srbm_soft_reset)
		return 0;
	srbm_soft_reset = adev->irq.srbm_soft_reset;

	if (srbm_soft_reset) {
		u32 tmp;

		tmp = RREG32(mmSRBM_SOFT_RESET);
		tmp |= srbm_soft_reset;
		dev_info(adev->dev, "SRBM_SOFT_RESET=0x%08X\n", tmp);
		WREG32(mmSRBM_SOFT_RESET, tmp);
		tmp = RREG32(mmSRBM_SOFT_RESET);

		udelay(50);

		tmp &= ~srbm_soft_reset;
		WREG32(mmSRBM_SOFT_RESET, tmp);
		tmp = RREG32(mmSRBM_SOFT_RESET);

		 
		udelay(50);
	}

	return 0;
}

static int tonga_ih_set_clockgating_state(void *handle,
					  enum amd_clockgating_state state)
{
	return 0;
}

static int tonga_ih_set_powergating_state(void *handle,
					  enum amd_powergating_state state)
{
	return 0;
}

static const struct amd_ip_funcs tonga_ih_ip_funcs = {
	.name = "tonga_ih",
	.early_init = tonga_ih_early_init,
	.late_init = NULL,
	.sw_init = tonga_ih_sw_init,
	.sw_fini = tonga_ih_sw_fini,
	.hw_init = tonga_ih_hw_init,
	.hw_fini = tonga_ih_hw_fini,
	.suspend = tonga_ih_suspend,
	.resume = tonga_ih_resume,
	.is_idle = tonga_ih_is_idle,
	.wait_for_idle = tonga_ih_wait_for_idle,
	.check_soft_reset = tonga_ih_check_soft_reset,
	.pre_soft_reset = tonga_ih_pre_soft_reset,
	.soft_reset = tonga_ih_soft_reset,
	.post_soft_reset = tonga_ih_post_soft_reset,
	.set_clockgating_state = tonga_ih_set_clockgating_state,
	.set_powergating_state = tonga_ih_set_powergating_state,
};

static const struct amdgpu_ih_funcs tonga_ih_funcs = {
	.get_wptr = tonga_ih_get_wptr,
	.decode_iv = tonga_ih_decode_iv,
	.set_rptr = tonga_ih_set_rptr
};

static void tonga_ih_set_interrupt_funcs(struct amdgpu_device *adev)
{
	adev->irq.ih_funcs = &tonga_ih_funcs;
}

const struct amdgpu_ip_block_version tonga_ih_ip_block = {
	.type = AMD_IP_BLOCK_TYPE_IH,
	.major = 3,
	.minor = 0,
	.rev = 0,
	.funcs = &tonga_ih_ip_funcs,
};
