
 

#include "test_util.h"
#include "kvm_util.h"
#include "processor.h"
#include "vmx.h"

#include <errno.h>
#include <linux/kvm.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

 
#define VMCS12_REVISION 0x11e57ed0

bool have_evmcs;

void test_nested_state(struct kvm_vcpu *vcpu, struct kvm_nested_state *state)
{
	vcpu_nested_state_set(vcpu, state);
}

void test_nested_state_expect_errno(struct kvm_vcpu *vcpu,
				    struct kvm_nested_state *state,
				    int expected_errno)
{
	int rv;

	rv = __vcpu_nested_state_set(vcpu, state);
	TEST_ASSERT(rv == -1 && errno == expected_errno,
		"Expected %s (%d) from vcpu_nested_state_set but got rv: %i errno: %s (%d)",
		strerror(expected_errno), expected_errno, rv, strerror(errno),
		errno);
}

void test_nested_state_expect_einval(struct kvm_vcpu *vcpu,
				     struct kvm_nested_state *state)
{
	test_nested_state_expect_errno(vcpu, state, EINVAL);
}

void test_nested_state_expect_efault(struct kvm_vcpu *vcpu,
				     struct kvm_nested_state *state)
{
	test_nested_state_expect_errno(vcpu, state, EFAULT);
}

void set_revision_id_for_vmcs12(struct kvm_nested_state *state,
				u32 vmcs12_revision)
{
	 
	memcpy(&state->data, &vmcs12_revision, sizeof(u32));
}

void set_default_state(struct kvm_nested_state *state)
{
	memset(state, 0, sizeof(*state));
	state->flags = KVM_STATE_NESTED_RUN_PENDING |
		       KVM_STATE_NESTED_GUEST_MODE;
	state->format = 0;
	state->size = sizeof(*state);
}

void set_default_vmx_state(struct kvm_nested_state *state, int size)
{
	memset(state, 0, size);
	if (have_evmcs)
		state->flags = KVM_STATE_NESTED_EVMCS;
	state->format = 0;
	state->size = size;
	state->hdr.vmx.vmxon_pa = 0x1000;
	state->hdr.vmx.vmcs12_pa = 0x2000;
	state->hdr.vmx.smm.flags = 0;
	set_revision_id_for_vmcs12(state, VMCS12_REVISION);
}

