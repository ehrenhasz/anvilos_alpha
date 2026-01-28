#include <stdio.h>
#include <string.h>
#include "kvm_util.h"
#include "processor.h"
#include "apic.h"
#define DR6_BD		(1 << 13)
#define DR7_GD		(1 << 13)
#define IRQ_VECTOR 0xAA
uint32_t guest_value;
extern unsigned char sw_bp, hw_bp, write_data, ss_start, bd_start;
static void guest_code(void)
{
	x2apic_enable();
	x2apic_write_reg(APIC_ICR, APIC_DEST_SELF | APIC_INT_ASSERT |
			 APIC_DM_FIXED | IRQ_VECTOR);
	asm volatile("sw_bp: int3");
	asm volatile("hw_bp: nop");
	asm volatile("mov $1234,%%rax;\n\t"
		     "mov %%rax,%0;\n\t write_data:"
		     : "=m" (guest_value) : : "rax");
	asm volatile("ss_start: "
		     "sti\n\t"
		     "xor %%eax,%%eax\n\t"
		     "cpuid\n\t"
		     "movl $0x1a0,%%ecx\n\t"
		     "rdmsr\n\t"
		     "cli\n\t"
		     : : : "eax", "ebx", "ecx", "edx");
	asm volatile("bd_start: mov %%dr0, %%rax" : : : "rax");
	GUEST_DONE();
}
#define  CAST_TO_RIP(v)  ((unsigned long long)&(v))
static void vcpu_skip_insn(struct kvm_vcpu *vcpu, int insn_len)
{
	struct kvm_regs regs;
	vcpu_regs_get(vcpu, &regs);
	regs.rip += insn_len;
	vcpu_regs_set(vcpu, &regs);
}
int main(void)
{
	struct kvm_guest_debug debug;
	unsigned long long target_dr6, target_rip;
	struct kvm_vcpu *vcpu;
	struct kvm_run *run;
	struct kvm_vm *vm;
	struct ucall uc;
	uint64_t cmd;
	int i;
	int ss_size[6] = {
		1,		 
		2,		 
		2,		 
		5,		 
		2,		 
		1,		 
	};
	TEST_REQUIRE(kvm_has_cap(KVM_CAP_SET_GUEST_DEBUG));
	vm = vm_create_with_one_vcpu(&vcpu, guest_code);
	run = vcpu->run;
	memset(&debug, 0, sizeof(debug));
	debug.control = KVM_GUESTDBG_ENABLE | KVM_GUESTDBG_USE_SW_BP;
	vcpu_guest_debug_set(vcpu, &debug);
	vcpu_run(vcpu);
	TEST_ASSERT(run->exit_reason == KVM_EXIT_DEBUG &&
		    run->debug.arch.exception == BP_VECTOR &&
		    run->debug.arch.pc == CAST_TO_RIP(sw_bp),
		    "INT3: exit %d exception %d rip 0x%llx (should be 0x%llx)",
		    run->exit_reason, run->debug.arch.exception,
		    run->debug.arch.pc, CAST_TO_RIP(sw_bp));
	vcpu_skip_insn(vcpu, 1);
	for (i = 0; i < 4; i++) {
		memset(&debug, 0, sizeof(debug));
		debug.control = KVM_GUESTDBG_ENABLE | KVM_GUESTDBG_USE_HW_BP;
		debug.arch.debugreg[i] = CAST_TO_RIP(hw_bp);
		debug.arch.debugreg[7] = 0x400 | (1UL << (2*i+1));
		vcpu_guest_debug_set(vcpu, &debug);
		vcpu_run(vcpu);
		target_dr6 = 0xffff0ff0 | (1UL << i);
		TEST_ASSERT(run->exit_reason == KVM_EXIT_DEBUG &&
			    run->debug.arch.exception == DB_VECTOR &&
			    run->debug.arch.pc == CAST_TO_RIP(hw_bp) &&
			    run->debug.arch.dr6 == target_dr6,
			    "INS_HW_BP (DR%d): exit %d exception %d rip 0x%llx "
			    "(should be 0x%llx) dr6 0x%llx (should be 0x%llx)",
			    i, run->exit_reason, run->debug.arch.exception,
			    run->debug.arch.pc, CAST_TO_RIP(hw_bp),
			    run->debug.arch.dr6, target_dr6);
	}
	vcpu_skip_insn(vcpu, 1);
	for (i = 0; i < 4; i++) {
		memset(&debug, 0, sizeof(debug));
		debug.control = KVM_GUESTDBG_ENABLE | KVM_GUESTDBG_USE_HW_BP;
		debug.arch.debugreg[i] = CAST_TO_RIP(guest_value);
		debug.arch.debugreg[7] = 0x00000400 | (1UL << (2*i+1)) |
		    (0x000d0000UL << (4*i));
		vcpu_guest_debug_set(vcpu, &debug);
		vcpu_run(vcpu);
		target_dr6 = 0xffff0ff0 | (1UL << i);
		TEST_ASSERT(run->exit_reason == KVM_EXIT_DEBUG &&
			    run->debug.arch.exception == DB_VECTOR &&
			    run->debug.arch.pc == CAST_TO_RIP(write_data) &&
			    run->debug.arch.dr6 == target_dr6,
			    "DATA_HW_BP (DR%d): exit %d exception %d rip 0x%llx "
			    "(should be 0x%llx) dr6 0x%llx (should be 0x%llx)",
			    i, run->exit_reason, run->debug.arch.exception,
			    run->debug.arch.pc, CAST_TO_RIP(write_data),
			    run->debug.arch.dr6, target_dr6);
		vcpu_skip_insn(vcpu, -7);
	}
	vcpu_skip_insn(vcpu, 7);
	target_rip = CAST_TO_RIP(ss_start);
	target_dr6 = 0xffff4ff0ULL;
	for (i = 0; i < (sizeof(ss_size) / sizeof(ss_size[0])); i++) {
		target_rip += ss_size[i];
		memset(&debug, 0, sizeof(debug));
		debug.control = KVM_GUESTDBG_ENABLE | KVM_GUESTDBG_SINGLESTEP |
				KVM_GUESTDBG_BLOCKIRQ;
		debug.arch.debugreg[7] = 0x00000400;
		vcpu_guest_debug_set(vcpu, &debug);
		vcpu_run(vcpu);
		TEST_ASSERT(run->exit_reason == KVM_EXIT_DEBUG &&
			    run->debug.arch.exception == DB_VECTOR &&
			    run->debug.arch.pc == target_rip &&
			    run->debug.arch.dr6 == target_dr6,
			    "SINGLE_STEP[%d]: exit %d exception %d rip 0x%llx "
			    "(should be 0x%llx) dr6 0x%llx (should be 0x%llx)",
			    i, run->exit_reason, run->debug.arch.exception,
			    run->debug.arch.pc, target_rip, run->debug.arch.dr6,
			    target_dr6);
	}
	memset(&debug, 0, sizeof(debug));
	debug.control = KVM_GUESTDBG_ENABLE | KVM_GUESTDBG_USE_HW_BP;
	debug.arch.debugreg[7] = 0x400 | DR7_GD;
	vcpu_guest_debug_set(vcpu, &debug);
	vcpu_run(vcpu);
	target_dr6 = 0xffff0ff0 | DR6_BD;
	TEST_ASSERT(run->exit_reason == KVM_EXIT_DEBUG &&
		    run->debug.arch.exception == DB_VECTOR &&
		    run->debug.arch.pc == CAST_TO_RIP(bd_start) &&
		    run->debug.arch.dr6 == target_dr6,
			    "DR7.GD: exit %d exception %d rip 0x%llx "
			    "(should be 0x%llx) dr6 0x%llx (should be 0x%llx)",
			    run->exit_reason, run->debug.arch.exception,
			    run->debug.arch.pc, target_rip, run->debug.arch.dr6,
			    target_dr6);
	memset(&debug, 0, sizeof(debug));
	vcpu_guest_debug_set(vcpu, &debug);
	vcpu_run(vcpu);
	TEST_ASSERT_KVM_EXIT_REASON(vcpu, KVM_EXIT_IO);
	cmd = get_ucall(vcpu, &uc);
	TEST_ASSERT(cmd == UCALL_DONE, "UCALL_DONE");
	kvm_vm_free(vm);
	return 0;
}
