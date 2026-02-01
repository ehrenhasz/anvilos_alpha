 

#include "kfd_priv.h"
#include "kfd_events.h"
#include "cik_int.h"
#include "amdgpu_amdkfd.h"
#include "kfd_smi_events.h"

static bool cik_event_interrupt_isr(struct kfd_node *dev,
					const uint32_t *ih_ring_entry,
					uint32_t *patched_ihre,
					bool *patched_flag)
{
	const struct cik_ih_ring_entry *ihre =
			(const struct cik_ih_ring_entry *)ih_ring_entry;
	const struct kfd2kgd_calls *f2g = dev->kfd2kgd;
	unsigned int vmid;
	uint16_t pasid;
	bool ret;

	 
	if ((ihre->source_id == CIK_INTSRC_GFX_PAGE_INV_FAULT ||
		ihre->source_id == CIK_INTSRC_GFX_MEM_PROT_FAULT) &&
		dev->adev->asic_type == CHIP_HAWAII) {
		struct cik_ih_ring_entry *tmp_ihre =
			(struct cik_ih_ring_entry *)patched_ihre;

		*patched_flag = true;
		*tmp_ihre = *ihre;

		vmid = f2g->read_vmid_from_vmfault_reg(dev->adev);
		ret = f2g->get_atc_vmid_pasid_mapping_info(dev->adev, vmid, &pasid);

		tmp_ihre->ring_id &= 0x000000ff;
		tmp_ihre->ring_id |= vmid << 8;
		tmp_ihre->ring_id |= pasid << 16;

		return ret && (pasid != 0) &&
			vmid >= dev->vm_info.first_vmid_kfd &&
			vmid <= dev->vm_info.last_vmid_kfd;
	}

	 
	vmid  = (ihre->ring_id & 0x0000ff00) >> 8;
	if (vmid < dev->vm_info.first_vmid_kfd ||
	    vmid > dev->vm_info.last_vmid_kfd)
		return false;

	 
	pasid = (ihre->ring_id & 0xffff0000) >> 16;
	if (WARN_ONCE(pasid == 0, "FW bug: No PASID in KFD interrupt"))
		return false;

	 
	return ihre->source_id == CIK_INTSRC_CP_END_OF_PIPE ||
		ihre->source_id == CIK_INTSRC_SDMA_TRAP ||
		ihre->source_id == CIK_INTSRC_SQ_INTERRUPT_MSG ||
		ihre->source_id == CIK_INTSRC_CP_BAD_OPCODE ||
		((ihre->source_id == CIK_INTSRC_GFX_PAGE_INV_FAULT ||
		ihre->source_id == CIK_INTSRC_GFX_MEM_PROT_FAULT) &&
		!amdgpu_no_queue_eviction_on_vm_fault);
}

static void cik_event_interrupt_wq(struct kfd_node *dev,
					const uint32_t *ih_ring_entry)
{
	const struct cik_ih_ring_entry *ihre =
			(const struct cik_ih_ring_entry *)ih_ring_entry;
	uint32_t context_id = ihre->data & 0xfffffff;
	unsigned int vmid  = (ihre->ring_id & 0x0000ff00) >> 8;
	u32 pasid = (ihre->ring_id & 0xffff0000) >> 16;

	if (pasid == 0)
		return;

	if (ihre->source_id == CIK_INTSRC_CP_END_OF_PIPE)
		kfd_signal_event_interrupt(pasid, context_id, 28);
	else if (ihre->source_id == CIK_INTSRC_SDMA_TRAP)
		kfd_signal_event_interrupt(pasid, context_id, 28);
	else if (ihre->source_id == CIK_INTSRC_SQ_INTERRUPT_MSG)
		kfd_signal_event_interrupt(pasid, context_id & 0xff, 8);
	else if (ihre->source_id == CIK_INTSRC_CP_BAD_OPCODE)
		kfd_signal_hw_exception_event(pasid);
	else if (ihre->source_id == CIK_INTSRC_GFX_PAGE_INV_FAULT ||
		ihre->source_id == CIK_INTSRC_GFX_MEM_PROT_FAULT) {
		struct kfd_vm_fault_info info;

		kfd_smi_event_update_vmfault(dev, pasid);
		kfd_dqm_evict_pasid(dev->dqm, pasid);

		memset(&info, 0, sizeof(info));
		amdgpu_amdkfd_gpuvm_get_vm_fault_info(dev->adev, &info);
		if (!info.page_addr && !info.status)
			return;

		if (info.vmid == vmid)
			kfd_signal_vm_fault_event(dev, pasid, &info, NULL);
		else
			kfd_signal_vm_fault_event(dev, pasid, NULL, NULL);
	}
}

const struct kfd_event_interrupt_class event_interrupt_class_cik = {
	.interrupt_isr = cik_event_interrupt_isr,
	.interrupt_wq = cik_event_interrupt_wq,
};
