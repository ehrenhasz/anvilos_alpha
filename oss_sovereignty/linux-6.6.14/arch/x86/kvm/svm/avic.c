
 

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/kvm_types.h>
#include <linux/hashtable.h>
#include <linux/amd-iommu.h>
#include <linux/kvm_host.h>

#include <asm/irq_remapping.h>

#include "trace.h"
#include "lapic.h"
#include "x86.h"
#include "irq.h"
#include "svm.h"

 
#define AVIC_VCPU_ID_MASK		AVIC_PHYSICAL_MAX_INDEX_MASK

#define AVIC_VM_ID_SHIFT		HWEIGHT32(AVIC_PHYSICAL_MAX_INDEX_MASK)
#define AVIC_VM_ID_MASK			(GENMASK(31, AVIC_VM_ID_SHIFT) >> AVIC_VM_ID_SHIFT)

#define AVIC_GATAG_TO_VMID(x)		((x >> AVIC_VM_ID_SHIFT) & AVIC_VM_ID_MASK)
#define AVIC_GATAG_TO_VCPUID(x)		(x & AVIC_VCPU_ID_MASK)

#define __AVIC_GATAG(vm_id, vcpu_id)	((((vm_id) & AVIC_VM_ID_MASK) << AVIC_VM_ID_SHIFT) | \
					 ((vcpu_id) & AVIC_VCPU_ID_MASK))
#define AVIC_GATAG(vm_id, vcpu_id)					\
({									\
	u32 ga_tag = __AVIC_GATAG(vm_id, vcpu_id);			\
									\
	WARN_ON_ONCE(AVIC_GATAG_TO_VCPUID(ga_tag) != (vcpu_id));	\
	WARN_ON_ONCE(AVIC_GATAG_TO_VMID(ga_tag) != (vm_id));		\
	ga_tag;								\
})

static_assert(__AVIC_GATAG(AVIC_VM_ID_MASK, AVIC_VCPU_ID_MASK) == -1u);

static bool force_avic;
module_param_unsafe(force_avic, bool, 0444);

 
#define SVM_VM_DATA_HASH_BITS	8
static DEFINE_HASHTABLE(svm_vm_data_hash, SVM_VM_DATA_HASH_BITS);
static u32 next_vm_id = 0;
static bool next_vm_id_wrapped = 0;
static DEFINE_SPINLOCK(svm_vm_data_hash_lock);
bool x2avic_enabled;

 
struct amd_svm_iommu_ir {
	struct list_head node;	 
	void *data;		 
};

static void avic_activate_vmcb(struct vcpu_svm *svm)
{
	struct vmcb *vmcb = svm->vmcb01.ptr;

	vmcb->control.int_ctl &= ~(AVIC_ENABLE_MASK | X2APIC_MODE_MASK);
	vmcb->control.avic_physical_id &= ~AVIC_PHYSICAL_MAX_INDEX_MASK;

	vmcb->control.int_ctl |= AVIC_ENABLE_MASK;

	 
	if (x2avic_enabled && apic_x2apic_mode(svm->vcpu.arch.apic)) {
		vmcb->control.int_ctl |= X2APIC_MODE_MASK;
		vmcb->control.avic_physical_id |= X2AVIC_MAX_PHYSICAL_ID;
		 
		svm_set_x2apic_msr_interception(svm, false);
	} else {
		 
		kvm_make_request(KVM_REQ_TLB_FLUSH_CURRENT, &svm->vcpu);

		 
		vmcb->control.avic_physical_id |= AVIC_MAX_PHYSICAL_ID;
		 
		svm_set_x2apic_msr_interception(svm, true);
	}
}

static void avic_deactivate_vmcb(struct vcpu_svm *svm)
{
	struct vmcb *vmcb = svm->vmcb01.ptr;

	vmcb->control.int_ctl &= ~(AVIC_ENABLE_MASK | X2APIC_MODE_MASK);
	vmcb->control.avic_physical_id &= ~AVIC_PHYSICAL_MAX_INDEX_MASK;

	 
	if (is_guest_mode(&svm->vcpu) &&
	    vmcb12_is_intercept(&svm->nested.ctl, INTERCEPT_MSR_PROT))
		return;

	 
	svm_set_x2apic_msr_interception(svm, true);
}

 
int avic_ga_log_notifier(u32 ga_tag)
{
	unsigned long flags;
	struct kvm_svm *kvm_svm;
	struct kvm_vcpu *vcpu = NULL;
	u32 vm_id = AVIC_GATAG_TO_VMID(ga_tag);
	u32 vcpu_id = AVIC_GATAG_TO_VCPUID(ga_tag);

	pr_debug("SVM: %s: vm_id=%#x, vcpu_id=%#x\n", __func__, vm_id, vcpu_id);
	trace_kvm_avic_ga_log(vm_id, vcpu_id);

	spin_lock_irqsave(&svm_vm_data_hash_lock, flags);
	hash_for_each_possible(svm_vm_data_hash, kvm_svm, hnode, vm_id) {
		if (kvm_svm->avic_vm_id != vm_id)
			continue;
		vcpu = kvm_get_vcpu_by_id(&kvm_svm->kvm, vcpu_id);
		break;
	}
	spin_unlock_irqrestore(&svm_vm_data_hash_lock, flags);

	 
	if (vcpu)
		kvm_vcpu_wake_up(vcpu);

	return 0;
}

