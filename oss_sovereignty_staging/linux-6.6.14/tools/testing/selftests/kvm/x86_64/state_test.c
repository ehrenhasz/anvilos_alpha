
 
#define _GNU_SOURCE  
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>

#include "test_util.h"

#include "kvm_util.h"
#include "processor.h"
#include "vmx.h"
#include "svm_util.h"

#define L2_GUEST_STACK_SIZE 256

void svm_l2_guest_code(void)
{
	GUEST_SYNC(4);
	 
	vmcall();
	GUEST_SYNC(6);
	 
	vmcall();
}

static void svm_l1_guest_code(struct svm_test_data *svm)
{
	unsigned long l2_guest_stack[L2_GUEST_STACK_SIZE];
	struct vmcb *vmcb = svm->vmcb;

	GUEST_ASSERT(svm->vmcb_gpa);
	 
	generic_svm_setup(svm, svm_l2_guest_code,
			  &l2_guest_stack[L2_GUEST_STACK_SIZE]);

	GUEST_SYNC(3);
	run_guest(vmcb, svm->vmcb_gpa);
	GUEST_ASSERT(vmcb->control.exit_code == SVM_EXIT_VMMCALL);
	GUEST_SYNC(5);
	vmcb->save.rip += 3;
	run_guest(vmcb, svm->vmcb_gpa);
	GUEST_ASSERT(vmcb->control.exit_code == SVM_EXIT_VMMCALL);
	GUEST_SYNC(7);
}

void vmx_l2_guest_code(void)
{
	GUEST_SYNC(6);

	 
	vmcall();

	 
	GUEST_ASSERT(vmreadz(GUEST_RIP) == 0xc0ffee);
	GUEST_SYNC(10);
	GUEST_ASSERT(vmreadz(GUEST_RIP) == 0xc0ffee);
	GUEST_ASSERT(!vmwrite(GUEST_RIP, 0xc0fffee));
	GUEST_SYNC(11);
	GUEST_ASSERT(vmreadz(GUEST_RIP) == 0xc0fffee);
	GUEST_ASSERT(!vmwrite(GUEST_RIP, 0xc0ffffee));
	GUEST_SYNC(12);

	 
	vmcall();
}

static void vmx_l1_guest_code(struct vmx_pages *vmx_pages)
{
	unsigned long l2_guest_stack[L2_GUEST_STACK_SIZE];

	GUEST_ASSERT(vmx_pages->vmcs_gpa);
	GUEST_ASSERT(prepare_for_vmx_operation(vmx_pages));
	GUEST_SYNC(3);
	GUEST_ASSERT(load_vmcs(vmx_pages));
	GUEST_ASSERT(vmptrstz() == vmx_pages->vmcs_gpa);

	GUEST_SYNC(4);
	GUEST_ASSERT(vmptrstz() == vmx_pages->vmcs_gpa);

	prepare_vmcs(vmx_pages, vmx_l2_guest_code,
		     &l2_guest_stack[L2_GUEST_STACK_SIZE]);

	GUEST_SYNC(5);
	GUEST_ASSERT(vmptrstz() == vmx_pages->vmcs_gpa);
	GUEST_ASSERT(!vmlaunch());
	GUEST_ASSERT(vmptrstz() == vmx_pages->vmcs_gpa);
	GUEST_ASSERT(vmreadz(VM_EXIT_REASON) == EXIT_REASON_VMCALL);

	 
	GUEST_ASSERT(vmlaunch());

	GUEST_ASSERT(!vmresume());
	GUEST_ASSERT(vmreadz(VM_EXIT_REASON) == EXIT_REASON_VMCALL);

	GUEST_SYNC(7);
	GUEST_ASSERT(vmreadz(VM_EXIT_REASON) == EXIT_REASON_VMCALL);

	GUEST_ASSERT(!vmresume());
	GUEST_ASSERT(vmreadz(VM_EXIT_REASON) == EXIT_REASON_VMCALL);

	vmwrite(GUEST_RIP, vmreadz(GUEST_RIP) + 3);

	vmwrite(SECONDARY_VM_EXEC_CONTROL, SECONDARY_EXEC_SHADOW_VMCS);
	vmwrite(VMCS_LINK_POINTER, vmx_pages->shadow_vmcs_gpa);

	GUEST_ASSERT(!vmptrld(vmx_pages->shadow_vmcs_gpa));
	GUEST_ASSERT(vmlaunch());
	GUEST_SYNC(8);
	GUEST_ASSERT(vmlaunch());
	GUEST_ASSERT(vmresume());

	vmwrite(GUEST_RIP, 0xc0ffee);
	GUEST_SYNC(9);
	GUEST_ASSERT(vmreadz(GUEST_RIP) == 0xc0ffee);

	GUEST_ASSERT(!vmptrld(vmx_pages->vmcs_gpa));
	GUEST_ASSERT(!vmresume());
	GUEST_ASSERT(vmreadz(VM_EXIT_REASON) == EXIT_REASON_VMCALL);

	GUEST_ASSERT(!vmptrld(vmx_pages->shadow_vmcs_gpa));
	GUEST_ASSERT(vmreadz(GUEST_RIP) == 0xc0ffffee);
	GUEST_ASSERT(vmlaunch());
	GUEST_ASSERT(vmresume());
	GUEST_SYNC(13);
	GUEST_ASSERT(vmreadz(GUEST_RIP) == 0xc0ffffee);
	GUEST_ASSERT(vmlaunch());
	GUEST_ASSERT(vmresume());
}

