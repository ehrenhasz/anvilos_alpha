#ifndef __ARM64_KVM_HYP_ADJUST_PC_H__
#define __ARM64_KVM_HYP_ADJUST_PC_H__
#include <asm/kvm_emulate.h>
#include <asm/kvm_host.h>
static inline void kvm_skip_instr(struct kvm_vcpu *vcpu)
{
	if (vcpu_mode_is_32bit(vcpu)) {
		kvm_skip_instr32(vcpu);
	} else {
		*vcpu_pc(vcpu) += 4;
		*vcpu_cpsr(vcpu) &= ~PSR_BTYPE_MASK;
	}
	*vcpu_cpsr(vcpu) &= ~DBG_SPSR_SS;
}
static inline void __kvm_skip_instr(struct kvm_vcpu *vcpu)
{
	*vcpu_pc(vcpu) = read_sysreg_el2(SYS_ELR);
	vcpu_gp_regs(vcpu)->pstate = read_sysreg_el2(SYS_SPSR);
	kvm_skip_instr(vcpu);
	write_sysreg_el2(vcpu_gp_regs(vcpu)->pstate, SYS_SPSR);
	write_sysreg_el2(*vcpu_pc(vcpu), SYS_ELR);
}
static inline void kvm_skip_host_instr(void)
{
	write_sysreg_el2(read_sysreg_el2(SYS_ELR) + 4, SYS_ELR);
}
#endif
