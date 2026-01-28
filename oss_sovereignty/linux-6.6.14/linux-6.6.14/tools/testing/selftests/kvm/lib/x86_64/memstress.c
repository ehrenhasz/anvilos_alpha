#include <stdio.h>
#include <stdlib.h>
#include <linux/bitmap.h>
#include <linux/bitops.h>
#include "test_util.h"
#include "kvm_util.h"
#include "memstress.h"
#include "processor.h"
#include "vmx.h"
void memstress_l2_guest_code(uint64_t vcpu_id)
{
	memstress_guest_code(vcpu_id);
	vmcall();
}
extern char memstress_l2_guest_entry[];
__asm__(
"memstress_l2_guest_entry:"
"	mov (%rsp), %rdi;"
"	call memstress_l2_guest_code;"
"	ud2;"
);
static void memstress_l1_guest_code(struct vmx_pages *vmx, uint64_t vcpu_id)
{
#define L2_GUEST_STACK_SIZE 64
	unsigned long l2_guest_stack[L2_GUEST_STACK_SIZE];
	unsigned long *rsp;
	GUEST_ASSERT(vmx->vmcs_gpa);
	GUEST_ASSERT(prepare_for_vmx_operation(vmx));
	GUEST_ASSERT(load_vmcs(vmx));
	GUEST_ASSERT(ept_1g_pages_supported());
	rsp = &l2_guest_stack[L2_GUEST_STACK_SIZE - 1];
	*rsp = vcpu_id;
	prepare_vmcs(vmx, memstress_l2_guest_entry, rsp);
	GUEST_ASSERT(!vmlaunch());
	GUEST_ASSERT(vmreadz(VM_EXIT_REASON) == EXIT_REASON_VMCALL);
	GUEST_DONE();
}
uint64_t memstress_nested_pages(int nr_vcpus)
{
	return 513 + 10 * nr_vcpus;
}
void memstress_setup_ept(struct vmx_pages *vmx, struct kvm_vm *vm)
{
	uint64_t start, end;
	prepare_eptp(vmx, vm, 0);
	nested_identity_map_1g(vmx, vm, 0, 0x100000000ULL);
	start = align_down(memstress_args.gpa, PG_SIZE_1G);
	end = align_up(memstress_args.gpa + memstress_args.size, PG_SIZE_1G);
	nested_identity_map_1g(vmx, vm, start, end - start);
}
void memstress_setup_nested(struct kvm_vm *vm, int nr_vcpus, struct kvm_vcpu *vcpus[])
{
	struct vmx_pages *vmx, *vmx0 = NULL;
	struct kvm_regs regs;
	vm_vaddr_t vmx_gva;
	int vcpu_id;
	TEST_REQUIRE(kvm_cpu_has(X86_FEATURE_VMX));
	TEST_REQUIRE(kvm_cpu_has_ept());
	for (vcpu_id = 0; vcpu_id < nr_vcpus; vcpu_id++) {
		vmx = vcpu_alloc_vmx(vm, &vmx_gva);
		if (vcpu_id == 0) {
			memstress_setup_ept(vmx, vm);
			vmx0 = vmx;
		} else {
			vmx->eptp = vmx0->eptp;
			vmx->eptp_hva = vmx0->eptp_hva;
			vmx->eptp_gpa = vmx0->eptp_gpa;
		}
		vcpu_regs_get(vcpus[vcpu_id], &regs);
		regs.rip = (unsigned long) memstress_l1_guest_code;
		vcpu_regs_set(vcpus[vcpu_id], &regs);
		vcpu_args_set(vcpus[vcpu_id], 2, vmx_gva, vcpu_id);
	}
}
