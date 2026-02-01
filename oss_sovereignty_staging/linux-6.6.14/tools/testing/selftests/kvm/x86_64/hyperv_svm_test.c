
 
#define _GNU_SOURCE  
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <linux/bitmap.h>

#include "test_util.h"

#include "kvm_util.h"
#include "processor.h"
#include "svm_util.h"
#include "hyperv.h"

#define L2_GUEST_STACK_SIZE 256

 
static inline void rdmsr_from_l2(uint32_t msr)
{
	 
	__asm__ __volatile__ ("rdmsr" : : "c"(msr) :
			      "rax", "rbx", "rdx", "rsi", "rdi", "r8", "r9",
			      "r10", "r11", "r12", "r13", "r14", "r15");
}

void l2_guest_code(void)
{
	u64 unused;

	GUEST_SYNC(3);
	 
	vmmcall();

	 
	rdmsr_from_l2(MSR_FS_BASE);  
	rdmsr_from_l2(MSR_FS_BASE);  
	rdmsr_from_l2(MSR_GS_BASE);  
	vmmcall();
	rdmsr_from_l2(MSR_GS_BASE);  

	GUEST_SYNC(5);

	 
	hyperv_hypercall(HVCALL_FLUSH_VIRTUAL_ADDRESS_SPACE |
			 HV_HYPERCALL_FAST_BIT, 0x0,
			 HV_FLUSH_ALL_VIRTUAL_ADDRESS_SPACES |
			 HV_FLUSH_ALL_PROCESSORS);
	rdmsr_from_l2(MSR_FS_BASE);
	 
	__hyperv_hypercall(HVCALL_FLUSH_VIRTUAL_ADDRESS_SPACE |
			   HV_HYPERCALL_FAST_BIT, 0x0,
			   HV_FLUSH_ALL_VIRTUAL_ADDRESS_SPACES |
			   HV_FLUSH_ALL_PROCESSORS, &unused);

	 
	vmmcall();
}

static void __attribute__((__flatten__)) guest_code(struct svm_test_data *svm,
						    struct hyperv_test_pages *hv_pages,
						    vm_vaddr_t pgs_gpa)
{
	unsigned long l2_guest_stack[L2_GUEST_STACK_SIZE];
	struct vmcb *vmcb = svm->vmcb;
	struct hv_vmcb_enlightenments *hve = &vmcb->control.hv_enlightenments;

	GUEST_SYNC(1);

	wrmsr(HV_X64_MSR_GUEST_OS_ID, HYPERV_LINUX_OS_ID);
	wrmsr(HV_X64_MSR_HYPERCALL, pgs_gpa);
	enable_vp_assist(hv_pages->vp_assist_gpa, hv_pages->vp_assist);

	GUEST_ASSERT(svm->vmcb_gpa);
	 
	generic_svm_setup(svm, l2_guest_code,
			  &l2_guest_stack[L2_GUEST_STACK_SIZE]);

	 
	hve->partition_assist_page = hv_pages->partition_assist_gpa;
	hve->hv_enlightenments_control.nested_flush_hypercall = 1;
	hve->hv_vm_id = 1;
	hve->hv_vp_id = 1;
	current_vp_assist->nested_control.features.directhypercall = 1;
	*(u32 *)(hv_pages->partition_assist) = 0;

	GUEST_SYNC(2);
	run_guest(vmcb, svm->vmcb_gpa);
	GUEST_ASSERT(vmcb->control.exit_code == SVM_EXIT_VMMCALL);
	GUEST_SYNC(4);
	vmcb->save.rip += 3;

	 
	vmcb->control.intercept |= 1ULL << INTERCEPT_MSR_PROT;
	__set_bit(2 * (MSR_FS_BASE & 0x1fff), svm->msr + 0x800);
	run_guest(vmcb, svm->vmcb_gpa);
	GUEST_ASSERT(vmcb->control.exit_code == SVM_EXIT_MSR);
	vmcb->save.rip += 2;  

	 
	hve->hv_enlightenments_control.msr_bitmap = 1;
	run_guest(vmcb, svm->vmcb_gpa);
	GUEST_ASSERT(vmcb->control.exit_code == SVM_EXIT_MSR);
	vmcb->save.rip += 2;  

	 
	__set_bit(2 * (MSR_GS_BASE & 0x1fff), svm->msr + 0x800);
	 
	vmcb->control.clean |= HV_VMCB_NESTED_ENLIGHTENMENTS;
	run_guest(vmcb, svm->vmcb_gpa);
	 
	GUEST_ASSERT(vmcb->control.exit_code == SVM_EXIT_VMMCALL);
	vmcb->save.rip += 3;  

	 
	vmcb->control.clean &= ~HV_VMCB_NESTED_ENLIGHTENMENTS;
	run_guest(vmcb, svm->vmcb_gpa);
	GUEST_ASSERT(vmcb->control.exit_code == SVM_EXIT_MSR);
	vmcb->save.rip += 2;  


	 
	run_guest(vmcb, svm->vmcb_gpa);
	GUEST_ASSERT(vmcb->control.exit_code == SVM_EXIT_MSR);
	vmcb->save.rip += 2;  
	 
	*(u32 *)(hv_pages->partition_assist) = 1;
	run_guest(vmcb, svm->vmcb_gpa);
	GUEST_ASSERT(vmcb->control.exit_code == HV_SVM_EXITCODE_ENL);
	GUEST_ASSERT(vmcb->control.exit_info_1 == HV_SVM_ENL_EXITCODE_TRAP_AFTER_FLUSH);

	run_guest(vmcb, svm->vmcb_gpa);
	GUEST_ASSERT(vmcb->control.exit_code == SVM_EXIT_VMMCALL);
	GUEST_SYNC(6);

	GUEST_DONE();
}

int main(int argc, char *argv[])
{
	vm_vaddr_t nested_gva = 0, hv_pages_gva = 0;
	vm_vaddr_t hcall_page;
	struct kvm_vcpu *vcpu;
	struct kvm_vm *vm;
	struct ucall uc;
	int stage;

	TEST_REQUIRE(kvm_cpu_has(X86_FEATURE_SVM));

	 
	vm = vm_create_with_one_vcpu(&vcpu, guest_code);
	vcpu_set_hv_cpuid(vcpu);
	vcpu_alloc_svm(vm, &nested_gva);
	vcpu_alloc_hyperv_test_pages(vm, &hv_pages_gva);

	hcall_page = vm_vaddr_alloc_pages(vm, 1);
	memset(addr_gva2hva(vm, hcall_page), 0x0,  getpagesize());

	vcpu_args_set(vcpu, 3, nested_gva, hv_pages_gva, addr_gva2gpa(vm, hcall_page));
	vcpu_set_msr(vcpu, HV_X64_MSR_VP_INDEX, vcpu->id);

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

	}

done:
	kvm_vm_free(vm);
}
