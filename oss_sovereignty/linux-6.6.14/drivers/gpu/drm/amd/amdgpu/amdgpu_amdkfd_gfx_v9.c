 
#include "amdgpu.h"
#include "amdgpu_amdkfd.h"
#include "gc/gc_9_0_offset.h"
#include "gc/gc_9_0_sh_mask.h"
#include "vega10_enum.h"
#include "sdma0/sdma0_4_0_offset.h"
#include "sdma0/sdma0_4_0_sh_mask.h"
#include "sdma1/sdma1_4_0_offset.h"
#include "sdma1/sdma1_4_0_sh_mask.h"
#include "athub/athub_1_0_offset.h"
#include "athub/athub_1_0_sh_mask.h"
#include "oss/osssys_4_0_offset.h"
#include "oss/osssys_4_0_sh_mask.h"
#include "soc15_common.h"
#include "v9_structs.h"
#include "soc15.h"
#include "soc15d.h"
#include "gfx_v9_0.h"
#include "amdgpu_amdkfd_gfx_v9.h"
#include <uapi/linux/kfd_ioctl.h>

enum hqd_dequeue_request_type {
	NO_ACTION = 0,
	DRAIN_PIPE,
	RESET_WAVES,
	SAVE_WAVES
};

static void kgd_gfx_v9_lock_srbm(struct amdgpu_device *adev, uint32_t mec, uint32_t pipe,
			uint32_t queue, uint32_t vmid, uint32_t inst)
{
	mutex_lock(&adev->srbm_mutex);
	soc15_grbm_select(adev, mec, pipe, queue, vmid, GET_INST(GC, inst));
}

static void kgd_gfx_v9_unlock_srbm(struct amdgpu_device *adev, uint32_t inst)
{
	soc15_grbm_select(adev, 0, 0, 0, 0, GET_INST(GC, inst));
	mutex_unlock(&adev->srbm_mutex);
}

void kgd_gfx_v9_acquire_queue(struct amdgpu_device *adev, uint32_t pipe_id,
				uint32_t queue_id, uint32_t inst)
{
	uint32_t mec = (pipe_id / adev->gfx.mec.num_pipe_per_mec) + 1;
	uint32_t pipe = (pipe_id % adev->gfx.mec.num_pipe_per_mec);

	kgd_gfx_v9_lock_srbm(adev, mec, pipe, queue_id, 0, inst);
}

uint64_t kgd_gfx_v9_get_queue_mask(struct amdgpu_device *adev,
			       uint32_t pipe_id, uint32_t queue_id)
{
	unsigned int bit = pipe_id * adev->gfx.mec.num_queue_per_pipe +
			queue_id;

	return 1ull << bit;
}

void kgd_gfx_v9_release_queue(struct amdgpu_device *adev, uint32_t inst)
{
	kgd_gfx_v9_unlock_srbm(adev, inst);
}

void kgd_gfx_v9_program_sh_mem_settings(struct amdgpu_device *adev, uint32_t vmid,
					uint32_t sh_mem_config,
					uint32_t sh_mem_ape1_base,
					uint32_t sh_mem_ape1_limit,
					uint32_t sh_mem_bases, uint32_t inst)
{
	kgd_gfx_v9_lock_srbm(adev, 0, 0, 0, vmid, inst);

	WREG32_RLC(SOC15_REG_OFFSET(GC, GET_INST(GC, inst), mmSH_MEM_CONFIG), sh_mem_config);
	WREG32_RLC(SOC15_REG_OFFSET(GC, GET_INST(GC, inst), mmSH_MEM_BASES), sh_mem_bases);
	 

	kgd_gfx_v9_unlock_srbm(adev, inst);
}

int kgd_gfx_v9_set_pasid_vmid_mapping(struct amdgpu_device *adev, u32 pasid,
					unsigned int vmid, uint32_t inst)
{
	 
	uint32_t pasid_mapping = (pasid == 0) ? 0 : (uint32_t)pasid |
			ATC_VMID0_PASID_MAPPING__VALID_MASK;

	 

	WREG32(SOC15_REG_OFFSET(ATHUB, 0, mmATC_VMID0_PASID_MAPPING) + vmid,
	       pasid_mapping);

	while (!(RREG32(SOC15_REG_OFFSET(
				ATHUB, 0,
				mmATC_VMID_PASID_MAPPING_UPDATE_STATUS)) &
		 (1U << vmid)))
		cpu_relax();

	WREG32(SOC15_REG_OFFSET(ATHUB, 0,
				mmATC_VMID_PASID_MAPPING_UPDATE_STATUS),
	       1U << vmid);

	 
	WREG32(SOC15_REG_OFFSET(OSSSYS, 0, mmIH_VMID_0_LUT) + vmid,
	       pasid_mapping);

	WREG32(SOC15_REG_OFFSET(ATHUB, 0, mmATC_VMID16_PASID_MAPPING) + vmid,
	       pasid_mapping);

	while (!(RREG32(SOC15_REG_OFFSET(
				ATHUB, 0,
				mmATC_VMID_PASID_MAPPING_UPDATE_STATUS)) &
		 (1U << (vmid + 16))))
		cpu_relax();

	WREG32(SOC15_REG_OFFSET(ATHUB, 0,
				mmATC_VMID_PASID_MAPPING_UPDATE_STATUS),
	       1U << (vmid + 16));

	 
	WREG32(SOC15_REG_OFFSET(OSSSYS, 0, mmIH_VMID_0_LUT_MM) + vmid,
	       pasid_mapping);
	return 0;
}

 