void avic_vm_destroy(struct kvm *kvm)
{
	unsigned long flags;
	struct kvm_svm *kvm_svm = to_kvm_svm(kvm);

	if (!enable_apicv)
		return;

	if (kvm_svm->avic_logical_id_table_page)
		__free_page(kvm_svm->avic_logical_id_table_page);
	if (kvm_svm->avic_physical_id_table_page)
		__free_page(kvm_svm->avic_physical_id_table_page);

	spin_lock_irqsave(&svm_vm_data_hash_lock, flags);
	hash_del(&kvm_svm->hnode);
	spin_unlock_irqrestore(&svm_vm_data_hash_lock, flags);
}

int avic_vm_init(struct kvm *kvm)
{
	unsigned long flags;
	int err = -ENOMEM;
	struct kvm_svm *kvm_svm = to_kvm_svm(kvm);
	struct kvm_svm *k2;
	struct page *p_page;
	struct page *l_page;
	u32 vm_id;

	if (!enable_apicv)
		return 0;

	 
	p_page = alloc_page(GFP_KERNEL_ACCOUNT | __GFP_ZERO);
	if (!p_page)
		goto free_avic;

	kvm_svm->avic_physical_id_table_page = p_page;

	 
	l_page = alloc_page(GFP_KERNEL_ACCOUNT | __GFP_ZERO);
	if (!l_page)
		goto free_avic;

	kvm_svm->avic_logical_id_table_page = l_page;

	spin_lock_irqsave(&svm_vm_data_hash_lock, flags);
 again:
	vm_id = next_vm_id = (next_vm_id + 1) & AVIC_VM_ID_MASK;
	if (vm_id == 0) {  
		next_vm_id_wrapped = 1;
		goto again;
	}
	 
	if (next_vm_id_wrapped) {
		hash_for_each_possible(svm_vm_data_hash, k2, hnode, vm_id) {
			if (k2->avic_vm_id == vm_id)
				goto again;
		}
	}
	kvm_svm->avic_vm_id = vm_id;
	hash_add(svm_vm_data_hash, &kvm_svm->hnode, kvm_svm->avic_vm_id);
	spin_unlock_irqrestore(&svm_vm_data_hash_lock, flags);

	return 0;

free_avic:
	avic_vm_destroy(kvm);
	return err;
}

void avic_init_vmcb(struct vcpu_svm *svm, struct vmcb *vmcb)
{
	struct kvm_svm *kvm_svm = to_kvm_svm(svm->vcpu.kvm);
	phys_addr_t bpa = __sme_set(page_to_phys(svm->avic_backing_page));
	phys_addr_t lpa = __sme_set(page_to_phys(kvm_svm->avic_logical_id_table_page));
	phys_addr_t ppa = __sme_set(page_to_phys(kvm_svm->avic_physical_id_table_page));

	vmcb->control.avic_backing_page = bpa & AVIC_HPA_MASK;
	vmcb->control.avic_logical_id = lpa & AVIC_HPA_MASK;
	vmcb->control.avic_physical_id = ppa & AVIC_HPA_MASK;
	vmcb->control.avic_vapic_bar = APIC_DEFAULT_PHYS_BASE & VMCB_AVIC_APIC_BAR_MASK;

	if (kvm_apicv_activated(svm->vcpu.kvm))
		avic_activate_vmcb(svm);
	else
		avic_deactivate_vmcb(svm);
}

static u64 *avic_get_physical_id_entry(struct kvm_vcpu *vcpu,
				       unsigned int index)
{
	u64 *avic_physical_id_table;
	struct kvm_svm *kvm_svm = to_kvm_svm(vcpu->kvm);

	if ((!x2avic_enabled && index > AVIC_MAX_PHYSICAL_ID) ||
	    (index > X2AVIC_MAX_PHYSICAL_ID))
		return NULL;

	avic_physical_id_table = page_address(kvm_svm->avic_physical_id_table_page);

	return &avic_physical_id_table[index];
}