static void __attribute__((__flatten__)) guest_code(void *arg)
{
	GUEST_SYNC(1);

	if (this_cpu_has(X86_FEATURE_XSAVE)) {
		uint64_t supported_xcr0 = this_cpu_supported_xcr0();
		uint8_t buffer[4096];

		memset(buffer, 0xcc, sizeof(buffer));

		set_cr4(get_cr4() | X86_CR4_OSXSAVE);
		GUEST_ASSERT(this_cpu_has(X86_FEATURE_OSXSAVE));

		xsetbv(0, xgetbv(0) | supported_xcr0);

		 
		GUEST_ASSERT(supported_xcr0 & XFEATURE_MASK_FP);
		asm volatile ("fincstp");

		GUEST_ASSERT(supported_xcr0 & XFEATURE_MASK_SSE);
		asm volatile ("vmovdqu %0, %%xmm0" :: "m" (buffer));

		if (supported_xcr0 & XFEATURE_MASK_YMM)
			asm volatile ("vmovdqu %0, %%ymm0" :: "m" (buffer));

		if (supported_xcr0 & XFEATURE_MASK_AVX512) {
			asm volatile ("kmovq %0, %%k1" :: "r" (-1ull));
			asm volatile ("vmovupd %0, %%zmm0" :: "m" (buffer));
			asm volatile ("vmovupd %0, %%zmm16" :: "m" (buffer));
		}

		if (this_cpu_has(X86_FEATURE_MPX)) {
			uint64_t bounds[2] = { 10, 0xffffffffull };
			uint64_t output[2] = { };

			GUEST_ASSERT(supported_xcr0 & XFEATURE_MASK_BNDREGS);
			GUEST_ASSERT(supported_xcr0 & XFEATURE_MASK_BNDCSR);

			 
			wrmsr(MSR_IA32_BNDCFGS, BIT_ULL(0));
			asm volatile (".byte 0x66,0x0f,0x1a,0x08" :: "a" (bounds));
			 
			asm volatile (".byte 0x66,0x0f,0x1b,0x08" :: "a" (output));
			wrmsr(MSR_IA32_BNDCFGS, 0);

			GUEST_ASSERT_EQ(bounds[0], output[0]);
			GUEST_ASSERT_EQ(bounds[1], output[1]);
		}
		if (this_cpu_has(X86_FEATURE_PKU)) {
			GUEST_ASSERT(supported_xcr0 & XFEATURE_MASK_PKRU);
			set_cr4(get_cr4() | X86_CR4_PKE);
			GUEST_ASSERT(this_cpu_has(X86_FEATURE_OSPKE));

			wrpkru(-1u);
		}
	}

	GUEST_SYNC(2);

	if (arg) {
		if (this_cpu_has(X86_FEATURE_SVM))
			svm_l1_guest_code(arg);
		else
			vmx_l1_guest_code(arg);
	}

	GUEST_DONE();
}