int kgd_gfx_v9_init_interrupts(struct amdgpu_device *adev, uint32_t pipe_id,
				uint32_t inst)
{
	uint32_t mec;
	uint32_t pipe;

	mec = (pipe_id / adev->gfx.mec.num_pipe_per_mec) + 1;
	pipe = (pipe_id % adev->gfx.mec.num_pipe_per_mec);

	kgd_gfx_v9_lock_srbm(adev, mec, pipe, 0, 0, inst);

	WREG32_SOC15(GC, GET_INST(GC, inst), mmCPC_INT_CNTL,
		CP_INT_CNTL_RING0__TIME_STAMP_INT_ENABLE_MASK |
		CP_INT_CNTL_RING0__OPCODE_ERROR_INT_ENABLE_MASK);

	kgd_gfx_v9_unlock_srbm(adev, inst);

	return 0;
}

static uint32_t get_sdma_rlc_reg_offset(struct amdgpu_device *adev,
				unsigned int engine_id,
				unsigned int queue_id)
{
	uint32_t sdma_engine_reg_base = 0;
	uint32_t sdma_rlc_reg_offset;

	switch (engine_id) {
	default:
		dev_warn(adev->dev,
			 "Invalid sdma engine id (%d), using engine id 0\n",
			 engine_id);
		fallthrough;
	case 0:
		sdma_engine_reg_base = SOC15_REG_OFFSET(SDMA0, 0,
				mmSDMA0_RLC0_RB_CNTL) - mmSDMA0_RLC0_RB_CNTL;
		break;
	case 1:
		sdma_engine_reg_base = SOC15_REG_OFFSET(SDMA1, 0,
				mmSDMA1_RLC0_RB_CNTL) - mmSDMA0_RLC0_RB_CNTL;
		break;
	}

	sdma_rlc_reg_offset = sdma_engine_reg_base
		+ queue_id * (mmSDMA0_RLC1_RB_CNTL - mmSDMA0_RLC0_RB_CNTL);

	pr_debug("RLC register offset for SDMA%d RLC%d: 0x%x\n", engine_id,
		 queue_id, sdma_rlc_reg_offset);

	return sdma_rlc_reg_offset;
}

static inline struct v9_mqd *get_mqd(void *mqd)
{
	return (struct v9_mqd *)mqd;
}

static inline struct v9_sdma_mqd *get_sdma_mqd(void *mqd)
{
	return (struct v9_sdma_mqd *)mqd;
}

int kgd_gfx_v9_hqd_load(struct amdgpu_device *adev, void *mqd,
			uint32_t pipe_id, uint32_t queue_id,
			uint32_t __user *wptr, uint32_t wptr_shift,
			uint32_t wptr_mask, struct mm_struct *mm,
			uint32_t inst)
{
	struct v9_mqd *m;
	uint32_t *mqd_hqd;
	uint32_t reg, hqd_base, data;

	m = get_mqd(mqd);

	kgd_gfx_v9_acquire_queue(adev, pipe_id, queue_id, inst);

	 
	mqd_hqd = &m->cp_mqd_base_addr_lo;
	hqd_base = SOC15_REG_OFFSET(GC, GET_INST(GC, inst), mmCP_MQD_BASE_ADDR);

	for (reg = hqd_base;
	     reg <= SOC15_REG_OFFSET(GC, GET_INST(GC, inst), mmCP_HQD_PQ_WPTR_HI); reg++)
		WREG32_RLC(reg, mqd_hqd[reg - hqd_base]);


	 
	data = REG_SET_FIELD(m->cp_hqd_pq_doorbell_control,
			     CP_HQD_PQ_DOORBELL_CONTROL, DOORBELL_EN, 1);
	WREG32_RLC(SOC15_REG_OFFSET(GC, GET_INST(GC, inst), mmCP_HQD_PQ_DOORBELL_CONTROL),
					data);

	if (wptr) {
		 
		uint32_t queue_size =
			2 << REG_GET_FIELD(m->cp_hqd_pq_control,
					   CP_HQD_PQ_CONTROL, QUEUE_SIZE);
		uint64_t guessed_wptr = m->cp_hqd_pq_rptr & (queue_size - 1);

		if ((m->cp_hqd_pq_wptr_lo & (queue_size - 1)) < guessed_wptr)
			guessed_wptr += queue_size;
		guessed_wptr += m->cp_hqd_pq_wptr_lo & ~(queue_size - 1);
		guessed_wptr += (uint64_t)m->cp_hqd_pq_wptr_hi << 32;

		WREG32_RLC(SOC15_REG_OFFSET(GC, GET_INST(GC, inst), mmCP_HQD_PQ_WPTR_LO),
		       lower_32_bits(guessed_wptr));
		WREG32_RLC(SOC15_REG_OFFSET(GC, GET_INST(GC, inst), mmCP_HQD_PQ_WPTR_HI),
		       upper_32_bits(guessed_wptr));
		WREG32_RLC(SOC15_REG_OFFSET(GC, GET_INST(GC, inst), mmCP_HQD_PQ_WPTR_POLL_ADDR),
		       lower_32_bits((uintptr_t)wptr));
		WREG32_RLC(SOC15_REG_OFFSET(GC, GET_INST(GC, inst), mmCP_HQD_PQ_WPTR_POLL_ADDR_HI),
		       upper_32_bits((uintptr_t)wptr));
		WREG32_SOC15(GC, GET_INST(GC, inst), mmCP_PQ_WPTR_POLL_CNTL1,
		       (uint32_t)kgd_gfx_v9_get_queue_mask(adev, pipe_id, queue_id));
	}

	 
	WREG32_RLC(SOC15_REG_OFFSET(GC, GET_INST(GC, inst), mmCP_HQD_EOP_RPTR),
	       REG_SET_FIELD(m->cp_hqd_eop_rptr,
			     CP_HQD_EOP_RPTR, INIT_FETCHER, 1));

	data = REG_SET_FIELD(m->cp_hqd_active, CP_HQD_ACTIVE, ACTIVE, 1);
	WREG32_RLC(SOC15_REG_OFFSET(GC, GET_INST(GC, inst), mmCP_HQD_ACTIVE), data);

	kgd_gfx_v9_release_queue(adev, inst);

	return 0;
}