static int avic_init_backing_page(struct kvm_vcpu *vcpu)
{
	u64 *entry, new_entry;
	int id = vcpu->vcpu_id;
	struct vcpu_svm *svm = to_svm(vcpu);

	if ((!x2avic_enabled && id > AVIC_MAX_PHYSICAL_ID) ||
	    (id > X2AVIC_MAX_PHYSICAL_ID))
		return -EINVAL;

	if (!vcpu->arch.apic->regs)
		return -EINVAL;

	if (kvm_apicv_activated(vcpu->kvm)) {
		int ret;

		 
		ret = kvm_alloc_apic_access_page(vcpu->kvm);
		if (ret)
			return ret;
	}

	svm->avic_backing_page = virt_to_page(vcpu->arch.apic->regs);

	 
	entry = avic_get_physical_id_entry(vcpu, id);
	if (!entry)
		return -EINVAL;

	new_entry = __sme_set((page_to_phys(svm->avic_backing_page) &
			      AVIC_PHYSICAL_ID_ENTRY_BACKING_PAGE_MASK) |
			      AVIC_PHYSICAL_ID_ENTRY_VALID_MASK);
	WRITE_ONCE(*entry, new_entry);

	svm->avic_physical_id_cache = entry;

	return 0;
}

void avic_ring_doorbell(struct kvm_vcpu *vcpu)
{
	 
	int cpu = READ_ONCE(vcpu->cpu);

	if (cpu != get_cpu()) {
		wrmsrl(MSR_AMD64_SVM_AVIC_DOORBELL, kvm_cpu_get_apicid(cpu));
		trace_kvm_avic_doorbell(vcpu->vcpu_id, kvm_cpu_get_apicid(cpu));
	}
	put_cpu();
}


static void avic_kick_vcpu(struct kvm_vcpu *vcpu, u32 icrl)
{
	vcpu->arch.apic->irr_pending = true;
	svm_complete_interrupt_delivery(vcpu,
					icrl & APIC_MODE_MASK,
					icrl & APIC_INT_LEVELTRIG,
					icrl & APIC_VECTOR_MASK);
}

static void avic_kick_vcpu_by_physical_id(struct kvm *kvm, u32 physical_id,
					  u32 icrl)
{
	 
	struct kvm_vcpu *target_vcpu = kvm_get_vcpu_by_id(kvm, physical_id);

	 
	if (unlikely(!target_vcpu))
		return;

	avic_kick_vcpu(target_vcpu, icrl);
}

static void avic_kick_vcpu_by_logical_id(struct kvm *kvm, u32 *avic_logical_id_table,
					 u32 logid_index, u32 icrl)
{
	u32 physical_id;

	if (avic_logical_id_table) {
		u32 logid_entry = avic_logical_id_table[logid_index];

		 
		if (unlikely(!(logid_entry & AVIC_LOGICAL_ID_ENTRY_VALID_MASK)))
			return;

		physical_id = logid_entry &
			      AVIC_LOGICAL_ID_ENTRY_GUEST_PHYSICAL_ID_MASK;
	} else {
		 
		physical_id = logid_index;
	}

	avic_kick_vcpu_by_physical_id(kvm, physical_id, icrl);
}

 
static int avic_kick_target_vcpus_fast(struct kvm *kvm, struct kvm_lapic *source,
				       u32 icrl, u32 icrh, u32 index)
{
	int dest_mode = icrl & APIC_DEST_MASK;
	int shorthand = icrl & APIC_SHORT_MASK;
	struct kvm_svm *kvm_svm = to_kvm_svm(kvm);
	u32 dest;

	if (shorthand != APIC_DEST_NOSHORT)
		return -EINVAL;

	if (apic_x2apic_mode(source))
		dest = icrh;
	else
		dest = GET_XAPIC_DEST_FIELD(icrh);

	if (dest_mode == APIC_DEST_PHYSICAL) {
		 
		if (apic_x2apic_mode(source) && dest == X2APIC_BROADCAST)
			return -EINVAL;
		if (!apic_x2apic_mode(source) && dest == APIC_BROADCAST)
			return -EINVAL;

		if (WARN_ON_ONCE(dest != index))
			return -EINVAL;

		avic_kick_vcpu_by_physical_id(kvm, dest, icrl);
	} else {
		u32 *avic_logical_id_table;
		unsigned long bitmap, i;
		u32 cluster;

		if (apic_x2apic_mode(source)) {
			 
			bitmap = dest & 0xFFFF;
			cluster = (dest >> 16) << 4;
		} else if (kvm_lapic_get_reg(source, APIC_DFR) == APIC_DFR_FLAT) {
			 
			bitmap = dest;
			cluster = 0;
		} else {
			 
			bitmap = dest & 0xF;
			cluster = (dest >> 4) << 2;
		}

		 
		if (unlikely(!bitmap))
			return 0;

		if (apic_x2apic_mode(source))
			avic_logical_id_table = NULL;
		else
			avic_logical_id_table = page_address(kvm_svm->avic_logical_id_table_page);

		 
		for_each_set_bit(i, &bitmap, 16)
			avic_kick_vcpu_by_logical_id(kvm, avic_logical_id_table,
						     cluster + i, icrl);
	}

	return 0;
}

