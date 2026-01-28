#include "kvm_util.h"
#include "processor.h"
#include "test_util.h"
static int add_init_2vcpus(struct kvm_vcpu_init *init0,
			   struct kvm_vcpu_init *init1)
{
	struct kvm_vcpu *vcpu0, *vcpu1;
	struct kvm_vm *vm;
	int ret;
	vm = vm_create_barebones();
	vcpu0 = __vm_vcpu_add(vm, 0);
	ret = __vcpu_ioctl(vcpu0, KVM_ARM_VCPU_INIT, init0);
	if (ret)
		goto free_exit;
	vcpu1 = __vm_vcpu_add(vm, 1);
	ret = __vcpu_ioctl(vcpu1, KVM_ARM_VCPU_INIT, init1);
free_exit:
	kvm_vm_free(vm);
	return ret;
}
static int add_2vcpus_init_2vcpus(struct kvm_vcpu_init *init0,
				  struct kvm_vcpu_init *init1)
{
	struct kvm_vcpu *vcpu0, *vcpu1;
	struct kvm_vm *vm;
	int ret;
	vm = vm_create_barebones();
	vcpu0 = __vm_vcpu_add(vm, 0);
	vcpu1 = __vm_vcpu_add(vm, 1);
	ret = __vcpu_ioctl(vcpu0, KVM_ARM_VCPU_INIT, init0);
	if (ret)
		goto free_exit;
	ret = __vcpu_ioctl(vcpu1, KVM_ARM_VCPU_INIT, init1);
free_exit:
	kvm_vm_free(vm);
	return ret;
}
int main(void)
{
	struct kvm_vcpu_init init0, init1;
	struct kvm_vm *vm;
	int ret;
	TEST_REQUIRE(kvm_has_cap(KVM_CAP_ARM_EL1_32BIT));
	vm = vm_create_barebones();
	vm_ioctl(vm, KVM_ARM_PREFERRED_TARGET, &init0);
	kvm_vm_free(vm);
	init1 = init0;
	ret = add_init_2vcpus(&init0, &init0);
	TEST_ASSERT(ret == 0,
		    "Configuring 64bit EL1 vCPUs failed unexpectedly");
	ret = add_2vcpus_init_2vcpus(&init0, &init0);
	TEST_ASSERT(ret == 0,
		    "Configuring 64bit EL1 vCPUs failed unexpectedly");
	init0.features[0] = (1 << KVM_ARM_VCPU_EL1_32BIT);
	ret = add_init_2vcpus(&init0, &init0);
	TEST_ASSERT(ret == 0,
		    "Configuring 32bit EL1 vCPUs failed unexpectedly");
	ret = add_2vcpus_init_2vcpus(&init0, &init0);
	TEST_ASSERT(ret == 0,
		    "Configuring 32bit EL1 vCPUs failed unexpectedly");
	init0.features[0] = 0;
	init1.features[0] = (1 << KVM_ARM_VCPU_EL1_32BIT);
	ret = add_init_2vcpus(&init0, &init1);
	TEST_ASSERT(ret != 0,
		    "Configuring mixed-width vCPUs worked unexpectedly");
	ret = add_2vcpus_init_2vcpus(&init0, &init1);
	TEST_ASSERT(ret != 0,
		    "Configuring mixed-width vCPUs worked unexpectedly");
	return 0;
}
