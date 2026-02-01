
 

#define _GNU_SOURCE  
#include <sys/ioctl.h>

#include "test_util.h"
#include "kvm_util.h"
#include "vmx.h"

#define MSR_BITS      64

int main(int argc, char *argv[])
{
	bool xss_in_msr_list;
	struct kvm_vm *vm;
	struct kvm_vcpu *vcpu;
	uint64_t xss_val;
	int i, r;

	 
	vm = vm_create_with_one_vcpu(&vcpu, NULL);

	TEST_REQUIRE(kvm_cpu_has(X86_FEATURE_XSAVES));

	xss_val = vcpu_get_msr(vcpu, MSR_IA32_XSS);
	TEST_ASSERT(xss_val == 0,
		    "MSR_IA32_XSS should be initialized to zero\n");

	vcpu_set_msr(vcpu, MSR_IA32_XSS, xss_val);

	 
	xss_in_msr_list = kvm_msr_is_in_save_restore_list(MSR_IA32_XSS);
	for (i = 0; i < MSR_BITS; ++i) {
		r = _vcpu_set_msr(vcpu, MSR_IA32_XSS, 1ull << i);

		 
		TEST_ASSERT(!r || r == 1, KVM_IOCTL_ERROR(KVM_SET_MSRS, r));
		TEST_ASSERT(r != 1 || xss_in_msr_list,
			    "IA32_XSS was able to be set, but was not in save/restore list");
	}

	kvm_vm_free(vm);
}