static void avic_kick_target_vcpus(struct kvm *kvm, struct kvm_lapic *source,
				   u32 icrl, u32 icrh, u32 index)
{
	u32 dest = apic_x2apic_mode(source) ? icrh : GET_XAPIC_DEST_FIELD(icrh);
	unsigned long i;
	struct kvm_vcpu *vcpu;

	if (!avic_kick_target_vcpus_fast(kvm, source, icrl, icrh, index))
		return;

	trace_kvm_avic_kick_vcpu_slowpath(icrh, icrl, index);

	 
	kvm_for_each_vcpu(i, vcpu, kvm) {
		if (kvm_apic_match_dest(vcpu, source, icrl & APIC_SHORT_MASK,
					dest, icrl & APIC_DEST_MASK))
			avic_kick_vcpu(vcpu, icrl);
	}
}

int avic_incomplete_ipi_interception(struct kvm_vcpu *vcpu)
{
	struct vcpu_svm *svm = to_svm(vcpu);
	u32 icrh = svm->vmcb->control.exit_info_1 >> 32;
	u32 icrl = svm->vmcb->control.exit_info_1;
	u32 id = svm->vmcb->control.exit_info_2 >> 32;
	u32 index = svm->vmcb->control.exit_info_2 & 0x1FF;
	struct kvm_lapic *apic = vcpu->arch.apic;

	trace_kvm_avic_incomplete_ipi(vcpu->vcpu_id, icrh, icrl, id, index);

	switch (id) {
	case AVIC_IPI_FAILURE_INVALID_TARGET:
	case AVIC_IPI_FAILURE_INVALID_INT_TYPE:
		 
		if (icrl & APIC_ICR_BUSY)
			kvm_apic_write_nodecode(vcpu, APIC_ICR);
		else
			kvm_apic_send_ipi(apic, icrl, icrh);
		break;
	case AVIC_IPI_FAILURE_TARGET_NOT_RUNNING:
		 
		avic_kick_target_vcpus(vcpu->kvm, apic, icrl, icrh, index);
		break;
	case AVIC_IPI_FAILURE_INVALID_BACKING_PAGE:
		WARN_ONCE(1, "Invalid backing page\n");
		break;
	case AVIC_IPI_FAILURE_INVALID_IPI_VECTOR:
		 
		break;
	default:
		vcpu_unimpl(vcpu, "Unknown avic incomplete IPI interception\n");
	}

	return 1;
}

unsigned long avic_vcpu_get_apicv_inhibit_reasons(struct kvm_vcpu *vcpu)
{
	if (is_guest_mode(vcpu))
		return APICV_INHIBIT_REASON_NESTED;
	return 0;
}

static u32 *avic_get_logical_id_entry(struct kvm_vcpu *vcpu, u32 ldr, bool flat)
{
	struct kvm_svm *kvm_svm = to_kvm_svm(vcpu->kvm);
	u32 *logical_apic_id_table;
	u32 cluster, index;

	ldr = GET_APIC_LOGICAL_ID(ldr);

	if (flat) {
		cluster = 0;
	} else {
		cluster = (ldr >> 4);
		if (cluster >= 0xf)
			return NULL;
		ldr &= 0xf;
	}
	if (!ldr || !is_power_of_2(ldr))
		return NULL;

	index = __ffs(ldr);
	if (WARN_ON_ONCE(index > 7))
		return NULL;
	index += (cluster << 2);

	logical_apic_id_table = (u32 *) page_address(kvm_svm->avic_logical_id_table_page);

	return &logical_apic_id_table[index];
}

