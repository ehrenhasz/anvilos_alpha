#define _GNU_SOURCE  
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <pthread.h>
#include "test_util.h"
#include "kvm_util.h"
#include "processor.h"
#define UCALL_PIO_PORT ((uint16_t)0x1000)
struct ucall uc_none = {
	.cmd = UCALL_NONE,
};
void guest_code(void)
{
	asm volatile("1: in %[port], %%al\n"
		     "add $0x1, %%rbx\n"
		     "jmp 1b"
		     : : [port] "d" (UCALL_PIO_PORT), "D" (&uc_none)
		     : "rax", "rbx");
}
static void compare_regs(struct kvm_regs *left, struct kvm_regs *right)
{
#define REG_COMPARE(reg) \
	TEST_ASSERT(left->reg == right->reg, \
		    "Register " #reg \
		    " values did not match: 0x%llx, 0x%llx\n", \
		    left->reg, right->reg)
	REG_COMPARE(rax);
	REG_COMPARE(rbx);
	REG_COMPARE(rcx);
	REG_COMPARE(rdx);
	REG_COMPARE(rsi);
	REG_COMPARE(rdi);
	REG_COMPARE(rsp);
	REG_COMPARE(rbp);
	REG_COMPARE(r8);
	REG_COMPARE(r9);
	REG_COMPARE(r10);
	REG_COMPARE(r11);
	REG_COMPARE(r12);
	REG_COMPARE(r13);
	REG_COMPARE(r14);
	REG_COMPARE(r15);
	REG_COMPARE(rip);
	REG_COMPARE(rflags);
#undef REG_COMPARE
}
static void compare_sregs(struct kvm_sregs *left, struct kvm_sregs *right)
{
}
static void compare_vcpu_events(struct kvm_vcpu_events *left,
				struct kvm_vcpu_events *right)
{
}
#define TEST_SYNC_FIELDS   (KVM_SYNC_X86_REGS|KVM_SYNC_X86_SREGS|KVM_SYNC_X86_EVENTS)
#define INVALID_SYNC_FIELD 0x80000000
static void *race_events_inj_pen(void *arg)
{
	struct kvm_run *run = (struct kvm_run *)arg;
	struct kvm_vcpu_events *events = &run->s.regs.events;
	WRITE_ONCE(events->exception.nr, UD_VECTOR);
	for (;;) {
		WRITE_ONCE(run->kvm_dirty_regs, KVM_SYNC_X86_EVENTS);
		WRITE_ONCE(events->flags, 0);
		WRITE_ONCE(events->exception.injected, 1);
		WRITE_ONCE(events->exception.pending, 1);
		pthread_testcancel();
	}
	return NULL;
}
static void *race_events_exc(void *arg)
{
	struct kvm_run *run = (struct kvm_run *)arg;
	struct kvm_vcpu_events *events = &run->s.regs.events;
	for (;;) {
		WRITE_ONCE(run->kvm_dirty_regs, KVM_SYNC_X86_EVENTS);
		WRITE_ONCE(events->flags, 0);
		WRITE_ONCE(events->exception.nr, UD_VECTOR);
		WRITE_ONCE(events->exception.pending, 1);
		WRITE_ONCE(events->exception.nr, 255);
		pthread_testcancel();
	}
	return NULL;
}
static noinline void *race_sregs_cr4(void *arg)
{
	struct kvm_run *run = (struct kvm_run *)arg;
	__u64 *cr4 = &run->s.regs.sregs.cr4;
	__u64 pae_enabled = *cr4;
	__u64 pae_disabled = *cr4 & ~X86_CR4_PAE;
	for (;;) {
		WRITE_ONCE(run->kvm_dirty_regs, KVM_SYNC_X86_SREGS);
		WRITE_ONCE(*cr4, pae_enabled);
		asm volatile(".rept 512\n\t"
			     "nop\n\t"
			     ".endr");
		WRITE_ONCE(*cr4, pae_disabled);
		pthread_testcancel();
	}
	return NULL;
}
static void race_sync_regs(void *racer)
{
	const time_t TIMEOUT = 2;  
	struct kvm_x86_state *state;
	struct kvm_translation tr;
	struct kvm_vcpu *vcpu;
	struct kvm_run *run;
	struct kvm_vm *vm;
	pthread_t thread;
	time_t t;
	vm = vm_create_with_one_vcpu(&vcpu, guest_code);
	run = vcpu->run;
	run->kvm_valid_regs = KVM_SYNC_X86_SREGS;
	vcpu_run(vcpu);
	run->kvm_valid_regs = 0;
	state = vcpu_save_state(vcpu);
	TEST_ASSERT((run->s.regs.sregs.cr4 & X86_CR4_PAE) &&
		    (run->s.regs.sregs.efer & EFER_LME),
		    "vCPU should be in long mode, CR4.PAE=%d, EFER.LME=%d",
		    !!(run->s.regs.sregs.cr4 & X86_CR4_PAE),
		    !!(run->s.regs.sregs.efer & EFER_LME));
	TEST_ASSERT_EQ(pthread_create(&thread, NULL, racer, (void *)run), 0);
	for (t = time(NULL) + TIMEOUT; time(NULL) < t;) {
		if (!__vcpu_run(vcpu) && run->exit_reason == KVM_EXIT_SHUTDOWN)
			vcpu_load_state(vcpu, state);
		if (racer == race_sregs_cr4) {
			tr = (struct kvm_translation) { .linear_address = 0 };
			__vcpu_ioctl(vcpu, KVM_TRANSLATE, &tr);
		}
	}
	TEST_ASSERT_EQ(pthread_cancel(thread), 0);
	TEST_ASSERT_EQ(pthread_join(thread, NULL), 0);
	kvm_x86_state_cleanup(state);
	kvm_vm_free(vm);
}
int main(int argc, char *argv[])
{
	struct kvm_vcpu *vcpu;
	struct kvm_vm *vm;
	struct kvm_run *run;
	struct kvm_regs regs;
	struct kvm_sregs sregs;
	struct kvm_vcpu_events events;
	int rv, cap;
	cap = kvm_check_cap(KVM_CAP_SYNC_REGS);
	TEST_REQUIRE((cap & TEST_SYNC_FIELDS) == TEST_SYNC_FIELDS);
	TEST_REQUIRE(!(cap & INVALID_SYNC_FIELD));
	vm = vm_create_with_one_vcpu(&vcpu, guest_code);
	run = vcpu->run;
	run->kvm_valid_regs = INVALID_SYNC_FIELD;
	rv = _vcpu_run(vcpu);
	TEST_ASSERT(rv < 0 && errno == EINVAL,
		    "Invalid kvm_valid_regs did not cause expected KVM_RUN error: %d\n",
		    rv);
	run->kvm_valid_regs = 0;
	run->kvm_valid_regs = INVALID_SYNC_FIELD | TEST_SYNC_FIELDS;
	rv = _vcpu_run(vcpu);
	TEST_ASSERT(rv < 0 && errno == EINVAL,
		    "Invalid kvm_valid_regs did not cause expected KVM_RUN error: %d\n",
		    rv);
	run->kvm_valid_regs = 0;
	run->kvm_dirty_regs = INVALID_SYNC_FIELD;
	rv = _vcpu_run(vcpu);
	TEST_ASSERT(rv < 0 && errno == EINVAL,
		    "Invalid kvm_dirty_regs did not cause expected KVM_RUN error: %d\n",
		    rv);
	run->kvm_dirty_regs = 0;
	run->kvm_dirty_regs = INVALID_SYNC_FIELD | TEST_SYNC_FIELDS;
	rv = _vcpu_run(vcpu);
	TEST_ASSERT(rv < 0 && errno == EINVAL,
		    "Invalid kvm_dirty_regs did not cause expected KVM_RUN error: %d\n",
		    rv);
	run->kvm_dirty_regs = 0;
	run->kvm_valid_regs = TEST_SYNC_FIELDS;
	rv = _vcpu_run(vcpu);
	TEST_ASSERT_KVM_EXIT_REASON(vcpu, KVM_EXIT_IO);
	vcpu_regs_get(vcpu, &regs);
	compare_regs(&regs, &run->s.regs.regs);
	vcpu_sregs_get(vcpu, &sregs);
	compare_sregs(&sregs, &run->s.regs.sregs);
	vcpu_events_get(vcpu, &events);
	compare_vcpu_events(&events, &run->s.regs.events);
	run->s.regs.regs.rbx = 0xBAD1DEA;
	run->s.regs.sregs.apic_base = 1 << 11;
	run->kvm_valid_regs = TEST_SYNC_FIELDS;
	run->kvm_dirty_regs = KVM_SYNC_X86_REGS | KVM_SYNC_X86_SREGS;
	rv = _vcpu_run(vcpu);
	TEST_ASSERT_KVM_EXIT_REASON(vcpu, KVM_EXIT_IO);
	TEST_ASSERT(run->s.regs.regs.rbx == 0xBAD1DEA + 1,
		    "rbx sync regs value incorrect 0x%llx.",
		    run->s.regs.regs.rbx);
	TEST_ASSERT(run->s.regs.sregs.apic_base == 1 << 11,
		    "apic_base sync regs value incorrect 0x%llx.",
		    run->s.regs.sregs.apic_base);
	vcpu_regs_get(vcpu, &regs);
	compare_regs(&regs, &run->s.regs.regs);
	vcpu_sregs_get(vcpu, &sregs);
	compare_sregs(&sregs, &run->s.regs.sregs);
	vcpu_events_get(vcpu, &events);
	compare_vcpu_events(&events, &run->s.regs.events);
	run->kvm_valid_regs = TEST_SYNC_FIELDS;
	run->kvm_dirty_regs = 0;
	run->s.regs.regs.rbx = 0xDEADBEEF;
	rv = _vcpu_run(vcpu);
	TEST_ASSERT_KVM_EXIT_REASON(vcpu, KVM_EXIT_IO);
	TEST_ASSERT(run->s.regs.regs.rbx != 0xDEADBEEF,
		    "rbx sync regs value incorrect 0x%llx.",
		    run->s.regs.regs.rbx);
	run->kvm_valid_regs = 0;
	run->kvm_dirty_regs = 0;
	run->s.regs.regs.rbx = 0xAAAA;
	regs.rbx = 0xBAC0;
	vcpu_regs_set(vcpu, &regs);
	rv = _vcpu_run(vcpu);
	TEST_ASSERT_KVM_EXIT_REASON(vcpu, KVM_EXIT_IO);
	TEST_ASSERT(run->s.regs.regs.rbx == 0xAAAA,
		    "rbx sync regs value incorrect 0x%llx.",
		    run->s.regs.regs.rbx);
	vcpu_regs_get(vcpu, &regs);
	TEST_ASSERT(regs.rbx == 0xBAC0 + 1,
		    "rbx guest value incorrect 0x%llx.",
		    regs.rbx);
	run->kvm_valid_regs = 0;
	run->kvm_dirty_regs = TEST_SYNC_FIELDS;
	run->s.regs.regs.rbx = 0xBBBB;
	rv = _vcpu_run(vcpu);
	TEST_ASSERT_KVM_EXIT_REASON(vcpu, KVM_EXIT_IO);
	TEST_ASSERT(run->s.regs.regs.rbx == 0xBBBB,
		    "rbx sync regs value incorrect 0x%llx.",
		    run->s.regs.regs.rbx);
	vcpu_regs_get(vcpu, &regs);
	TEST_ASSERT(regs.rbx == 0xBBBB + 1,
		    "rbx guest value incorrect 0x%llx.",
		    regs.rbx);
	kvm_vm_free(vm);
	race_sync_regs(race_sregs_cr4);
	race_sync_regs(race_events_exc);
	race_sync_regs(race_events_inj_pen);
	return 0;
}
