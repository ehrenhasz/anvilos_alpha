#define _GNU_SOURCE  
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/resource.h>
#include "test_util.h"
#include "kvm_util.h"
#include "asm/kvm.h"
#include "linux/kvm.h"
void test_vcpu_creation(int first_vcpu_id, int num_vcpus)
{
	struct kvm_vm *vm;
	int i;
	pr_info("Testing creating %d vCPUs, with IDs %d...%d.\n",
		num_vcpus, first_vcpu_id, first_vcpu_id + num_vcpus - 1);
	vm = vm_create_barebones();
	for (i = first_vcpu_id; i < first_vcpu_id + num_vcpus; i++)
		__vm_vcpu_add(vm, i);
	kvm_vm_free(vm);
}
int main(int argc, char *argv[])
{
	int kvm_max_vcpu_id = kvm_check_cap(KVM_CAP_MAX_VCPU_ID);
	int kvm_max_vcpus = kvm_check_cap(KVM_CAP_MAX_VCPUS);
	int nr_fds_wanted = kvm_max_vcpus + 100;
	struct rlimit rl;
	pr_info("KVM_CAP_MAX_VCPU_ID: %d\n", kvm_max_vcpu_id);
	pr_info("KVM_CAP_MAX_VCPUS: %d\n", kvm_max_vcpus);
	TEST_ASSERT(!getrlimit(RLIMIT_NOFILE, &rl), "getrlimit() failed!");
	if (rl.rlim_cur < nr_fds_wanted) {
		rl.rlim_cur = nr_fds_wanted;
		if (rl.rlim_max < nr_fds_wanted) {
			int old_rlim_max = rl.rlim_max;
			rl.rlim_max = nr_fds_wanted;
			int r = setrlimit(RLIMIT_NOFILE, &rl);
			__TEST_REQUIRE(r >= 0,
				       "RLIMIT_NOFILE hard limit is too low (%d, wanted %d)\n",
				       old_rlim_max, nr_fds_wanted);
		} else {
			TEST_ASSERT(!setrlimit(RLIMIT_NOFILE, &rl), "setrlimit() failed!");
		}
	}
	if (!kvm_max_vcpu_id)
		kvm_max_vcpu_id = kvm_max_vcpus;
	TEST_ASSERT(kvm_max_vcpu_id >= kvm_max_vcpus,
		    "KVM_MAX_VCPU_IDS (%d) must be at least as large as KVM_MAX_VCPUS (%d).",
		    kvm_max_vcpu_id, kvm_max_vcpus);
	test_vcpu_creation(0, kvm_max_vcpus);
	if (kvm_max_vcpu_id > kvm_max_vcpus)
		test_vcpu_creation(
			kvm_max_vcpu_id - kvm_max_vcpus, kvm_max_vcpus);
	return 0;
}