static void avic_ldr_write(struct kvm_vcpu *vcpu, u8 g_physical_id, u32 ldr)
{
	bool flat;
	u32 *entry, new_entry;

	flat = kvm_lapic_get_reg(vcpu->arch.apic, APIC_DFR) == APIC_DFR_FLAT;
	entry = avic_get_logical_id_entry(vcpu, ldr, flat);
	if (!entry)
		return;

	new_entry = READ_ONCE(*entry);
	new_entry &= ~AVIC_LOGICAL_ID_ENTRY_GUEST_PHYSICAL_ID_MASK;
	new_entry |= (g_physical_id & AVIC_LOGICAL_ID_ENTRY_GUEST_PHYSICAL_ID_MASK);
	new_entry |= AVIC_LOGICAL_ID_ENTRY_VALID_MASK;
	WRITE_ONCE(*entry, new_entry);
}

static void avic_invalidate_logical_id_entry(struct kvm_vcpu *vcpu)
{
	struct vcpu_svm *svm = to_svm(vcpu);
	bool flat = svm->dfr_reg == APIC_DFR_FLAT;
	u32 *entry;

	 
	if (apic_x2apic_mode(vcpu->arch.apic))
		return;

	entry = avic_get_logical_id_entry(vcpu, svm->ldr_reg, flat);
	if (entry)
		clear_bit(AVIC_LOGICAL_ID_ENTRY_VALID_BIT, (unsigned long *)entry);
}

static void avic_handle_ldr_update(struct kvm_vcpu *vcpu)
{
	struct vcpu_svm *svm = to_svm(vcpu);
	u32 ldr = kvm_lapic_get_reg(vcpu->arch.apic, APIC_LDR);
	u32 id = kvm_xapic_id(vcpu->arch.apic);

	 
	if (apic_x2apic_mode(vcpu->arch.apic))
		return;

	if (ldr == svm->ldr_reg)
		return;

	avic_invalidate_logical_id_entry(vcpu);

	svm->ldr_reg = ldr;
	avic_ldr_write(vcpu, id, ldr);
}

static void avic_handle_dfr_update(struct kvm_vcpu *vcpu)
{
	struct vcpu_svm *svm = to_svm(vcpu);
	u32 dfr = kvm_lapic_get_reg(vcpu->arch.apic, APIC_DFR);

	if (svm->dfr_reg == dfr)
		return;

	avic_invalidate_logical_id_entry(vcpu);
	svm->dfr_reg = dfr;
}

static int avic_unaccel_trap_write(struct kvm_vcpu *vcpu)
{
	u32 offset = to_svm(vcpu)->vmcb->control.exit_info_1 &
				AVIC_UNACCEL_ACCESS_OFFSET_MASK;

	switch (offset) {
	case APIC_LDR:
		avic_handle_ldr_update(vcpu);
		break;
	case APIC_DFR:
		avic_handle_dfr_update(vcpu);
		break;
	case APIC_RRR:
		 
		return 1;
	default:
		break;
	}

	kvm_apic_write_nodecode(vcpu, offset);
	return 1;
}

static bool is_avic_unaccelerated_access_trap(u32 offset)
{
	bool ret = false;

	switch (offset) {
	case APIC_ID:
	case APIC_EOI:
	case APIC_RRR:
	case APIC_LDR:
	case APIC_DFR:
	case APIC_SPIV:
	case APIC_ESR:
	case APIC_ICR:
	case APIC_LVTT:
	case APIC_LVTTHMR:
	case APIC_LVTPC:
	case APIC_LVT0:
	case APIC_LVT1:
	case APIC_LVTERR:
	case APIC_TMICT:
	case APIC_TDCR:
		ret = true;
		break;
	default:
		break;
	}
	return ret;
}

int avic_unaccelerated_access_interception(struct kvm_vcpu *vcpu)
{
	struct vcpu_svm *svm = to_svm(vcpu);
	int ret = 0;
	u32 offset = svm->vmcb->control.exit_info_1 &
		     AVIC_UNACCEL_ACCESS_OFFSET_MASK;
	u32 vector = svm->vmcb->control.exit_info_2 &
		     AVIC_UNACCEL_ACCESS_VECTOR_MASK;
	bool write = (svm->vmcb->control.exit_info_1 >> 32) &
		     AVIC_UNACCEL_ACCESS_WRITE_MASK;
	bool trap = is_avic_unaccelerated_access_trap(offset);

	trace_kvm_avic_unaccelerated_access(vcpu->vcpu_id, offset,
					    trap, write, vector);
	if (trap) {
		 
		WARN_ONCE(!write, "svm: Handling trap read.\n");
		ret = avic_unaccel_trap_write(vcpu);
	} else {
		 
		ret = kvm_emulate_instruction(vcpu, 0);
	}

	return ret;
}

