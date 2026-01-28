


#ifndef __ARCH_X86_KVM_KVM_ONHYPERV_H__
#define __ARCH_X86_KVM_KVM_ONHYPERV_H__

#if IS_ENABLED(CONFIG_HYPERV)
int hv_flush_remote_tlbs_range(struct kvm *kvm, gfn_t gfn, gfn_t nr_pages);
int hv_flush_remote_tlbs(struct kvm *kvm);
void hv_track_root_tdp(struct kvm_vcpu *vcpu, hpa_t root_tdp);
#else 
static inline int hv_flush_remote_tlbs(struct kvm *kvm)
{
	return -EOPNOTSUPP;
}

static inline void hv_track_root_tdp(struct kvm_vcpu *vcpu, hpa_t root_tdp)
{
}
#endif 

#endif
