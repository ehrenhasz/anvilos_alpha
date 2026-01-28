#include <inttypes.h>
#include <limits.h>
#include <pthread.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "kvm_util.h"
#include "test_util.h"
#include "memstress.h"
#include "guest_modes.h"
#include "processor.h"
static int iteration;
static enum {
	ITERATION_ACCESS_MEMORY,
	ITERATION_MARK_IDLE,
} iteration_work;
static int vcpu_last_completed_iteration[KVM_MAX_VCPUS];
static bool overlap_memory_access;
struct test_params {
	enum vm_mem_backing_src_type backing_src;
	uint64_t vcpu_memory_bytes;
	int nr_vcpus;
};
static uint64_t pread_uint64(int fd, const char *filename, uint64_t index)
{
	uint64_t value;
	off_t offset = index * sizeof(value);
	TEST_ASSERT(pread(fd, &value, sizeof(value), offset) == sizeof(value),
		    "pread from %s offset 0x%" PRIx64 " failed!",
		    filename, offset);
	return value;
}
#define PAGEMAP_PRESENT (1ULL << 63)
#define PAGEMAP_PFN_MASK ((1ULL << 55) - 1)
static uint64_t lookup_pfn(int pagemap_fd, struct kvm_vm *vm, uint64_t gva)
{
	uint64_t hva = (uint64_t) addr_gva2hva(vm, gva);
	uint64_t entry;
	uint64_t pfn;
	entry = pread_uint64(pagemap_fd, "pagemap", hva / getpagesize());
	if (!(entry & PAGEMAP_PRESENT))
		return 0;
	pfn = entry & PAGEMAP_PFN_MASK;
	__TEST_REQUIRE(pfn, "Looking up PFNs requires CAP_SYS_ADMIN");
	return pfn;
}
static bool is_page_idle(int page_idle_fd, uint64_t pfn)
{
	uint64_t bits = pread_uint64(page_idle_fd, "page_idle", pfn / 64);
	return !!((bits >> (pfn % 64)) & 1);
}
static void mark_page_idle(int page_idle_fd, uint64_t pfn)
{
	uint64_t bits = 1ULL << (pfn % 64);
	TEST_ASSERT(pwrite(page_idle_fd, &bits, 8, 8 * (pfn / 64)) == 8,
		    "Set page_idle bits for PFN 0x%" PRIx64, pfn);
}
static void mark_vcpu_memory_idle(struct kvm_vm *vm,
				  struct memstress_vcpu_args *vcpu_args)
{
	int vcpu_idx = vcpu_args->vcpu_idx;
	uint64_t base_gva = vcpu_args->gva;
	uint64_t pages = vcpu_args->pages;
	uint64_t page;
	uint64_t still_idle = 0;
	uint64_t no_pfn = 0;
	int page_idle_fd;
	int pagemap_fd;
	if (overlap_memory_access && vcpu_idx)
		return;
	page_idle_fd = open("/sys/kernel/mm/page_idle/bitmap", O_RDWR);
	TEST_ASSERT(page_idle_fd > 0, "Failed to open page_idle.");
	pagemap_fd = open("/proc/self/pagemap", O_RDONLY);
	TEST_ASSERT(pagemap_fd > 0, "Failed to open pagemap.");
	for (page = 0; page < pages; page++) {
		uint64_t gva = base_gva + page * memstress_args.guest_page_size;
		uint64_t pfn = lookup_pfn(pagemap_fd, vm, gva);
		if (!pfn) {
			no_pfn++;
			continue;
		}
		if (is_page_idle(page_idle_fd, pfn)) {
			still_idle++;
			continue;
		}
		mark_page_idle(page_idle_fd, pfn);
	}
	TEST_ASSERT(no_pfn < pages / 100,
		    "vCPU %d: No PFN for %" PRIu64 " out of %" PRIu64 " pages.",
		    vcpu_idx, no_pfn, pages);
	if (still_idle >= pages / 10) {
#ifdef __x86_64__
		TEST_ASSERT(this_cpu_has(X86_FEATURE_HYPERVISOR),
			    "vCPU%d: Too many pages still idle (%lu out of %lu)",
			    vcpu_idx, still_idle, pages);
#endif
		printf("WARNING: vCPU%d: Too many pages still idle (%lu out of %lu), "
		       "this will affect performance results.\n",
		       vcpu_idx, still_idle, pages);
	}
	close(page_idle_fd);
	close(pagemap_fd);
}
static void assert_ucall(struct kvm_vcpu *vcpu, uint64_t expected_ucall)
{
	struct ucall uc;
	uint64_t actual_ucall = get_ucall(vcpu, &uc);
	TEST_ASSERT(expected_ucall == actual_ucall,
		    "Guest exited unexpectedly (expected ucall %" PRIu64
		    ", got %" PRIu64 ")",
		    expected_ucall, actual_ucall);
}
static bool spin_wait_for_next_iteration(int *current_iteration)
{
	int last_iteration = *current_iteration;
	do {
		if (READ_ONCE(memstress_args.stop_vcpus))
			return false;
		*current_iteration = READ_ONCE(iteration);
	} while (last_iteration == *current_iteration);
	return true;
}
static void vcpu_thread_main(struct memstress_vcpu_args *vcpu_args)
{
	struct kvm_vcpu *vcpu = vcpu_args->vcpu;
	struct kvm_vm *vm = memstress_args.vm;
	int vcpu_idx = vcpu_args->vcpu_idx;
	int current_iteration = 0;
	while (spin_wait_for_next_iteration(&current_iteration)) {
		switch (READ_ONCE(iteration_work)) {
		case ITERATION_ACCESS_MEMORY:
			vcpu_run(vcpu);
			assert_ucall(vcpu, UCALL_SYNC);
			break;
		case ITERATION_MARK_IDLE:
			mark_vcpu_memory_idle(vm, vcpu_args);
			break;
		};
		vcpu_last_completed_iteration[vcpu_idx] = current_iteration;
	}
}
static void spin_wait_for_vcpu(int vcpu_idx, int target_iteration)
{
	while (READ_ONCE(vcpu_last_completed_iteration[vcpu_idx]) !=
	       target_iteration) {
		continue;
	}
}
enum access_type {
	ACCESS_READ,
	ACCESS_WRITE,
};
static void run_iteration(struct kvm_vm *vm, int nr_vcpus, const char *description)
{
	struct timespec ts_start;
	struct timespec ts_elapsed;
	int next_iteration, i;
	next_iteration = ++iteration;
	clock_gettime(CLOCK_MONOTONIC, &ts_start);
	for (i = 0; i < nr_vcpus; i++)
		spin_wait_for_vcpu(i, next_iteration);
	ts_elapsed = timespec_elapsed(ts_start);
	pr_info("%-30s: %ld.%09lds\n",
		description, ts_elapsed.tv_sec, ts_elapsed.tv_nsec);
}
static void access_memory(struct kvm_vm *vm, int nr_vcpus,
			  enum access_type access, const char *description)
{
	memstress_set_write_percent(vm, (access == ACCESS_READ) ? 0 : 100);
	iteration_work = ITERATION_ACCESS_MEMORY;
	run_iteration(vm, nr_vcpus, description);
}
static void mark_memory_idle(struct kvm_vm *vm, int nr_vcpus)
{
	pr_debug("Marking VM memory idle (slow)...\n");
	iteration_work = ITERATION_MARK_IDLE;
	run_iteration(vm, nr_vcpus, "Mark memory idle");
}
static void run_test(enum vm_guest_mode mode, void *arg)
{
	struct test_params *params = arg;
	struct kvm_vm *vm;
	int nr_vcpus = params->nr_vcpus;
	vm = memstress_create_vm(mode, nr_vcpus, params->vcpu_memory_bytes, 1,
				 params->backing_src, !overlap_memory_access);
	memstress_start_vcpu_threads(nr_vcpus, vcpu_thread_main);
	pr_info("\n");
	access_memory(vm, nr_vcpus, ACCESS_WRITE, "Populating memory");
	access_memory(vm, nr_vcpus, ACCESS_WRITE, "Writing to populated memory");
	access_memory(vm, nr_vcpus, ACCESS_READ, "Reading from populated memory");
	mark_memory_idle(vm, nr_vcpus);
	access_memory(vm, nr_vcpus, ACCESS_WRITE, "Writing to idle memory");
	mark_memory_idle(vm, nr_vcpus);
	access_memory(vm, nr_vcpus, ACCESS_READ, "Reading from idle memory");
	memstress_join_vcpu_threads(nr_vcpus);
	memstress_destroy_vm(vm);
}
static void help(char *name)
{
	puts("");
	printf("usage: %s [-h] [-m mode] [-b vcpu_bytes] [-v vcpus] [-o]  [-s mem_type]\n",
	       name);
	puts("");
	printf(" -h: Display this help message.");
	guest_modes_help();
	printf(" -b: specify the size of the memory region which should be\n"
	       "     dirtied by each vCPU. e.g. 10M or 3G.\n"
	       "     (default: 1G)\n");
	printf(" -v: specify the number of vCPUs to run.\n");
	printf(" -o: Overlap guest memory accesses instead of partitioning\n"
	       "     them into a separate region of memory for each vCPU.\n");
	backing_src_help("-s");
	puts("");
	exit(0);
}
int main(int argc, char *argv[])
{
	struct test_params params = {
		.backing_src = DEFAULT_VM_MEM_SRC,
		.vcpu_memory_bytes = DEFAULT_PER_VCPU_MEM_SIZE,
		.nr_vcpus = 1,
	};
	int page_idle_fd;
	int opt;
	guest_modes_append_default();
	while ((opt = getopt(argc, argv, "hm:b:v:os:")) != -1) {
		switch (opt) {
		case 'm':
			guest_modes_cmdline(optarg);
			break;
		case 'b':
			params.vcpu_memory_bytes = parse_size(optarg);
			break;
		case 'v':
			params.nr_vcpus = atoi_positive("Number of vCPUs", optarg);
			break;
		case 'o':
			overlap_memory_access = true;
			break;
		case 's':
			params.backing_src = parse_backing_src_type(optarg);
			break;
		case 'h':
		default:
			help(argv[0]);
			break;
		}
	}
	page_idle_fd = open("/sys/kernel/mm/page_idle/bitmap", O_RDWR);
	__TEST_REQUIRE(page_idle_fd >= 0,
		       "CONFIG_IDLE_PAGE_TRACKING is not enabled");
	close(page_idle_fd);
	for_each_guest_mode(run_test, &params);
	return 0;
}