int avic_init_vcpu(struct vcpu_svm *svm)
{
	int ret;
	struct kvm_vcpu *vcpu = &svm->vcpu;

	if (!enable_apicv || !irqchip_in_kernel(vcpu->kvm))
		return 0;

	ret = avic_init_backing_page(vcpu);
	if (ret)
		return ret;

	INIT_LIST_HEAD(&svm->ir_list);
	spin_lock_init(&svm->ir_list_lock);
	svm->dfr_reg = APIC_DFR_FLAT;

	return ret;
}

void avic_apicv_post_state_restore(struct kvm_vcpu *vcpu)
{
	avic_handle_dfr_update(vcpu);
	avic_handle_ldr_update(vcpu);
}

static int avic_set_pi_irte_mode(struct kvm_vcpu *vcpu, bool activate)
{
	int ret = 0;
	unsigned long flags;
	struct amd_svm_iommu_ir *ir;
	struct vcpu_svm *svm = to_svm(vcpu);

	if (!kvm_arch_has_assigned_device(vcpu->kvm))
		return 0;

	 
	spin_lock_irqsave(&svm->ir_list_lock, flags);

	if (list_empty(&svm->ir_list))
		goto out;

	list_for_each_entry(ir, &svm->ir_list, node) {
		if (activate)
			ret = amd_iommu_activate_guest_mode(ir->data);
		else
			ret = amd_iommu_deactivate_guest_mode(ir->data);
		if (ret)
			break;
	}
out:
	spin_unlock_irqrestore(&svm->ir_list_lock, flags);
	return ret;
}

static void svm_ir_list_del(struct vcpu_svm *svm, struct amd_iommu_pi_data *pi)
{
	unsigned long flags;
	struct amd_svm_iommu_ir *cur;

	spin_lock_irqsave(&svm->ir_list_lock, flags);
	list_for_each_entry(cur, &svm->ir_list, node) {
		if (cur->data != pi->ir_data)
			continue;
		list_del(&cur->node);
		kfree(cur);
		break;
	}
	spin_unlock_irqrestore(&svm->ir_list_lock, flags);
}

static int svm_ir_list_add(struct vcpu_svm *svm, struct amd_iommu_pi_data *pi)
{
	int ret = 0;
	unsigned long flags;
	struct amd_svm_iommu_ir *ir;
	u64 entry;

	 
	if (pi->ir_data && (pi->prev_ga_tag != 0)) {
		struct kvm *kvm = svm->vcpu.kvm;
		u32 vcpu_id = AVIC_GATAG_TO_VCPUID(pi->prev_ga_tag);
		struct kvm_vcpu *prev_vcpu = kvm_get_vcpu_by_id(kvm, vcpu_id);
		struct vcpu_svm *prev_svm;

		if (!prev_vcpu) {
			ret = -EINVAL;
			goto out;
		}

		prev_svm = to_svm(prev_vcpu);
		svm_ir_list_del(prev_svm, pi);
	}

	 
	ir = kzalloc(sizeof(struct amd_svm_iommu_ir), GFP_KERNEL_ACCOUNT);
	if (!ir) {
		ret = -ENOMEM;
		goto out;
	}
	ir->data = pi->ir_data;

	spin_lock_irqsave(&svm->ir_list_lock, flags);

	 
	entry = READ_ONCE(*(svm->avic_physical_id_cache));
	if (entry & AVIC_PHYSICAL_ID_ENTRY_IS_RUNNING_MASK)
		amd_iommu_update_ga(entry & AVIC_PHYSICAL_ID_ENTRY_HOST_PHYSICAL_ID_MASK,
				    true, pi->ir_data);

	list_add(&ir->node, &svm->ir_list);
	spin_unlock_irqrestore(&svm->ir_list_lock, flags);
out:
	return ret;
}

 
static int
get_pi_vcpu_info(struct kvm *kvm, struct kvm_kernel_irq_routing_entry *e,
		 struct vcpu_data *vcpu_info, struct vcpu_svm **svm)
{
	struct kvm_lapic_irq irq;
	struct kvm_vcpu *vcpu = NULL;

	kvm_set_msi_irq(kvm, e, &irq);

	if (!kvm_intr_is_single_vcpu(kvm, &irq, &vcpu) ||
	    !kvm_irq_is_postable(&irq)) {
		pr_debug("SVM: %s: use legacy intr remap mode for irq %u\n",
			 __func__, irq.vector);
		return -1;
	}