int kgd_gfx_v9_hiq_mqd_load(struct amdgpu_device *adev, void *mqd,
			    uint32_t pipe_id, uint32_t queue_id,
			    uint32_t doorbell_off, uint32_t inst)
{
	struct amdgpu_ring *kiq_ring = &adev->gfx.kiq[inst].ring;
	struct v9_mqd *m;
	uint32_t mec, pipe;
	int r;

	m = get_mqd(mqd);

	kgd_gfx_v9_acquire_queue(adev, pipe_id, queue_id, inst);

	mec = (pipe_id / adev->gfx.mec.num_pipe_per_mec) + 1;
	pipe = (pipe_id % adev->gfx.mec.num_pipe_per_mec);

	pr_debug("kfd: set HIQ, mec:%d, pipe:%d, queue:%d.\n",
		 mec, pipe, queue_id);

	spin_lock(&adev->gfx.kiq[inst].ring_lock);
	r = amdgpu_ring_alloc(kiq_ring, 7);
	if (r) {
		pr_err("Failed to alloc KIQ (%d).\n", r);
		goto out_unlock;
	}

	amdgpu_ring_write(kiq_ring, PACKET3(PACKET3_MAP_QUEUES, 5));
	amdgpu_ring_write(kiq_ring,
			  PACKET3_MAP_QUEUES_QUEUE_SEL(0) |  
			  PACKET3_MAP_QUEUES_VMID(m->cp_hqd_vmid) |  
			  PACKET3_MAP_QUEUES_QUEUE(queue_id) |
			  PACKET3_MAP_QUEUES_PIPE(pipe) |
			  PACKET3_MAP_QUEUES_ME((mec - 1)) |
			  PACKET3_MAP_QUEUES_QUEUE_TYPE(0) |  
			  PACKET3_MAP_QUEUES_ALLOC_FORMAT(0) |  
			  PACKET3_MAP_QUEUES_ENGINE_SEL(1) |  
			  PACKET3_MAP_QUEUES_NUM_QUEUES(1));  
	amdgpu_ring_write(kiq_ring,
			  PACKET3_MAP_QUEUES_DOORBELL_OFFSET(doorbell_off));
	amdgpu_ring_write(kiq_ring, m->cp_mqd_base_addr_lo);
	amdgpu_ring_write(kiq_ring, m->cp_mqd_base_addr_hi);
	amdgpu_ring_write(kiq_ring, m->cp_hqd_pq_wptr_poll_addr_lo);
	amdgpu_ring_write(kiq_ring, m->cp_hqd_pq_wptr_poll_addr_hi);
	amdgpu_ring_commit(kiq_ring);

out_unlock:
	spin_unlock(&adev->gfx.kiq[inst].ring_lock);
	kgd_gfx_v9_release_queue(adev, inst);

	return r;
}

int kgd_gfx_v9_hqd_dump(struct amdgpu_device *adev,
			uint32_t pipe_id, uint32_t queue_id,
			uint32_t (**dump)[2], uint32_t *n_regs, uint32_t inst)
{
	uint32_t i = 0, reg;
#define HQD_N_REGS 56
#define DUMP_REG(addr) do {				\
		if (WARN_ON_ONCE(i >= HQD_N_REGS))	\
			break;				\
		(*dump)[i][0] = (addr) << 2;		\
		(*dump)[i++][1] = RREG32(addr);		\
	} while (0)

	*dump = kmalloc_array(HQD_N_REGS * 2, sizeof(uint32_t), GFP_KERNEL);
	if (*dump == NULL)
		return -ENOMEM;

	kgd_gfx_v9_acquire_queue(adev, pipe_id, queue_id, inst);

	for (reg = SOC15_REG_OFFSET(GC, GET_INST(GC, inst), mmCP_MQD_BASE_ADDR);
	     reg <= SOC15_REG_OFFSET(GC, GET_INST(GC, inst), mmCP_HQD_PQ_WPTR_HI); reg++)
		DUMP_REG(reg);

	kgd_gfx_v9_release_queue(adev, inst);

	WARN_ON_ONCE(i != HQD_N_REGS);
	*n_regs = i;

	return 0;
}

