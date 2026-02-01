
 
#define _GNU_SOURCE  
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <linux/bitmap.h>

#include "test_util.h"

#include "kvm_util.h"

#include "hyperv.h"
#include "vmx.h"

static int ud_count;

static void guest_ud_handler(struct ex_regs *regs)
{
	ud_count++;
	regs->rip += 3;  
}

static void guest_nmi_handler(struct ex_regs *regs)
{
}

static inline void rdmsr_from_l2(uint32_t msr)
{
	 
	__asm__ __volatile__ ("rdmsr" : : "c"(msr) :
			      "rax", "rbx", "rdx", "rsi", "rdi", "r8", "r9",
			      "r10", "r11", "r12", "r13", "r14", "r15");
}

 
void l2_guest_code(void)
{
	u64 unused;

	GUEST_SYNC(7);

	GUEST_SYNC(8);

	 
	GUEST_SYNC(9);

	vmcall();

	 
	rdmsr_from_l2(MSR_FS_BASE);  
	rdmsr_from_l2(MSR_FS_BASE);  
	rdmsr_from_l2(MSR_GS_BASE);  
	vmcall();
	rdmsr_from_l2(MSR_GS_BASE);  

	 
	hyperv_hypercall(HVCALL_FLUSH_VIRTUAL_ADDRESS_SPACE | HV_HYPERCALL_FAST_BIT, 0x0,
			 HV_FLUSH_ALL_VIRTUAL_ADDRESS_SPACES | HV_FLUSH_ALL_PROCESSORS);
	rdmsr_from_l2(MSR_FS_BASE);
	 
	__hyperv_hypercall(HVCALL_FLUSH_VIRTUAL_ADDRESS_SPACE | HV_HYPERCALL_FAST_BIT, 0x0,
			   HV_FLUSH_ALL_VIRTUAL_ADDRESS_SPACES | HV_FLUSH_ALL_PROCESSORS,
			   &unused);

	 
	vmcall();
}

void guest_code(struct vmx_pages *vmx_pages, struct hyperv_test_pages *hv_pages,
		vm_vaddr_t hv_hcall_page_gpa)
{
#define L2_GUEST_STACK_SIZE 64
	unsigned long l2_guest_stack[L2_GUEST_STACK_SIZE];

	wrmsr(HV_X64_MSR_GUEST_OS_ID, HYPERV_LINUX_OS_ID);
	wrmsr(HV_X64_MSR_HYPERCALL, hv_hcall_page_gpa);

	x2apic_enable();

	GUEST_SYNC(1);
	GUEST_SYNC(2);

	enable_vp_assist(hv_pages->vp_assist_gpa, hv_pages->vp_assist);
	evmcs_enable();

	GUEST_ASSERT(prepare_for_vmx_operation(vmx_pages));
	GUEST_SYNC(3);
	GUEST_ASSERT(load_evmcs(hv_pages));
	GUEST_ASSERT(vmptrstz() == hv_pages->enlightened_vmcs_gpa);

	GUEST_SYNC(4);
	GUEST_ASSERT(vmptrstz() == hv_pages->enlightened_vmcs_gpa);

	prepare_vmcs(vmx_pages, l2_guest_code,
		     &l2_guest_stack[L2_GUEST_STACK_SIZE]);

	GUEST_SYNC(5);
	GUEST_ASSERT(vmptrstz() == hv_pages->enlightened_vmcs_gpa);
	current_evmcs->revision_id = -1u;
	GUEST_ASSERT(vmlaunch());
	current_evmcs->revision_id = EVMCS_VERSION;
	GUEST_SYNC(6);

	vmwrite(PIN_BASED_VM_EXEC_CONTROL, vmreadz(PIN_BASED_VM_EXEC_CONTROL) |
		PIN_BASED_NMI_EXITING);

	 
	current_evmcs->partition_assist_page = hv_pages->partition_assist_gpa;
	current_evmcs->hv_enlightenments_control.nested_flush_hypercall = 1;
	current_evmcs->hv_vm_id = 1;
	current_evmcs->hv_vp_id = 1;
	current_vp_assist->nested_control.features.directhypercall = 1;
	*(u32 *)(hv_pages->partition_assist) = 0;

	GUEST_ASSERT(!vmlaunch());
	GUEST_ASSERT_EQ(vmreadz(VM_EXIT_REASON), EXIT_REASON_EXCEPTION_NMI);
	GUEST_ASSERT_EQ((vmreadz(VM_EXIT_INTR_INFO) & 0xff), NMI_VECTOR);
	GUEST_ASSERT(vmptrstz() == hv_pages->enlightened_vmcs_gpa);

	 
	GUEST_ASSERT(!vmresume());

	GUEST_SYNC(10);

	GUEST_ASSERT(vmreadz(VM_EXIT_REASON) == EXIT_REASON_VMCALL);
	current_evmcs->guest_rip += 3;  

	 
	vmwrite(CPU_BASED_VM_EXEC_CONTROL, vmreadz(CPU_BASED_VM_EXEC_CONTROL) |
		CPU_BASED_USE_MSR_BITMAPS);
	__set_bit(MSR_FS_BASE & 0x1fff, vmx_pages->msr + 0x400);
	GUEST_ASSERT(!vmresume());
	GUEST_ASSERT(vmreadz(VM_EXIT_REASON) == EXIT_REASON_MSR_READ);
	current_evmcs->guest_rip += 2;  

	 
	current_evmcs->hv_enlightenments_control.msr_bitmap = 1;
	GUEST_ASSERT(!vmresume());
	GUEST_ASSERT(vmreadz(VM_EXIT_REASON) == EXIT_REASON_MSR_READ);
	current_evmcs->guest_rip += 2;  

	 
	__set_bit(MSR_GS_BASE & 0x1fff, vmx_pages->msr + 0x400);
	 
	current_evmcs->hv_clean_fields |= HV_VMX_ENLIGHTENED_CLEAN_FIELD_MSR_BITMAP;
	GUEST_ASSERT(!vmresume());
	 
	GUEST_ASSERT(vmreadz(VM_EXIT_REASON) == EXIT_REASON_VMCALL);
	current_evmcs->guest_rip += 3;  

	 
	current_evmcs->hv_clean_fields &= ~HV_VMX_ENLIGHTENED_CLEAN_FIELD_MSR_BITMAP;
	GUEST_ASSERT(!vmresume());
	GUEST_ASSERT(vmreadz(VM_EXIT_REASON) == EXIT_REASON_MSR_READ);
	current_evmcs->guest_rip += 2;  

	 
	GUEST_ASSERT(!vmresume());
	GUEST_ASSERT(vmreadz(VM_EXIT_REASON) == EXIT_REASON_MSR_READ);
	current_evmcs->guest_rip += 2;  
	 
	*(u32 *)(hv_pages->partition_assist) = 1;
	GUEST_ASSERT(!vmresume());
	GUEST_ASSERT(vmreadz(VM_EXIT_REASON) == HV_VMX_SYNTHETIC_EXIT_REASON_TRAP_AFTER_FLUSH);

	GUEST_ASSERT(!vmresume());
	GUEST_ASSERT(vmreadz(VM_EXIT_REASON) == EXIT_REASON_VMCALL);
	GUEST_SYNC(11);

	 
	evmcs_vmptrld(0xdeadbeef, hv_pages->enlightened_vmcs);
	GUEST_ASSERT(vmlaunch());
	GUEST_ASSERT(ud_count == 1);
	GUEST_DONE();
}