int main(int argc, char *argv[])
{
	uint64_t *xstate_bv, saved_xstate_bv;
	vm_vaddr_t nested_gva = 0;
	struct kvm_cpuid2 empty_cpuid = {};
	struct kvm_regs regs1, regs2;
	struct kvm_vcpu *vcpu, *vcpuN;
	struct kvm_vm *vm;
	struct kvm_x86_state *state;
	struct ucall uc;
	int stage;

	 
	vm = vm_create_with_one_vcpu(&vcpu, guest_code);

	vcpu_regs_get(vcpu, &regs1);

	if (kvm_has_cap(KVM_CAP_NESTED_STATE)) {
		if (kvm_cpu_has(X86_FEATURE_SVM))
			vcpu_alloc_svm(vm, &nested_gva);
		else if (kvm_cpu_has(X86_FEATURE_VMX))
			vcpu_alloc_vmx(vm, &nested_gva);
	}

	if (!nested_gva)
		pr_info("will skip nested state checks\n");

	vcpu_args_set(vcpu, 1, nested_gva);

	for (stage = 1;; stage++) {
		vcpu_run(vcpu);
		TEST_ASSERT_KVM_EXIT_REASON(vcpu, KVM_EXIT_IO);

		switch (get_ucall(vcpu, &uc)) {
		case UCALL_ABORT:
			REPORT_GUEST_ASSERT(uc);
			 
		case UCALL_SYNC:
			break;
		case UCALL_DONE:
			goto done;
		default:
			TEST_FAIL("Unknown ucall %lu", uc.cmd);
		}

		 
		TEST_ASSERT(!strcmp((const char *)uc.args[0], "hello") &&
			    uc.args[1] == stage, "Stage %d: Unexpected register values vmexit, got %lx",
			    stage, (ulong)uc.args[1]);

		state = vcpu_save_state(vcpu);
		memset(&regs1, 0, sizeof(regs1));
		vcpu_regs_get(vcpu, &regs1);

		kvm_vm_release(vm);

		 
		vcpu = vm_recreate_with_one_vcpu(vm);
		vcpu_load_state(vcpu, state);

		 
		xstate_bv = (void *)&((uint8_t *)state->xsave->region)[512];
		saved_xstate_bv = *xstate_bv;

		vcpuN = __vm_vcpu_add(vm, vcpu->id + 1);
		vcpu_xsave_set(vcpuN, state->xsave);
		*xstate_bv = kvm_cpu_supported_xcr0();
		vcpu_xsave_set(vcpuN, state->xsave);

		vcpu_init_cpuid(vcpuN, &empty_cpuid);
		vcpu_xsave_set(vcpuN, state->xsave);
		*xstate_bv = saved_xstate_bv;
		vcpu_xsave_set(vcpuN, state->xsave);

		kvm_x86_state_cleanup(state);

		memset(&regs2, 0, sizeof(regs2));
		vcpu_regs_get(vcpu, &regs2);
		TEST_ASSERT(!memcmp(&regs1, &regs2, sizeof(regs2)),
			    "Unexpected register values after vcpu_load_state; rdi: %lx rsi: %lx",
			    (ulong) regs2.rdi, (ulong) regs2.rsi);
	}

done:
	kvm_vm_free(vm);
}
