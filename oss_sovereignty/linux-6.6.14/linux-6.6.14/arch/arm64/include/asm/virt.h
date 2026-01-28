#ifndef __ASM__VIRT_H
#define __ASM__VIRT_H
#define HVC_SET_VECTORS 0
#define HVC_SOFT_RESTART 1
#define HVC_RESET_VECTORS 2
#define HVC_FINALISE_EL2	3
#define HVC_STUB_HCALL_NR 4
#define HVC_STUB_ERR	0xbadca11
#define BOOT_CPU_MODE_EL1	(0xe11)
#define BOOT_CPU_MODE_EL2	(0xe12)
#define BOOT_CPU_FLAG_E2H	BIT_ULL(32)
#ifndef __ASSEMBLY__
#include <asm/ptrace.h>
#include <asm/sections.h>
#include <asm/sysreg.h>
#include <asm/cpufeature.h>
extern u32 __boot_cpu_mode[2];
#define ARM64_VECTOR_TABLE_LEN	SZ_2K
void __hyp_set_vectors(phys_addr_t phys_vector_base);
void __hyp_reset_vectors(void);
bool is_kvm_arm_initialised(void);
DECLARE_STATIC_KEY_FALSE(kvm_protected_mode_initialized);
static inline bool is_hyp_mode_available(void)
{
	if (IS_ENABLED(CONFIG_KVM) &&
	    static_branch_likely(&kvm_protected_mode_initialized))
		return true;
	return (__boot_cpu_mode[0] == BOOT_CPU_MODE_EL2 &&
		__boot_cpu_mode[1] == BOOT_CPU_MODE_EL2);
}
static inline bool is_hyp_mode_mismatched(void)
{
	if (IS_ENABLED(CONFIG_KVM) &&
	    static_branch_likely(&kvm_protected_mode_initialized))
		return false;
	return __boot_cpu_mode[0] != __boot_cpu_mode[1];
}
static __always_inline bool is_kernel_in_hyp_mode(void)
{
	BUILD_BUG_ON(__is_defined(__KVM_NVHE_HYPERVISOR__) ||
		     __is_defined(__KVM_VHE_HYPERVISOR__));
	return read_sysreg(CurrentEL) == CurrentEL_EL2;
}
static __always_inline bool has_vhe(void)
{
	if (is_vhe_hyp_code())
		return true;
	else if (is_nvhe_hyp_code())
		return false;
	else
		return cpus_have_final_cap(ARM64_HAS_VIRT_HOST_EXTN);
}
static __always_inline bool is_protected_kvm_enabled(void)
{
	if (is_vhe_hyp_code())
		return false;
	else
		return cpus_have_final_cap(ARM64_KVM_PROTECTED_MODE);
}
static __always_inline bool has_hvhe(void)
{
	if (is_vhe_hyp_code())
		return false;
	return cpus_have_final_cap(ARM64_KVM_HVHE);
}
static inline bool is_hyp_nvhe(void)
{
	return is_hyp_mode_available() && !is_kernel_in_hyp_mode();
}
#endif  
#endif  