void inject_nmi(struct kvm_vcpu *vcpu)
{
	struct kvm_vcpu_events events;

	vcpu_events_get(vcpu, &events);

	events.nmi.pending = 1;
	events.flags |= KVM_VCPUEVENT_VALID_NMI_PENDING;

	vcpu_events_set(vcpu, &events);
}

static struct kvm_vcpu *save_restore_vm(struct kvm_vm *vm,
					struct kvm_vcpu *vcpu)
{
	struct kvm_regs regs1, regs2;
	struct kvm_x86_state *state;

	state = vcpu_save_state(vcpu);
	memset(&regs1, 0, sizeof(regs1));
	vcpu_regs_get(vcpu, &regs1);

	kvm_vm_release(vm);

	 
	vcpu = vm_recreate_with_one_vcpu(vm);
	vcpu_set_hv_cpuid(vcpu);
	vcpu_enable_evmcs(vcpu);
	vcpu_load_state(vcpu, state);
	kvm_x86_state_cleanup(state);

	memset(&regs2, 0, sizeof(regs2));
	vcpu_regs_get(vcpu, &regs2);
	TEST_ASSERT(!memcmp(&regs1, &regs2, sizeof(regs2)),
		    "Unexpected register values after vcpu_load_state; rdi: %lx rsi: %lx",
		    (ulong) regs2.rdi, (ulong) regs2.rsi);
	return vcpu;
}

int main(int argc, char *argv[])
{
	vm_vaddr_t vmx_pages_gva = 0, hv_pages_gva = 0;
	vm_vaddr_t hcall_page;

	struct kvm_vcpu *vcpu;
	struct kvm_vm *vm;
	struct ucall uc;
	int stage;

	vm = vm_create_with_one_vcpu(&vcpu, guest_code);

	TEST_REQUIRE(kvm_cpu_has(X86_FEATURE_VMX));
	TEST_REQUIRE(kvm_has_cap(KVM_CAP_NESTED_STATE));
	TEST_REQUIRE(kvm_has_cap(KVM_CAP_HYPERV_ENLIGHTENED_VMCS));

	hcall_page = vm_vaddr_alloc_pages(vm, 1);
	memset(addr_gva2hva(vm, hcall_page), 0x0,  getpagesize());

	vcpu_set_hv_cpuid(vcpu);
	vcpu_enable_evmcs(vcpu);

	vcpu_alloc_vmx(vm, &vmx_pages_gva);
	vcpu_alloc_hyperv_test_pages(vm, &hv_pages_gva);
	vcpu_args_set(vcpu, 3, vmx_pages_gva, hv_pages_gva, addr_gva2gpa(vm, hcall_page));
	vcpu_set_msr(vcpu, HV_X64_MSR_VP_INDEX, vcpu->id);

	vm_init_descriptor_tables(vm);
	vcpu_init_descriptor_tables(vcpu);
	vm_install_exception_handler(vm, UD_VECTOR, guest_ud_handler);
	vm_install_exception_handler(vm, NMI_VECTOR, guest_nmi_handler);

	pr_info("Running L1 which uses EVMCS to run L2\n");

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

		vcpu = save_restore_vm(vm, vcpu);

		 
		if (stage == 8) {
			pr_info("Injecting NMI into L1 before L2 had a chance to run after restore\n");
			inject_nmi(vcpu);
		}

		 
		if (stage == 9) {
			pr_info("Trying extra KVM_GET_NESTED_STATE/KVM_SET_NESTED_STATE cycle\n");
			vcpu = save_restore_vm(vm, vcpu);
		}
	}

done:
	kvm_vm_free(vm);
}