	pr_debug("SVM: %s: use GA mode for irq %u\n", __func__,
		 irq.vector);
	*svm = to_svm(vcpu);
	vcpu_info->pi_desc_addr = __sme_set(page_to_phys((*svm)->avic_backing_page));
	vcpu_info->vector = irq.vector;

	return 0;
}

 
int avic_pi_update_irte(struct kvm *kvm, unsigned int host_irq,
			uint32_t guest_irq, bool set)
{
	struct kvm_kernel_irq_routing_entry *e;
	struct kvm_irq_routing_table *irq_rt;
	int idx, ret = 0;

	if (!kvm_arch_has_assigned_device(kvm) ||
	    !irq_remapping_cap(IRQ_POSTING_CAP))
		return 0;

	pr_debug("SVM: %s: host_irq=%#x, guest_irq=%#x, set=%#x\n",
		 __func__, host_irq, guest_irq, set);

	idx = srcu_read_lock(&kvm->irq_srcu);
	irq_rt = srcu_dereference(kvm->irq_routing, &kvm->irq_srcu);

	if (guest_irq >= irq_rt->nr_rt_entries ||
		hlist_empty(&irq_rt->map[guest_irq])) {
		pr_warn_once("no route for guest_irq %u/%u (broken user space?)\n",
			     guest_irq, irq_rt->nr_rt_entries);
		goto out;
	}

	hlist_for_each_entry(e, &irq_rt->map[guest_irq], link) {
		struct vcpu_data vcpu_info;
		struct vcpu_svm *svm = NULL;

		if (e->type != KVM_IRQ_ROUTING_MSI)
			continue;

		 
		if (!get_pi_vcpu_info(kvm, e, &vcpu_info, &svm) && set &&
		    kvm_vcpu_apicv_active(&svm->vcpu)) {
			struct amd_iommu_pi_data pi;

			 
			pi.base = __sme_set(page_to_phys(svm->avic_backing_page) &
					    AVIC_HPA_MASK);
			pi.ga_tag = AVIC_GATAG(to_kvm_svm(kvm)->avic_vm_id,
						     svm->vcpu.vcpu_id);
			pi.is_guest_mode = true;
			pi.vcpu_data = &vcpu_info;
			ret = irq_set_vcpu_affinity(host_irq, &pi);

			 
			if (!ret && pi.is_guest_mode)
				svm_ir_list_add(svm, &pi);
		} else {
			 
			struct amd_iommu_pi_data pi;

			 
			pi.prev_ga_tag = 0;
			pi.is_guest_mode = false;
			ret = irq_set_vcpu_affinity(host_irq, &pi);

			 
			if (!ret && pi.prev_ga_tag) {
				int id = AVIC_GATAG_TO_VCPUID(pi.prev_ga_tag);
				struct kvm_vcpu *vcpu;

				vcpu = kvm_get_vcpu_by_id(kvm, id);
				if (vcpu)
					svm_ir_list_del(to_svm(vcpu), &pi);
			}
		}

		if (!ret && svm) {
			trace_kvm_pi_irte_update(host_irq, svm->vcpu.vcpu_id,
						 e->gsi, vcpu_info.vector,
						 vcpu_info.pi_desc_addr, set);
		}

		if (ret < 0) {
			pr_err("%s: failed to update PI IRTE\n", __func__);
			goto out;
		}
	}

	ret = 0;
out:
	srcu_read_unlock(&kvm->irq_srcu, idx);
	return ret;
}

static inline int
avic_update_iommu_vcpu_affinity(struct kvm_vcpu *vcpu, int cpu, bool r)
{
	int ret = 0;
	struct amd_svm_iommu_ir *ir;
	struct vcpu_svm *svm = to_svm(vcpu);

	lockdep_assert_held(&svm->ir_list_lock);

	if (!kvm_arch_has_assigned_device(vcpu->kvm))
		return 0;

	 
	if (list_empty(&svm->ir_list))
		return 0;

	list_for_each_entry(ir, &svm->ir_list, node) {
		ret = amd_iommu_update_ga(cpu, r, ir->data);
		if (ret)
			return ret;
	}
	return 0;
}

