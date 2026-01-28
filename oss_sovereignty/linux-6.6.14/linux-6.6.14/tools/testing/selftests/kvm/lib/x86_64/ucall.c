#include "kvm_util.h"
#define UCALL_PIO_PORT ((uint16_t)0x1000)
void ucall_arch_do_ucall(vm_vaddr_t uc)
{
#define HORRIFIC_L2_UCALL_CLOBBER_HACK	\
	"rcx", "rsi", "r8", "r9", "r10", "r11"
	asm volatile("push %%rbp\n\t"
		     "push %%r15\n\t"
		     "push %%r14\n\t"
		     "push %%r13\n\t"
		     "push %%r12\n\t"
		     "push %%rbx\n\t"
		     "push %%rdx\n\t"
		     "push %%rdi\n\t"
		     "in %[port], %%al\n\t"
		     "pop %%rdi\n\t"
		     "pop %%rdx\n\t"
		     "pop %%rbx\n\t"
		     "pop %%r12\n\t"
		     "pop %%r13\n\t"
		     "pop %%r14\n\t"
		     "pop %%r15\n\t"
		     "pop %%rbp\n\t"
		: : [port] "d" (UCALL_PIO_PORT), "D" (uc) : "rax", "memory",
		     HORRIFIC_L2_UCALL_CLOBBER_HACK);
}
void *ucall_arch_get_ucall(struct kvm_vcpu *vcpu)
{
	struct kvm_run *run = vcpu->run;
	if (run->exit_reason == KVM_EXIT_IO && run->io.port == UCALL_PIO_PORT) {
		struct kvm_regs regs;
		vcpu_regs_get(vcpu, &regs);
		return (void *)regs.rdi;
	}
	return NULL;
}