static int kgd_hqd_sdma_load(struct amdgpu_device *adev, void *mqd,
			     uint32_t __user *wptr, struct mm_struct *mm)
{
	struct v9_sdma_mqd *m;
	uint32_t sdma_rlc_reg_offset;
	unsigned long end_jiffies;
	uint32_t data;
	uint64_t data64;
	uint64_t __user *wptr64 = (uint64_t __user *)wptr;

	m = get_sdma_mqd(mqd);
	sdma_rlc_reg_offset = get_sdma_rlc_reg_offset(adev, m->sdma_engine_id,
					    m->sdma_queue_id);

	WREG32(sdma_rlc_reg_offset + mmSDMA0_RLC0_RB_CNTL,
		m->sdmax_rlcx_rb_cntl & (~SDMA0_RLC0_RB_CNTL__RB_ENABLE_MASK));

	end_jiffies = msecs_to_jiffies(2000) + jiffies;
	while (true) {
		data = RREG32(sdma_rlc_reg_offset + mmSDMA0_RLC0_CONTEXT_STATUS);
		if (data & SDMA0_RLC0_CONTEXT_STATUS__IDLE_MASK)
			break;
		if (time_after(jiffies, end_jiffies)) {
			pr_err("SDMA RLC not idle in %s\n", __func__);
			return -ETIME;
		}
		usleep_range(500, 1000);
	}

	WREG32(sdma_rlc_reg_offset + mmSDMA0_RLC0_DOORBELL_OFFSET,
	       m->sdmax_rlcx_doorbell_offset);

	data = REG_SET_FIELD(m->sdmax_rlcx_doorbell, SDMA0_RLC0_DOORBELL,
			     ENABLE, 1);
	WREG32(sdma_rlc_reg_offset + mmSDMA0_RLC0_DOORBELL, data);
	WREG32(sdma_rlc_reg_offset + mmSDMA0_RLC0_RB_RPTR,
				m->sdmax_rlcx_rb_rptr);
	WREG32(sdma_rlc_reg_offset + mmSDMA0_RLC0_RB_RPTR_HI,
				m->sdmax_rlcx_rb_rptr_hi);

	WREG32(sdma_rlc_reg_offset + mmSDMA0_RLC0_MINOR_PTR_UPDATE, 1);
	if (read_user_wptr(mm, wptr64, data64)) {
		WREG32(sdma_rlc_reg_offset + mmSDMA0_RLC0_RB_WPTR,
		       lower_32_bits(data64));
		WREG32(sdma_rlc_reg_offset + mmSDMA0_RLC0_RB_WPTR_HI,
		       upper_32_bits(data64));
	} else {
		WREG32(sdma_rlc_reg_offset + mmSDMA0_RLC0_RB_WPTR,
		       m->sdmax_rlcx_rb_rptr);
		WREG32(sdma_rlc_reg_offset + mmSDMA0_RLC0_RB_WPTR_HI,
		       m->sdmax_rlcx_rb_rptr_hi);
	}
	WREG32(sdma_rlc_reg_offset + mmSDMA0_RLC0_MINOR_PTR_UPDATE, 0);

	WREG32(sdma_rlc_reg_offset + mmSDMA0_RLC0_RB_BASE, m->sdmax_rlcx_rb_base);
	WREG32(sdma_rlc_reg_offset + mmSDMA0_RLC0_RB_BASE_HI,
			m->sdmax_rlcx_rb_base_hi);
	WREG32(sdma_rlc_reg_offset + mmSDMA0_RLC0_RB_RPTR_ADDR_LO,
			m->sdmax_rlcx_rb_rptr_addr_lo);
	WREG32(sdma_rlc_reg_offset + mmSDMA0_RLC0_RB_RPTR_ADDR_HI,
			m->sdmax_rlcx_rb_rptr_addr_hi);

	data = REG_SET_FIELD(m->sdmax_rlcx_rb_cntl, SDMA0_RLC0_RB_CNTL,
			     RB_ENABLE, 1);
	WREG32(sdma_rlc_reg_offset + mmSDMA0_RLC0_RB_CNTL, data);

	return 0;
}

static int kgd_hqd_sdma_dump(struct amdgpu_device *adev,
			     uint32_t engine_id, uint32_t queue_id,
			     uint32_t (**dump)[2], uint32_t *n_regs)
{
	uint32_t sdma_rlc_reg_offset = get_sdma_rlc_reg_offset(adev,
			engine_id, queue_id);
	uint32_t i = 0, reg;
#undef HQD_N_REGS
#define HQD_N_REGS (19+6+7+10)

	*dump = kmalloc_array(HQD_N_REGS * 2, sizeof(uint32_t), GFP_KERNEL);
	if (*dump == NULL)
		return -ENOMEM;

	for (reg = mmSDMA0_RLC0_RB_CNTL; reg <= mmSDMA0_RLC0_DOORBELL; reg++)
		DUMP_REG(sdma_rlc_reg_offset + reg);
	for (reg = mmSDMA0_RLC0_STATUS; reg <= mmSDMA0_RLC0_CSA_ADDR_HI; reg++)
		DUMP_REG(sdma_rlc_reg_offset + reg);
	for (reg = mmSDMA0_RLC0_IB_SUB_REMAIN;
	     reg <= mmSDMA0_RLC0_MINOR_PTR_UPDATE; reg++)
		DUMP_REG(sdma_rlc_reg_offset + reg);
	for (reg = mmSDMA0_RLC0_MIDCMD_DATA0;
	     reg <= mmSDMA0_RLC0_MIDCMD_CNTL; reg++)
		DUMP_REG(sdma_rlc_reg_offset + reg);

	WARN_ON_ONCE(i != HQD_N_REGS);
	*n_regs = i;

	return 0;
}

bool kgd_gfx_v9_hqd_is_occupied(struct amdgpu_device *adev,
				uint64_t queue_address, uint32_t pipe_id,
				uint32_t queue_id, uint32_t inst)
{
	uint32_t act;
	bool retval = false;
	uint32_t low, high;

	kgd_gfx_v9_acquire_queue(adev, pipe_id, queue_id, inst);
	act = RREG32_SOC15(GC, GET_INST(GC, inst), mmCP_HQD_ACTIVE);
	if (act) {
		low = lower_32_bits(queue_address >> 8);
		high = upper_32_bits(queue_address >> 8);

		if (low == RREG32_SOC15(GC, GET_INST(GC, inst), mmCP_HQD_PQ_BASE) &&
		   high == RREG32_SOC15(GC, GET_INST(GC, inst), mmCP_HQD_PQ_BASE_HI))
			retval = true;
	}
	kgd_gfx_v9_release_queue(adev, inst);
	return retval;
}

static bool kgd_hqd_sdma_is_occupied(struct amdgpu_device *adev, void *mqd)
{
	struct v9_sdma_mqd *m;
	uint32_t sdma_rlc_reg_offset;
	uint32_t sdma_rlc_rb_cntl;

	m = get_sdma_mqd(mqd);
	sdma_rlc_reg_offset = get_sdma_rlc_reg_offset(adev, m->sdma_engine_id,
					    m->sdma_queue_id);

	sdma_rlc_rb_cntl = RREG32(sdma_rlc_reg_offset + mmSDMA0_RLC0_RB_CNTL);

	if (sdma_rlc_rb_cntl & SDMA0_RLC0_RB_CNTL__RB_ENABLE_MASK)
		return true;

	return false;
}

