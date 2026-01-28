#ifndef __ASM_STACKTRACE_NVHE_H
#define __ASM_STACKTRACE_NVHE_H
#include <asm/stacktrace/common.h>
static inline void kvm_nvhe_unwind_init(struct unwind_state *state,
					unsigned long fp,
					unsigned long pc)
{
	unwind_init_common(state, NULL);
	state->fp = fp;
	state->pc = pc;
}
#ifndef __KVM_NVHE_HYPERVISOR__
DECLARE_KVM_NVHE_PER_CPU(unsigned long [OVERFLOW_STACK_SIZE/sizeof(long)], overflow_stack);
DECLARE_KVM_NVHE_PER_CPU(struct kvm_nvhe_stacktrace_info, kvm_stacktrace_info);
DECLARE_PER_CPU(unsigned long, kvm_arm_hyp_stack_page);
void kvm_nvhe_dump_backtrace(unsigned long hyp_offset);
#endif	 
#endif	 
