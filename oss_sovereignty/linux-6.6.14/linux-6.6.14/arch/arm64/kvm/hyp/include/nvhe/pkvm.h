#ifndef __ARM64_KVM_NVHE_PKVM_H__
#define __ARM64_KVM_NVHE_PKVM_H__
#include <asm/kvm_pkvm.h>
#include <nvhe/gfp.h>
#include <nvhe/spinlock.h>
struct pkvm_hyp_vcpu {
	struct kvm_vcpu vcpu;
	struct kvm_vcpu *host_vcpu;
};
struct pkvm_hyp_vm {
	struct kvm kvm;
	struct kvm *host_kvm;
	struct kvm_pgtable pgt;
	struct kvm_pgtable_mm_ops mm_ops;
	struct hyp_pool pool;
	hyp_spinlock_t lock;
	unsigned int nr_vcpus;
	struct pkvm_hyp_vcpu *vcpus[];
};
static inline struct pkvm_hyp_vm *
pkvm_hyp_vcpu_to_hyp_vm(struct pkvm_hyp_vcpu *hyp_vcpu)
{
	return container_of(hyp_vcpu->vcpu.kvm, struct pkvm_hyp_vm, kvm);
}
void pkvm_hyp_vm_table_init(void *tbl);
int __pkvm_init_vm(struct kvm *host_kvm, unsigned long vm_hva,
		   unsigned long pgd_hva);
int __pkvm_init_vcpu(pkvm_handle_t handle, struct kvm_vcpu *host_vcpu,
		     unsigned long vcpu_hva);
int __pkvm_teardown_vm(pkvm_handle_t handle);
struct pkvm_hyp_vcpu *pkvm_load_hyp_vcpu(pkvm_handle_t handle,
					 unsigned int vcpu_idx);
void pkvm_put_hyp_vcpu(struct pkvm_hyp_vcpu *hyp_vcpu);
#endif  