int kgd_gfx_v9_hqd_destroy(struct amdgpu_device *adev, void *mqd,
				enum kfd_preempt_type reset_type,
				unsigned int utimeout, uint32_t pipe_id,
				uint32_t queue_id, uint32_t inst)
{
	enum hqd_dequeue_request_type type;
	unsigned long end_jiffies;
	uint32_t temp;
	struct v9_mqd *m = get_mqd(mqd);

	if (amdgpu_in_reset(adev))
		return -EIO;

	kgd_gfx_v9_acquire_queue(adev, pipe_id, queue_id, inst);

	if (m->cp_hqd_vmid == 0)
		WREG32_FIELD15_RLC(GC, GET_INST(GC, inst), RLC_CP_SCHEDULERS, scheduler1, 0);

	switch (reset_type) {
	case KFD_PREEMPT_TYPE_WAVEFRONT_DRAIN:
		type = DRAIN_PIPE;
		break;
	case KFD_PREEMPT_TYPE_WAVEFRONT_RESET:
		type = RESET_WAVES;
		break;
	case KFD_PREEMPT_TYPE_WAVEFRONT_SAVE:
		type = SAVE_WAVES;
		break;
	default:
		type = DRAIN_PIPE;
		break;
	}

	WREG32_RLC(SOC15_REG_OFFSET(GC, GET_INST(GC, inst), mmCP_HQD_DEQUEUE_REQUEST), type);

	end_jiffies = (utimeout * HZ / 1000) + jiffies;
	while (true) {
		temp = RREG32_SOC15(GC, GET_INST(GC, inst), mmCP_HQD_ACTIVE);
		if (!(temp & CP_HQD_ACTIVE__ACTIVE_MASK))
			break;
		if (time_after(jiffies, end_jiffies)) {
			pr_err("cp queue preemption time out.\n");
			kgd_gfx_v9_release_queue(adev, inst);
			return -ETIME;
		}
		usleep_range(500, 1000);
	}

	kgd_gfx_v9_release_queue(adev, inst);
	return 0;
}

static int kgd_hqd_sdma_destroy(struct amdgpu_device *adev, void *mqd,
				unsigned int utimeout)
{
	struct v9_sdma_mqd *m;
	uint32_t sdma_rlc_reg_offset;
	uint32_t temp;
	unsigned long end_jiffies = (utimeout * HZ / 1000) + jiffies;

	m = get_sdma_mqd(mqd);
	sdma_rlc_reg_offset = get_sdma_rlc_reg_offset(adev, m->sdma_engine_id,
					    m->sdma_queue_id);

	temp = RREG32(sdma_rlc_reg_offset + mmSDMA0_RLC0_RB_CNTL);
	temp = temp & ~SDMA0_RLC0_RB_CNTL__RB_ENABLE_MASK;
	WREG32(sdma_rlc_reg_offset + mmSDMA0_RLC0_RB_CNTL, temp);

	while (true) {
		temp = RREG32(sdma_rlc_reg_offset + mmSDMA0_RLC0_CONTEXT_STATUS);
		if (temp & SDMA0_RLC0_CONTEXT_STATUS__IDLE_MASK)
			break;
		if (time_after(jiffies, end_jiffies)) {
			pr_err("SDMA RLC not idle in %s\n", __func__);
			return -ETIME;
		}
		usleep_range(500, 1000);
	}

	WREG32(sdma_rlc_reg_offset + mmSDMA0_RLC0_DOORBELL, 0);
	WREG32(sdma_rlc_reg_offset + mmSDMA0_RLC0_RB_CNTL,
		RREG32(sdma_rlc_reg_offset + mmSDMA0_RLC0_RB_CNTL) |
		SDMA0_RLC0_RB_CNTL__RB_ENABLE_MASK);

	m->sdmax_rlcx_rb_rptr = RREG32(sdma_rlc_reg_offset + mmSDMA0_RLC0_RB_RPTR);
	m->sdmax_rlcx_rb_rptr_hi =
		RREG32(sdma_rlc_reg_offset + mmSDMA0_RLC0_RB_RPTR_HI);

	return 0;
}

bool kgd_gfx_v9_get_atc_vmid_pasid_mapping_info(struct amdgpu_device *adev,
					uint8_t vmid, uint16_t *p_pasid)
{
	uint32_t value;

	value = RREG32(SOC15_REG_OFFSET(ATHUB, 0, mmATC_VMID0_PASID_MAPPING)
		     + vmid);
	*p_pasid = value & ATC_VMID0_PASID_MAPPING__PASID_MASK;

	return !!(value & ATC_VMID0_PASID_MAPPING__VALID_MASK);
}

