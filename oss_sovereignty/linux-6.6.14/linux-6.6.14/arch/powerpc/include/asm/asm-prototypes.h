#ifndef _ASM_POWERPC_ASM_PROTOTYPES_H
#define _ASM_POWERPC_ASM_PROTOTYPES_H
#include <linux/threads.h>
#include <asm/cacheflush.h>
#include <asm/checksum.h>
#include <linux/uaccess.h>
#include <asm/epapr_hcalls.h>
#include <asm/dcr.h>
#include <asm/mmu_context.h>
#include <asm/ultravisor-api.h>
#include <uapi/asm/ucontext.h>
#if defined(CONFIG_PPC_POWERNV) || defined(CONFIG_PPC_SVM)
long ucall_norets(unsigned long opcode, ...);
#else
static inline long ucall_norets(unsigned long opcode, ...)
{
	return U_NOT_AVAILABLE;
}
#endif
int64_t __opal_call(int64_t a0, int64_t a1, int64_t a2, int64_t a3,
		    int64_t a4, int64_t a5, int64_t a6, int64_t a7,
		    int64_t opcode, uint64_t msr);
void enable_machine_check(void);
extern u64 __bswapdi2(u64);
extern s64 __lshrdi3(s64, int);
extern s64 __ashldi3(s64, int);
extern s64 __ashrdi3(s64, int);
extern int __cmpdi2(s64, s64);
extern int __ucmpdi2(u64, u64);
void _mcount(void);
void tm_enable(void);
void tm_disable(void);
void tm_abort(uint8_t cause);
struct kvm_vcpu;
void _kvmppc_restore_tm_pr(struct kvm_vcpu *vcpu, u64 guest_msr);
void _kvmppc_save_tm_pr(struct kvm_vcpu *vcpu, u64 guest_msr);
#ifdef CONFIG_PPC_TRANSACTIONAL_MEM
void kvmppc_save_tm_hv(struct kvm_vcpu *vcpu, u64 msr, bool preserve_nv);
void kvmppc_restore_tm_hv(struct kvm_vcpu *vcpu, u64 msr, bool preserve_nv);
#else
static inline void kvmppc_save_tm_hv(struct kvm_vcpu *vcpu, u64 msr,
				     bool preserve_nv) { }
static inline void kvmppc_restore_tm_hv(struct kvm_vcpu *vcpu, u64 msr,
					bool preserve_nv) { }
#endif  
void kvmppc_p9_enter_guest(struct kvm_vcpu *vcpu);
long kvmppc_h_set_dabr(struct kvm_vcpu *vcpu, unsigned long dabr);
long kvmppc_h_set_xdabr(struct kvm_vcpu *vcpu, unsigned long dabr,
			unsigned long dabrx);
#endif  