void test_vmx_nested_state(struct kvm_vcpu *vcpu)
{
	 
	const int state_sz = sizeof(struct kvm_nested_state) + getpagesize();
	struct kvm_nested_state *state =
		(struct kvm_nested_state *)malloc(state_sz);

	 
	set_default_vmx_state(state, state_sz);
	state->format = 1;
	test_nested_state_expect_einval(vcpu, state);

	 
	set_default_vmx_state(state, state_sz);
	test_nested_state_expect_einval(vcpu, state);

	 
	set_default_vmx_state(state, state_sz);
	state->hdr.vmx.vmxon_pa = -1ull;
	test_nested_state_expect_einval(vcpu, state);

	state->hdr.vmx.vmcs12_pa = -1ull;
	state->flags = KVM_STATE_NESTED_EVMCS;
	test_nested_state_expect_einval(vcpu, state);

	state->flags = 0;
	test_nested_state(vcpu, state);

	 
	vcpu_set_cpuid_feature(vcpu, X86_FEATURE_VMX);

	 
	set_default_vmx_state(state, state_sz);
	state->hdr.vmx.vmxon_pa = -1ull;
	state->hdr.vmx.vmcs12_pa = -1ull;
	test_nested_state_expect_einval(vcpu, state);

	state->flags &= KVM_STATE_NESTED_EVMCS;
	if (have_evmcs) {
		test_nested_state_expect_einval(vcpu, state);
		vcpu_enable_evmcs(vcpu);
	}
	test_nested_state(vcpu, state);

	 
	state->hdr.vmx.smm.flags = 1;
	test_nested_state_expect_einval(vcpu, state);

	 
	set_default_vmx_state(state, state_sz);
	state->hdr.vmx.flags = ~0;
	test_nested_state_expect_einval(vcpu, state);

	 
	set_default_vmx_state(state, state_sz);
	state->hdr.vmx.vmxon_pa = -1ull;
	state->flags = 0;
	test_nested_state_expect_einval(vcpu, state);

	 
	set_default_vmx_state(state, state_sz);
	state->hdr.vmx.vmxon_pa = 1;
	test_nested_state_expect_einval(vcpu, state);

	 
	set_default_vmx_state(state, state_sz);
	state->flags = KVM_STATE_NESTED_GUEST_MODE  |
		      KVM_STATE_NESTED_RUN_PENDING;
	state->hdr.vmx.smm.flags = KVM_STATE_NESTED_SMM_GUEST_MODE;
	test_nested_state_expect_einval(vcpu, state);

	 
	set_default_vmx_state(state, state_sz);
	state->hdr.vmx.smm.flags = ~(KVM_STATE_NESTED_SMM_GUEST_MODE |
				KVM_STATE_NESTED_SMM_VMXON);
	test_nested_state_expect_einval(vcpu, state);

	 
	set_default_vmx_state(state, state_sz);
	state->flags = 0;
	state->hdr.vmx.smm.flags = KVM_STATE_NESTED_SMM_GUEST_MODE;
	test_nested_state_expect_einval(vcpu, state);

	 
	set_default_vmx_state(state, state_sz);
	state->size = sizeof(*state);
	state->flags = 0;
	test_nested_state_expect_einval(vcpu, state);

	set_default_vmx_state(state, state_sz);
	state->size = sizeof(*state);
	state->flags = 0;
	state->hdr.vmx.vmcs12_pa = -1;
	test_nested_state(vcpu, state);

	 
	set_default_vmx_state(state, state_sz);
	state->flags = 0;
	test_nested_state(vcpu, state);

	 
	set_default_vmx_state(state, state_sz);
	state->size = sizeof(*state);
	state->flags = 0;
	state->hdr.vmx.vmcs12_pa = -1;
	state->hdr.vmx.flags = ~0;
	test_nested_state_expect_einval(vcpu, state);

	 
	set_default_vmx_state(state, state_sz);
	state->hdr.vmx.vmxon_pa = 0;
	state->hdr.vmx.vmcs12_pa = 0;
	test_nested_state_expect_einval(vcpu, state);

	 
	set_default_vmx_state(state, state_sz);
	state->hdr.vmx.vmxon_pa = -1ull;
	state->hdr.vmx.vmcs12_pa = -1ull;
	state->flags = 0;
	test_nested_state(vcpu, state);
	vcpu_nested_state_get(vcpu, state);
	TEST_ASSERT(state->size >= sizeof(*state) && state->size <= state_sz,
		    "Size must be between %ld and %d.  The size returned was %d.",
		    sizeof(*state), state_sz, state->size);
	TEST_ASSERT(state->hdr.vmx.vmxon_pa == -1ull, "vmxon_pa must be -1ull.");
	TEST_ASSERT(state->hdr.vmx.vmcs12_pa == -1ull, "vmcs_pa must be -1ull.");

	free(state);
}

int main(int argc, char *argv[])
{
	struct kvm_vm *vm;
	struct kvm_nested_state state;
	struct kvm_vcpu *vcpu;

	have_evmcs = kvm_check_cap(KVM_CAP_HYPERV_ENLIGHTENED_VMCS);

	TEST_REQUIRE(kvm_has_cap(KVM_CAP_NESTED_STATE));

	 
	TEST_REQUIRE(kvm_cpu_has(X86_FEATURE_VMX));

	vm = vm_create_with_one_vcpu(&vcpu, NULL);

	 
	vcpu_clear_cpuid_feature(vcpu, X86_FEATURE_VMX);

	 
	test_nested_state_expect_efault(vcpu, NULL);

	 
	set_default_state(&state);
	state.size = 0;
	test_nested_state_expect_einval(vcpu, &state);

	 
	set_default_state(&state);
	state.flags = 0xf;
	test_nested_state_expect_einval(vcpu, &state);

	 
	set_default_state(&state);
	state.flags = KVM_STATE_NESTED_RUN_PENDING;
	test_nested_state_expect_einval(vcpu, &state);

	test_vmx_nested_state(vcpu);

	kvm_vm_free(vm);
	return 0;
}