int kgd_gfx_v9_wave_control_execute(struct amdgpu_device *adev,
					uint32_t gfx_index_val,
					uint32_t sq_cmd, uint32_t inst)
{
	uint32_t data = 0;

	mutex_lock(&adev->grbm_idx_mutex);

	WREG32_SOC15_RLC_SHADOW(GC, GET_INST(GC, inst), mmGRBM_GFX_INDEX, gfx_index_val);
	WREG32_SOC15(GC, GET_INST(GC, inst), mmSQ_CMD, sq_cmd);

	data = REG_SET_FIELD(data, GRBM_GFX_INDEX,
		INSTANCE_BROADCAST_WRITES, 1);
	data = REG_SET_FIELD(data, GRBM_GFX_INDEX,
		SH_BROADCAST_WRITES, 1);
	data = REG_SET_FIELD(data, GRBM_GFX_INDEX,
		SE_BROADCAST_WRITES, 1);

	WREG32_SOC15_RLC_SHADOW(GC, GET_INST(GC, inst), mmGRBM_GFX_INDEX, data);
	mutex_unlock(&adev->grbm_idx_mutex);

	return 0;
}

 
#define KGD_GFX_V9_WAVE_LAUNCH_SPI_DRAIN_LATENCY	3
void kgd_gfx_v9_set_wave_launch_stall(struct amdgpu_device *adev,
					uint32_t vmid,
					bool stall)
{
	int i;
	uint32_t data = RREG32(SOC15_REG_OFFSET(GC, 0, mmSPI_GDBG_WAVE_CNTL));

	if (adev->ip_versions[GC_HWIP][0] == IP_VERSION(9, 4, 1))
		data = REG_SET_FIELD(data, SPI_GDBG_WAVE_CNTL, STALL_VMID,
							stall ? 1 << vmid : 0);
	else
		data = REG_SET_FIELD(data, SPI_GDBG_WAVE_CNTL, STALL_RA,
							stall ? 1 : 0);

	WREG32(SOC15_REG_OFFSET(GC, 0, mmSPI_GDBG_WAVE_CNTL), data);

	if (!stall)
		return;

	for (i = 0; i < KGD_GFX_V9_WAVE_LAUNCH_SPI_DRAIN_LATENCY; i++)
		RREG32(SOC15_REG_OFFSET(GC, 0, mmSPI_GDBG_WAVE_CNTL));
}

 
uint32_t kgd_gfx_v9_enable_debug_trap(struct amdgpu_device *adev,
				bool restore_dbg_registers,
				uint32_t vmid)
{
	mutex_lock(&adev->grbm_idx_mutex);

	kgd_gfx_v9_set_wave_launch_stall(adev, vmid, true);

	WREG32(SOC15_REG_OFFSET(GC, 0, mmSPI_GDBG_TRAP_MASK), 0);

	kgd_gfx_v9_set_wave_launch_stall(adev, vmid, false);

	mutex_unlock(&adev->grbm_idx_mutex);

	return 0;
}

 
uint32_t kgd_gfx_v9_disable_debug_trap(struct amdgpu_device *adev,
					bool keep_trap_enabled,
					uint32_t vmid)
{
	mutex_lock(&adev->grbm_idx_mutex);

	kgd_gfx_v9_set_wave_launch_stall(adev, vmid, true);

	WREG32(SOC15_REG_OFFSET(GC, 0, mmSPI_GDBG_TRAP_MASK), 0);

	kgd_gfx_v9_set_wave_launch_stall(adev, vmid, false);

	mutex_unlock(&adev->grbm_idx_mutex);

	return 0;
}

int kgd_gfx_v9_validate_trap_override_request(struct amdgpu_device *adev,
					uint32_t trap_override,
					uint32_t *trap_mask_supported)
{
	*trap_mask_supported &= KFD_DBG_TRAP_MASK_DBG_ADDRESS_WATCH;

	 
	if (trap_override != KFD_DBG_TRAP_OVERRIDE_OR)
		return -EINVAL;

	return 0;
}

uint32_t kgd_gfx_v9_set_wave_launch_trap_override(struct amdgpu_device *adev,
					     uint32_t vmid,
					     uint32_t trap_override,
					     uint32_t trap_mask_bits,
					     uint32_t trap_mask_request,
					     uint32_t *trap_mask_prev,
					     uint32_t kfd_dbg_cntl_prev)
{
	uint32_t data, wave_cntl_prev;

	mutex_lock(&adev->grbm_idx_mutex);

	wave_cntl_prev = RREG32(SOC15_REG_OFFSET(GC, 0, mmSPI_GDBG_WAVE_CNTL));

	kgd_gfx_v9_set_wave_launch_stall(adev, vmid, true);

	data = RREG32(SOC15_REG_OFFSET(GC, 0, mmSPI_GDBG_TRAP_MASK));
	*trap_mask_prev = REG_GET_FIELD(data, SPI_GDBG_TRAP_MASK, EXCP_EN);

	trap_mask_bits = (trap_mask_bits & trap_mask_request) |
		(*trap_mask_prev & ~trap_mask_request);

	data = REG_SET_FIELD(data, SPI_GDBG_TRAP_MASK, EXCP_EN, trap_mask_bits);
	data = REG_SET_FIELD(data, SPI_GDBG_TRAP_MASK, REPLACE, trap_override);
	WREG32(SOC15_REG_OFFSET(GC, 0, mmSPI_GDBG_TRAP_MASK), data);

	 
	WREG32(SOC15_REG_OFFSET(GC, 0, mmSPI_GDBG_WAVE_CNTL), wave_cntl_prev);

	mutex_unlock(&adev->grbm_idx_mutex);

	return 0;
}

uint32_t kgd_gfx_v9_set_wave_launch_mode(struct amdgpu_device *adev,
					uint8_t wave_launch_mode,
					uint32_t vmid)
{
	uint32_t data = 0;
	bool is_mode_set = !!wave_launch_mode;

	mutex_lock(&adev->grbm_idx_mutex);

	kgd_gfx_v9_set_wave_launch_stall(adev, vmid, true);

	data = REG_SET_FIELD(data, SPI_GDBG_WAVE_CNTL2,
		VMID_MASK, is_mode_set ? 1 << vmid : 0);
	data = REG_SET_FIELD(data, SPI_GDBG_WAVE_CNTL2,
		MODE, is_mode_set ? wave_launch_mode : 0);
	WREG32(SOC15_REG_OFFSET(GC, 0, mmSPI_GDBG_WAVE_CNTL2), data);

	kgd_gfx_v9_set_wave_launch_stall(adev, vmid, false);

	mutex_unlock(&adev->grbm_idx_mutex);

	return 0;
}