void avic_vcpu_load(struct kvm_vcpu *vcpu, int cpu)
{
	u64 entry;
	int h_physical_id = kvm_cpu_get_apicid(cpu);
	struct vcpu_svm *svm = to_svm(vcpu);
	unsigned long flags;

	lockdep_assert_preemption_disabled();

	if (WARN_ON(h_physical_id & ~AVIC_PHYSICAL_ID_ENTRY_HOST_PHYSICAL_ID_MASK))
		return;

	 
	if (kvm_vcpu_is_blocking(vcpu))
		return;

	 
	spin_lock_irqsave(&svm->ir_list_lock, flags);

	entry = READ_ONCE(*(svm->avic_physical_id_cache));
	WARN_ON_ONCE(entry & AVIC_PHYSICAL_ID_ENTRY_IS_RUNNING_MASK);

	entry &= ~AVIC_PHYSICAL_ID_ENTRY_HOST_PHYSICAL_ID_MASK;
	entry |= (h_physical_id & AVIC_PHYSICAL_ID_ENTRY_HOST_PHYSICAL_ID_MASK);
	entry |= AVIC_PHYSICAL_ID_ENTRY_IS_RUNNING_MASK;

	WRITE_ONCE(*(svm->avic_physical_id_cache), entry);
	avic_update_iommu_vcpu_affinity(vcpu, h_physical_id, true);

	spin_unlock_irqrestore(&svm->ir_list_lock, flags);
}

void avic_vcpu_put(struct kvm_vcpu *vcpu)
{
	u64 entry;
	struct vcpu_svm *svm = to_svm(vcpu);
	unsigned long flags;

	lockdep_assert_preemption_disabled();

	 
	entry = READ_ONCE(*(svm->avic_physical_id_cache));

	 
	if (!(entry & AVIC_PHYSICAL_ID_ENTRY_IS_RUNNING_MASK))
		return;

	 
	spin_lock_irqsave(&svm->ir_list_lock, flags);

	avic_update_iommu_vcpu_affinity(vcpu, -1, 0);

	entry &= ~AVIC_PHYSICAL_ID_ENTRY_IS_RUNNING_MASK;
	WRITE_ONCE(*(svm->avic_physical_id_cache), entry);

	spin_unlock_irqrestore(&svm->ir_list_lock, flags);

}

void avic_refresh_virtual_apic_mode(struct kvm_vcpu *vcpu)
{
	struct vcpu_svm *svm = to_svm(vcpu);
	struct vmcb *vmcb = svm->vmcb01.ptr;

	if (!lapic_in_kernel(vcpu) || !enable_apicv)
		return;

	if (kvm_vcpu_apicv_active(vcpu)) {
		 
		avic_apicv_post_state_restore(vcpu);
		avic_activate_vmcb(svm);
	} else {
		avic_deactivate_vmcb(svm);
	}
	vmcb_mark_dirty(vmcb, VMCB_AVIC);
}

void avic_refresh_apicv_exec_ctrl(struct kvm_vcpu *vcpu)
{
	bool activated = kvm_vcpu_apicv_active(vcpu);

	if (!enable_apicv)
		return;

	avic_refresh_virtual_apic_mode(vcpu);

	if (activated)
		avic_vcpu_load(vcpu, vcpu->cpu);
	else
		avic_vcpu_put(vcpu);

	avic_set_pi_irte_mode(vcpu, activated);
}

void avic_vcpu_blocking(struct kvm_vcpu *vcpu)
{
	if (!kvm_vcpu_apicv_active(vcpu))
		return;

        
	avic_vcpu_put(vcpu);
}

void avic_vcpu_unblocking(struct kvm_vcpu *vcpu)
{
	if (!kvm_vcpu_apicv_active(vcpu))
		return;

	avic_vcpu_load(vcpu, vcpu->cpu);
}

 
bool avic_hardware_setup(void)
{
	if (!npt_enabled)
		return false;

	 
	if (!boot_cpu_has(X86_FEATURE_AVIC) && !force_avic) {
		if (boot_cpu_has(X86_FEATURE_X2AVIC)) {
			pr_warn(FW_BUG "Cannot support x2AVIC due to AVIC is disabled");
			pr_warn(FW_BUG "Try enable AVIC using force_avic option");
		}
		return false;
	}

	if (boot_cpu_has(X86_FEATURE_AVIC)) {
		pr_info("AVIC enabled\n");
	} else if (force_avic) {
		 
		pr_warn("AVIC is not supported in CPUID but force enabled");
		pr_warn("Your system might crash and burn");
	}

	 
	x2avic_enabled = boot_cpu_has(X86_FEATURE_X2AVIC);
	if (x2avic_enabled)
		pr_info("x2AVIC enabled\n");

	amd_iommu_register_ga_log_notifier(&avic_ga_log_notifier);

	return true;
}
