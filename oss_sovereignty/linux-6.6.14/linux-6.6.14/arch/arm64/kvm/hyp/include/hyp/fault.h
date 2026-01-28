#ifndef __ARM64_KVM_HYP_FAULT_H__
#define __ARM64_KVM_HYP_FAULT_H__
#include <asm/kvm_asm.h>
#include <asm/kvm_emulate.h>
#include <asm/kvm_hyp.h>
#include <asm/kvm_mmu.h>
static inline bool __translate_far_to_hpfar(u64 far, u64 *hpfar)
{
	u64 par, tmp;
	par = read_sysreg_par();
	if (!__kvm_at("s1e1r", far))
		tmp = read_sysreg_par();
	else
		tmp = SYS_PAR_EL1_F;  
	write_sysreg(par, par_el1);
	if (unlikely(tmp & SYS_PAR_EL1_F))
		return false;  
	*hpfar = PAR_TO_HPFAR(tmp);
	return true;
}
static inline bool __get_fault_info(u64 esr, struct kvm_vcpu_fault_info *fault)
{
	u64 hpfar, far;
	far = read_sysreg_el2(SYS_FAR);
	if (!(esr & ESR_ELx_S1PTW) &&
	    (cpus_have_final_cap(ARM64_WORKAROUND_834220) ||
	     (esr & ESR_ELx_FSC_TYPE) == ESR_ELx_FSC_PERM)) {
		if (!__translate_far_to_hpfar(far, &hpfar))
			return false;
	} else {
		hpfar = read_sysreg(hpfar_el2);
	}
	fault->far_el2 = far;
	fault->hpfar_el2 = hpfar;
	return true;
}
#endif