#define TCP_WATCH_STRIDE (mmTCP_WATCH1_ADDR_H - mmTCP_WATCH0_ADDR_H)
uint32_t kgd_gfx_v9_set_address_watch(struct amdgpu_device *adev,
					uint64_t watch_address,
					uint32_t watch_address_mask,
					uint32_t watch_id,
					uint32_t watch_mode,
					uint32_t debug_vmid,
					uint32_t inst)
{
	uint32_t watch_address_high;
	uint32_t watch_address_low;
	uint32_t watch_address_cntl;

	watch_address_cntl = 0;

	watch_address_low = lower_32_bits(watch_address);
	watch_address_high = upper_32_bits(watch_address) & 0xffff;

	watch_address_cntl = REG_SET_FIELD(watch_address_cntl,
			TCP_WATCH0_CNTL,
			VMID,
			debug_vmid);
	watch_address_cntl = REG_SET_FIELD(watch_address_cntl,
			TCP_WATCH0_CNTL,
			MODE,
			watch_mode);
	watch_address_cntl = REG_SET_FIELD(watch_address_cntl,
			TCP_WATCH0_CNTL,
			MASK,
			watch_address_mask >> 6);

	 
	watch_address_cntl = REG_SET_FIELD(watch_address_cntl,
			TCP_WATCH0_CNTL,
			VALID,
			0);

	WREG32_RLC((SOC15_REG_OFFSET(GC, 0, mmTCP_WATCH0_CNTL) +
			(watch_id * TCP_WATCH_STRIDE)),
			watch_address_cntl);

	WREG32_RLC((SOC15_REG_OFFSET(GC, 0, mmTCP_WATCH0_ADDR_H) +
			(watch_id * TCP_WATCH_STRIDE)),
			watch_address_high);

	WREG32_RLC((SOC15_REG_OFFSET(GC, 0, mmTCP_WATCH0_ADDR_L) +
			(watch_id * TCP_WATCH_STRIDE)),
			watch_address_low);

	 
	watch_address_cntl = REG_SET_FIELD(watch_address_cntl,
			TCP_WATCH0_CNTL,
			VALID,
			1);

	WREG32_RLC((SOC15_REG_OFFSET(GC, 0, mmTCP_WATCH0_CNTL) +
			(watch_id * TCP_WATCH_STRIDE)),
			watch_address_cntl);

	return 0;
}

uint32_t kgd_gfx_v9_clear_address_watch(struct amdgpu_device *adev,
					uint32_t watch_id)
{
	uint32_t watch_address_cntl;

	watch_address_cntl = 0;

	WREG32_RLC((SOC15_REG_OFFSET(GC, 0, mmTCP_WATCH0_CNTL) +
			(watch_id * TCP_WATCH_STRIDE)),
			watch_address_cntl);

	return 0;
}

 
void kgd_gfx_v9_get_iq_wait_times(struct amdgpu_device *adev,
					uint32_t *wait_times,
					uint32_t inst)

{
	*wait_times = RREG32(SOC15_REG_OFFSET(GC, GET_INST(GC, inst),
			mmCP_IQ_WAIT_TIME2));
}

void kgd_gfx_v9_set_vm_context_page_table_base(struct amdgpu_device *adev,
			uint32_t vmid, uint64_t page_table_base)
{
	if (!amdgpu_amdkfd_is_kfd_vmid(adev, vmid)) {
		pr_err("trying to set page table base for wrong VMID %u\n",
		       vmid);
		return;
	}

	adev->mmhub.funcs->setup_vm_pt_regs(adev, vmid, page_table_base);

	adev->gfxhub.funcs->setup_vm_pt_regs(adev, vmid, page_table_base);
}

static void lock_spi_csq_mutexes(struct amdgpu_device *adev)
{
	mutex_lock(&adev->srbm_mutex);
	mutex_lock(&adev->grbm_idx_mutex);

}

static void unlock_spi_csq_mutexes(struct amdgpu_device *adev)
{
	mutex_unlock(&adev->grbm_idx_mutex);
	mutex_unlock(&adev->srbm_mutex);
}

 
static void get_wave_count(struct amdgpu_device *adev, int queue_idx,
		int *wave_cnt, int *vmid, uint32_t inst)
{
	int pipe_idx;
	int queue_slot;
	unsigned int reg_val;

	 
	*vmid = 0xFF;
	*wave_cnt = 0;
	pipe_idx = queue_idx / adev->gfx.mec.num_queue_per_pipe;
	queue_slot = queue_idx % adev->gfx.mec.num_queue_per_pipe;
	soc15_grbm_select(adev, 1, pipe_idx, queue_slot, 0, inst);
	reg_val = RREG32_SOC15_IP(GC, SOC15_REG_OFFSET(GC, inst, mmSPI_CSQ_WF_ACTIVE_COUNT_0) +
			 queue_slot);
	*wave_cnt = reg_val & SPI_CSQ_WF_ACTIVE_COUNT_0__COUNT_MASK;
	if (*wave_cnt != 0)
		*vmid = (RREG32_SOC15(GC, inst, mmCP_HQD_VMID) &
			 CP_HQD_VMID__VMID_MASK) >> CP_HQD_VMID__VMID__SHIFT;
}

 
void kgd_gfx_v9_get_cu_occupancy(struct amdgpu_device *adev, int pasid,
		int *pasid_wave_cnt, int *max_waves_per_cu, uint32_t inst)
{
	int qidx;
	int vmid;
	int se_idx;
	int sh_idx;
	int se_cnt;
	int sh_cnt;
	int wave_cnt;
	int queue_map;
	int pasid_tmp;
	int max_queue_cnt;
	int vmid_wave_cnt = 0;
	DECLARE_BITMAP(cp_queue_bitmap, KGD_MAX_QUEUES);

	lock_spi_csq_mutexes(adev);
	soc15_grbm_select(adev, 1, 0, 0, 0, inst);

	 
	bitmap_complement(cp_queue_bitmap, adev->gfx.mec_bitmap[0].queue_bitmap,
			  KGD_MAX_QUEUES);
	max_queue_cnt = adev->gfx.mec.num_pipe_per_mec *
			adev->gfx.mec.num_queue_per_pipe;
	sh_cnt = adev->gfx.config.max_sh_per_se;
	se_cnt = adev->gfx.config.max_shader_engines;
	for (se_idx = 0; se_idx < se_cnt; se_idx++) {
		for (sh_idx = 0; sh_idx < sh_cnt; sh_idx++) {

			amdgpu_gfx_select_se_sh(adev, se_idx, sh_idx, 0xffffffff, inst);
			queue_map = RREG32_SOC15(GC, inst, mmSPI_CSQ_WF_ACTIVE_STATUS);

			 
			for (qidx = 0; qidx < max_queue_cnt; qidx++) {

				 
				if (!test_bit(qidx, cp_queue_bitmap))
					continue;

				if (!(queue_map & (1 << qidx)))
					continue;

				 
				get_wave_count(adev, qidx, &wave_cnt, &vmid,
						inst);
				if (wave_cnt != 0) {
					pasid_tmp =
					  RREG32(SOC15_REG_OFFSET(OSSSYS, inst,
						 mmIH_VMID_0_LUT) + vmid);
					if (pasid_tmp == pasid)
						vmid_wave_cnt += wave_cnt;
				}
			}
		}
	}

	amdgpu_gfx_select_se_sh(adev, 0xffffffff, 0xffffffff, 0xffffffff, inst);
	soc15_grbm_select(adev, 0, 0, 0, 0, inst);
	unlock_spi_csq_mutexes(adev);

	 
	*pasid_wave_cnt = vmid_wave_cnt;
	*max_waves_per_cu = adev->gfx.cu_info.simd_per_cu *
				adev->gfx.cu_info.max_waves_per_simd;
}

