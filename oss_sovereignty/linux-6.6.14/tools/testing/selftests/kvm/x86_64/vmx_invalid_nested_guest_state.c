
#include "test_util.h"
#include "kvm_util.h"
#include "processor.h"
#include "vmx.h"

#include <string.h>
#include <sys/ioctl.h>

#include "kselftest.h"

#define ARBITRARY_IO_PORT 0x2000

static struct kvm_vm *vm;

static void l2_guest_code(void)
{
	 
	asm volatile("inb %%dx, %%al"
		     : : [port] "d" (ARBITRARY_IO_PORT) : "rax");
}

static void l1_guest_code(struct vmx_pages *vmx_pages)
{
#define L2_GUEST_STACK_SIZE 64
	unsigned long l2_guest_stack[L2_GUEST_STACK_SIZE];

	GUEST_ASSERT(prepare_for_vmx_operation(vmx_pages));
	GUEST_ASSERT(load_vmcs(vmx_pages));

	 
	prepare_vmcs(vmx_pages, l2_guest_code,
		     &l2_guest_stack[L2_GUEST_STACK_SIZE]);

	 
	GUEST_ASSERT(!(vmreadz(CPU_BASED_VM_EXEC_CONTROL) & CPU_BASED_ACTIVATE_SECONDARY_CONTROLS) ||
		     !(vmreadz(SECONDARY_VM_EXEC_CONTROL) & SECONDARY_EXEC_UNRESTRICTED_GUEST));

	GUEST_ASSERT(!vmlaunch());

	 
	GUEST_ASSERT(vmreadz(VM_EXIT_REASON) == EXIT_REASON_TRIPLE_FAULT);
	GUEST_DONE();
}

int main(int argc, char *argv[])
{
	vm_vaddr_t vmx_pages_gva;
	struct kvm_sregs sregs;
	struct kvm_vcpu *vcpu;
	struct kvm_run *run;
	struct ucall uc;

	TEST_REQUIRE(kvm_cpu_has(X86_FEATURE_VMX));

	vm = vm_create_with_one_vcpu(&vcpu, l1_guest_code);

	 
	vcpu_alloc_vmx(vm, &vmx_pages_gva);
	vcpu_args_set(vcpu, 1, vmx_pages_gva);

	vcpu_run(vcpu);

	run = vcpu->run;

	 
	TEST_ASSERT_KVM_EXIT_REASON(vcpu, KVM_EXIT_IO);

	TEST_ASSERT(run->io.port == ARBITRARY_IO_PORT,
		    "Expected IN from port %d from L2, got port %d",
		    ARBITRARY_IO_PORT, run->io.port);

	 
	memset(&sregs, 0, sizeof(sregs));
	vcpu_sregs_get(vcpu, &sregs);
	sregs.tr.unusable = 1;
	vcpu_sregs_set(vcpu, &sregs);

	vcpu_run(vcpu);

	switch (get_ucall(vcpu, &uc)) {
	case UCALL_DONE:
		break;
	case UCALL_ABORT:
		REPORT_GUEST_ASSERT(uc);
	default:
		TEST_FAIL("Unexpected ucall: %lu", uc.cmd);
	}
}
