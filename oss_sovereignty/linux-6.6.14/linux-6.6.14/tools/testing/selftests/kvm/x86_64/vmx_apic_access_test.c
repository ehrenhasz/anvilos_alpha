#include "test_util.h"
#include "kvm_util.h"
#include "processor.h"
#include "vmx.h"
#include <string.h>
#include <sys/ioctl.h>
#include "kselftest.h"
static void l2_guest_code(void)
{
	__asm__ __volatile__("vmcall");
}
static void l1_guest_code(struct vmx_pages *vmx_pages, unsigned long high_gpa)
{
#define L2_GUEST_STACK_SIZE 64
	unsigned long l2_guest_stack[L2_GUEST_STACK_SIZE];
	uint32_t control;
	GUEST_ASSERT(prepare_for_vmx_operation(vmx_pages));
	GUEST_ASSERT(load_vmcs(vmx_pages));
	prepare_vmcs(vmx_pages, l2_guest_code,
		     &l2_guest_stack[L2_GUEST_STACK_SIZE]);
	control = vmreadz(CPU_BASED_VM_EXEC_CONTROL);
	control |= CPU_BASED_ACTIVATE_SECONDARY_CONTROLS;
	vmwrite(CPU_BASED_VM_EXEC_CONTROL, control);
	control = vmreadz(SECONDARY_VM_EXEC_CONTROL);
	control |= SECONDARY_EXEC_VIRTUALIZE_APIC_ACCESSES;
	vmwrite(SECONDARY_VM_EXEC_CONTROL, control);
	vmwrite(APIC_ACCESS_ADDR, vmx_pages->apic_access_gpa);
	GUEST_SYNC(vmreadz(APIC_ACCESS_ADDR));
	GUEST_ASSERT(!vmlaunch());
	GUEST_ASSERT(vmreadz(VM_EXIT_REASON) == EXIT_REASON_VMCALL);
	vmwrite(APIC_ACCESS_ADDR, high_gpa);
	GUEST_SYNC(vmreadz(APIC_ACCESS_ADDR));
	GUEST_ASSERT(!vmresume());
	GUEST_ASSERT(vmreadz(VM_EXIT_REASON) == EXIT_REASON_VMCALL);
	GUEST_DONE();
}
int main(int argc, char *argv[])
{
	unsigned long apic_access_addr = ~0ul;
	vm_vaddr_t vmx_pages_gva;
	unsigned long high_gpa;
	struct vmx_pages *vmx;
	bool done = false;
	struct kvm_vcpu *vcpu;
	struct kvm_vm *vm;
	TEST_REQUIRE(kvm_cpu_has(X86_FEATURE_VMX));
	vm = vm_create_with_one_vcpu(&vcpu, l1_guest_code);
	high_gpa = (vm->max_gfn - 1) << vm->page_shift;
	vmx = vcpu_alloc_vmx(vm, &vmx_pages_gva);
	prepare_virtualize_apic_accesses(vmx, vm);
	vcpu_args_set(vcpu, 2, vmx_pages_gva, high_gpa);
	while (!done) {
		volatile struct kvm_run *run = vcpu->run;
		struct ucall uc;
		vcpu_run(vcpu);
		if (apic_access_addr == high_gpa) {
			TEST_ASSERT_KVM_EXIT_REASON(vcpu, KVM_EXIT_INTERNAL_ERROR);
			TEST_ASSERT(run->internal.suberror ==
				    KVM_INTERNAL_ERROR_EMULATION,
				    "Got internal suberror other than KVM_INTERNAL_ERROR_EMULATION: %u\n",
				    run->internal.suberror);
			break;
		}
		TEST_ASSERT_KVM_EXIT_REASON(vcpu, KVM_EXIT_IO);
		switch (get_ucall(vcpu, &uc)) {
		case UCALL_ABORT:
			REPORT_GUEST_ASSERT(uc);
		case UCALL_SYNC:
			apic_access_addr = uc.args[1];
			break;
		case UCALL_DONE:
			done = true;
			break;
		default:
			TEST_ASSERT(false, "Unknown ucall %lu", uc.cmd);
		}
	}
	kvm_vm_free(vm);
	return 0;
}