void kgd_gfx_v9_build_grace_period_packet_info(struct amdgpu_device *adev,
		uint32_t wait_times,
		uint32_t grace_period,
		uint32_t *reg_offset,
		uint32_t *reg_data)
{
	*reg_data = wait_times;

	 
	if (grace_period == 0)
		grace_period = 1;

	*reg_data = REG_SET_FIELD(*reg_data,
			CP_IQ_WAIT_TIME2,
			SCH_WAVE,
			grace_period);

	*reg_offset = SOC15_REG_OFFSET(GC, 0, mmCP_IQ_WAIT_TIME2);
}

void kgd_gfx_v9_program_trap_handler_settings(struct amdgpu_device *adev,
		uint32_t vmid, uint64_t tba_addr, uint64_t tma_addr, uint32_t inst)
{
	kgd_gfx_v9_lock_srbm(adev, 0, 0, 0, vmid, inst);

	 
	WREG32_SOC15(GC, GET_INST(GC, inst), mmSQ_SHADER_TBA_LO,
			lower_32_bits(tba_addr >> 8));
	WREG32_SOC15(GC, GET_INST(GC, inst), mmSQ_SHADER_TBA_HI,
			upper_32_bits(tba_addr >> 8));

	 
	WREG32_SOC15(GC, GET_INST(GC, inst), mmSQ_SHADER_TMA_LO,
			lower_32_bits(tma_addr >> 8));
	WREG32_SOC15(GC, GET_INST(GC, inst), mmSQ_SHADER_TMA_HI,
			upper_32_bits(tma_addr >> 8));

	kgd_gfx_v9_unlock_srbm(adev, inst);
}

const struct kfd2kgd_calls gfx_v9_kfd2kgd = {
	.program_sh_mem_settings = kgd_gfx_v9_program_sh_mem_settings,
	.set_pasid_vmid_mapping = kgd_gfx_v9_set_pasid_vmid_mapping,
	.init_interrupts = kgd_gfx_v9_init_interrupts,
	.hqd_load = kgd_gfx_v9_hqd_load,
	.hiq_mqd_load = kgd_gfx_v9_hiq_mqd_load,
	.hqd_sdma_load = kgd_hqd_sdma_load,
	.hqd_dump = kgd_gfx_v9_hqd_dump,
	.hqd_sdma_dump = kgd_hqd_sdma_dump,
	.hqd_is_occupied = kgd_gfx_v9_hqd_is_occupied,
	.hqd_sdma_is_occupied = kgd_hqd_sdma_is_occupied,
	.hqd_destroy = kgd_gfx_v9_hqd_destroy,
	.hqd_sdma_destroy = kgd_hqd_sdma_destroy,
	.wave_control_execute = kgd_gfx_v9_wave_control_execute,
	.get_atc_vmid_pasid_mapping_info =
			kgd_gfx_v9_get_atc_vmid_pasid_mapping_info,
	.set_vm_context_page_table_base = kgd_gfx_v9_set_vm_context_page_table_base,
	.enable_debug_trap = kgd_gfx_v9_enable_debug_trap,
	.disable_debug_trap = kgd_gfx_v9_disable_debug_trap,
	.validate_trap_override_request = kgd_gfx_v9_validate_trap_override_request,
	.set_wave_launch_trap_override = kgd_gfx_v9_set_wave_launch_trap_override,
	.set_wave_launch_mode = kgd_gfx_v9_set_wave_launch_mode,
	.set_address_watch = kgd_gfx_v9_set_address_watch,
	.clear_address_watch = kgd_gfx_v9_clear_address_watch,
	.get_iq_wait_times = kgd_gfx_v9_get_iq_wait_times,
	.build_grace_period_packet_info = kgd_gfx_v9_build_grace_period_packet_info,
	.get_cu_occupancy = kgd_gfx_v9_get_cu_occupancy,
	.program_trap_handler_settings = kgd_gfx_v9_program_trap_handler_settings,
};
