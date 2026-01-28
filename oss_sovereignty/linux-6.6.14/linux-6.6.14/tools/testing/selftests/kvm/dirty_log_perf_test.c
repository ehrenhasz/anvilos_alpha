#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <pthread.h>
#include <linux/bitmap.h>
#include "kvm_util.h"
#include "test_util.h"
#include "memstress.h"
#include "guest_modes.h"
#ifdef __aarch64__
#include "aarch64/vgic.h"
#define GICD_BASE_GPA			0x8000000ULL
#define GICR_BASE_GPA			0x80A0000ULL
static int gic_fd;
static void arch_setup_vm(struct kvm_vm *vm, unsigned int nr_vcpus)
{
	gic_fd = vgic_v3_setup(vm, nr_vcpus, 64, GICD_BASE_GPA, GICR_BASE_GPA);
}
static void arch_cleanup_vm(struct kvm_vm *vm)
{
	if (gic_fd > 0)
		close(gic_fd);
}
#else  
static void arch_setup_vm(struct kvm_vm *vm, unsigned int nr_vcpus)
{
}
static void arch_cleanup_vm(struct kvm_vm *vm)
{
}
#endif
#define TEST_HOST_LOOP_N		2UL
static int nr_vcpus = 1;
static uint64_t guest_percpu_mem_size = DEFAULT_PER_VCPU_MEM_SIZE;
static bool run_vcpus_while_disabling_dirty_logging;
static u64 dirty_log_manual_caps;
static bool host_quit;
static int iteration;
static int vcpu_last_completed_iteration[KVM_MAX_VCPUS];
static void vcpu_worker(struct memstress_vcpu_args *vcpu_args)
{
	struct kvm_vcpu *vcpu = vcpu_args->vcpu;
	int vcpu_idx = vcpu_args->vcpu_idx;
	uint64_t pages_count = 0;
	struct kvm_run *run;
	struct timespec start;
	struct timespec ts_diff;
	struct timespec total = (struct timespec){0};
	struct timespec avg;
	int ret;
	run = vcpu->run;
	while (!READ_ONCE(host_quit)) {
		int current_iteration = READ_ONCE(iteration);
		clock_gettime(CLOCK_MONOTONIC, &start);
		ret = _vcpu_run(vcpu);
		ts_diff = timespec_elapsed(start);
		TEST_ASSERT(ret == 0, "vcpu_run failed: %d\n", ret);
		TEST_ASSERT(get_ucall(vcpu, NULL) == UCALL_SYNC,
			    "Invalid guest sync status: exit_reason=%s\n",
			    exit_reason_str(run->exit_reason));
		pr_debug("Got sync event from vCPU %d\n", vcpu_idx);
		vcpu_last_completed_iteration[vcpu_idx] = current_iteration;
		pr_debug("vCPU %d updated last completed iteration to %d\n",
			 vcpu_idx, vcpu_last_completed_iteration[vcpu_idx]);
		if (current_iteration) {
			pages_count += vcpu_args->pages;
			total = timespec_add(total, ts_diff);
			pr_debug("vCPU %d iteration %d dirty memory time: %ld.%.9lds\n",
				vcpu_idx, current_iteration, ts_diff.tv_sec,
				ts_diff.tv_nsec);
		} else {
			pr_debug("vCPU %d iteration %d populate memory time: %ld.%.9lds\n",
				vcpu_idx, current_iteration, ts_diff.tv_sec,
				ts_diff.tv_nsec);
		}
		while (current_iteration == READ_ONCE(iteration) &&
		       READ_ONCE(iteration) >= 0 && !READ_ONCE(host_quit)) {}
	}
	avg = timespec_div(total, vcpu_last_completed_iteration[vcpu_idx]);
	pr_debug("\nvCPU %d dirtied 0x%lx pages over %d iterations in %ld.%.9lds. (Avg %ld.%.9lds/iteration)\n",
		vcpu_idx, pages_count, vcpu_last_completed_iteration[vcpu_idx],
		total.tv_sec, total.tv_nsec, avg.tv_sec, avg.tv_nsec);
}
struct test_params {
	unsigned long iterations;
	uint64_t phys_offset;
	bool partition_vcpu_memory_access;
	enum vm_mem_backing_src_type backing_src;
	int slots;
	uint32_t write_percent;
	uint32_t random_seed;
	bool random_access;
};
static void run_test(enum vm_guest_mode mode, void *arg)
{
	struct test_params *p = arg;
	struct kvm_vm *vm;
	unsigned long **bitmaps;
	uint64_t guest_num_pages;
	uint64_t host_num_pages;
	uint64_t pages_per_slot;
	struct timespec start;
	struct timespec ts_diff;
	struct timespec get_dirty_log_total = (struct timespec){0};
	struct timespec vcpu_dirty_total = (struct timespec){0};
	struct timespec avg;
	struct timespec clear_dirty_log_total = (struct timespec){0};
	int i;
	vm = memstress_create_vm(mode, nr_vcpus, guest_percpu_mem_size,
				 p->slots, p->backing_src,
				 p->partition_vcpu_memory_access);
	pr_info("Random seed: %u\n", p->random_seed);
	memstress_set_random_seed(vm, p->random_seed);
	memstress_set_write_percent(vm, p->write_percent);
	guest_num_pages = (nr_vcpus * guest_percpu_mem_size) >> vm->page_shift;
	guest_num_pages = vm_adjust_num_guest_pages(mode, guest_num_pages);
	host_num_pages = vm_num_host_pages(mode, guest_num_pages);
	pages_per_slot = host_num_pages / p->slots;
	bitmaps = memstress_alloc_bitmaps(p->slots, pages_per_slot);
	if (dirty_log_manual_caps)
		vm_enable_cap(vm, KVM_CAP_MANUAL_DIRTY_LOG_PROTECT2,
			      dirty_log_manual_caps);
	arch_setup_vm(vm, nr_vcpus);
	iteration = 0;
	host_quit = false;
	clock_gettime(CLOCK_MONOTONIC, &start);
	for (i = 0; i < nr_vcpus; i++)
		vcpu_last_completed_iteration[i] = -1;
	memstress_set_write_percent(vm, 100);
	memstress_set_random_access(vm, false);
	memstress_start_vcpu_threads(nr_vcpus, vcpu_worker);
	pr_debug("Starting iteration %d - Populating\n", iteration);
	for (i = 0; i < nr_vcpus; i++) {
		while (READ_ONCE(vcpu_last_completed_iteration[i]) !=
		       iteration)
			;
	}
	ts_diff = timespec_elapsed(start);
	pr_info("Populate memory time: %ld.%.9lds\n",
		ts_diff.tv_sec, ts_diff.tv_nsec);
	clock_gettime(CLOCK_MONOTONIC, &start);
	memstress_enable_dirty_logging(vm, p->slots);
	ts_diff = timespec_elapsed(start);
	pr_info("Enabling dirty logging time: %ld.%.9lds\n\n",
		ts_diff.tv_sec, ts_diff.tv_nsec);
	memstress_set_write_percent(vm, p->write_percent);
	memstress_set_random_access(vm, p->random_access);
	while (iteration < p->iterations) {
		clock_gettime(CLOCK_MONOTONIC, &start);
		iteration++;
		pr_debug("Starting iteration %d\n", iteration);
		for (i = 0; i < nr_vcpus; i++) {
			while (READ_ONCE(vcpu_last_completed_iteration[i])
			       != iteration)
				;
		}
		ts_diff = timespec_elapsed(start);
		vcpu_dirty_total = timespec_add(vcpu_dirty_total, ts_diff);
		pr_info("Iteration %d dirty memory time: %ld.%.9lds\n",
			iteration, ts_diff.tv_sec, ts_diff.tv_nsec);
		clock_gettime(CLOCK_MONOTONIC, &start);
		memstress_get_dirty_log(vm, bitmaps, p->slots);
		ts_diff = timespec_elapsed(start);
		get_dirty_log_total = timespec_add(get_dirty_log_total,
						   ts_diff);
		pr_info("Iteration %d get dirty log time: %ld.%.9lds\n",
			iteration, ts_diff.tv_sec, ts_diff.tv_nsec);
		if (dirty_log_manual_caps) {
			clock_gettime(CLOCK_MONOTONIC, &start);
			memstress_clear_dirty_log(vm, bitmaps, p->slots,
						  pages_per_slot);
			ts_diff = timespec_elapsed(start);
			clear_dirty_log_total = timespec_add(clear_dirty_log_total,
							     ts_diff);
			pr_info("Iteration %d clear dirty log time: %ld.%.9lds\n",
				iteration, ts_diff.tv_sec, ts_diff.tv_nsec);
		}
	}
	if (run_vcpus_while_disabling_dirty_logging)
		WRITE_ONCE(iteration, -1);
	clock_gettime(CLOCK_MONOTONIC, &start);
	memstress_disable_dirty_logging(vm, p->slots);
	ts_diff = timespec_elapsed(start);
	pr_info("Disabling dirty logging time: %ld.%.9lds\n",
		ts_diff.tv_sec, ts_diff.tv_nsec);
	host_quit = true;
	memstress_join_vcpu_threads(nr_vcpus);
	avg = timespec_div(get_dirty_log_total, p->iterations);
	pr_info("Get dirty log over %lu iterations took %ld.%.9lds. (Avg %ld.%.9lds/iteration)\n",
		p->iterations, get_dirty_log_total.tv_sec,
		get_dirty_log_total.tv_nsec, avg.tv_sec, avg.tv_nsec);
	if (dirty_log_manual_caps) {
		avg = timespec_div(clear_dirty_log_total, p->iterations);
		pr_info("Clear dirty log over %lu iterations took %ld.%.9lds. (Avg %ld.%.9lds/iteration)\n",
			p->iterations, clear_dirty_log_total.tv_sec,
			clear_dirty_log_total.tv_nsec, avg.tv_sec, avg.tv_nsec);
	}
	memstress_free_bitmaps(bitmaps, p->slots);
	arch_cleanup_vm(vm);
	memstress_destroy_vm(vm);
}
static void help(char *name)
{
	puts("");
	printf("usage: %s [-h] [-a] [-i iterations] [-p offset] [-g] "
	       "[-m mode] [-n] [-b vcpu bytes] [-v vcpus] [-o] [-r random seed ] [-s mem type]"
	       "[-x memslots] [-w percentage] [-c physical cpus to run test on]\n", name);
	puts("");
	printf(" -a: access memory randomly rather than in order.\n");
	printf(" -i: specify iteration counts (default: %"PRIu64")\n",
	       TEST_HOST_LOOP_N);
	printf(" -g: Do not enable KVM_CAP_MANUAL_DIRTY_LOG_PROTECT2. This\n"
	       "     makes KVM_GET_DIRTY_LOG clear the dirty log (i.e.\n"
	       "     KVM_DIRTY_LOG_MANUAL_PROTECT_ENABLE is not enabled)\n"
	       "     and writes will be tracked as soon as dirty logging is\n"
	       "     enabled on the memslot (i.e. KVM_DIRTY_LOG_INITIALLY_SET\n"
	       "     is not enabled).\n");
	printf(" -p: specify guest physical test memory offset\n"
	       "     Warning: a low offset can conflict with the loaded test code.\n");
	guest_modes_help();
	printf(" -n: Run the vCPUs in nested mode (L2)\n");
	printf(" -e: Run vCPUs while dirty logging is being disabled.  This\n"
	       "     can significantly increase runtime, especially if there\n"
	       "     isn't a dedicated pCPU for the main thread.\n");
	printf(" -b: specify the size of the memory region which should be\n"
	       "     dirtied by each vCPU. e.g. 10M or 3G.\n"
	       "     (default: 1G)\n");
	printf(" -v: specify the number of vCPUs to run.\n");
	printf(" -o: Overlap guest memory accesses instead of partitioning\n"
	       "     them into a separate region of memory for each vCPU.\n");
	printf(" -r: specify the starting random seed.\n");
	backing_src_help("-s");
	printf(" -x: Split the memory region into this number of memslots.\n"
	       "     (default: 1)\n");
	printf(" -w: specify the percentage of pages which should be written to\n"
	       "     as an integer from 0-100 inclusive. This is probabilistic,\n"
	       "     so -w X means each page has an X%% chance of writing\n"
	       "     and a (100-X)%% chance of reading.\n"
	       "     (default: 100 i.e. all pages are written to.)\n");
	kvm_print_vcpu_pinning_help();
	puts("");
	exit(0);
}
int main(int argc, char *argv[])
{
	int max_vcpus = kvm_check_cap(KVM_CAP_MAX_VCPUS);
	const char *pcpu_list = NULL;
	struct test_params p = {
		.iterations = TEST_HOST_LOOP_N,
		.partition_vcpu_memory_access = true,
		.backing_src = DEFAULT_VM_MEM_SRC,
		.slots = 1,
		.random_seed = 1,
		.write_percent = 100,
	};
	int opt;
	dirty_log_manual_caps =
		kvm_check_cap(KVM_CAP_MANUAL_DIRTY_LOG_PROTECT2);
	dirty_log_manual_caps &= (KVM_DIRTY_LOG_MANUAL_PROTECT_ENABLE |
				  KVM_DIRTY_LOG_INITIALLY_SET);
	guest_modes_append_default();
	while ((opt = getopt(argc, argv, "ab:c:eghi:m:nop:r:s:v:x:w:")) != -1) {
		switch (opt) {
		case 'a':
			p.random_access = true;
			break;
		case 'b':
			guest_percpu_mem_size = parse_size(optarg);
			break;
		case 'c':
			pcpu_list = optarg;
			break;
		case 'e':
			run_vcpus_while_disabling_dirty_logging = true;
			break;
		case 'g':
			dirty_log_manual_caps = 0;
			break;
		case 'h':
			help(argv[0]);
			break;
		case 'i':
			p.iterations = atoi_positive("Number of iterations", optarg);
			break;
		case 'm':
			guest_modes_cmdline(optarg);
			break;
		case 'n':
			memstress_args.nested = true;
			break;
		case 'o':
			p.partition_vcpu_memory_access = false;
			break;
		case 'p':
			p.phys_offset = strtoull(optarg, NULL, 0);
			break;
		case 'r':
			p.random_seed = atoi_positive("Random seed", optarg);
			break;
		case 's':
			p.backing_src = parse_backing_src_type(optarg);
			break;
		case 'v':
			nr_vcpus = atoi_positive("Number of vCPUs", optarg);
			TEST_ASSERT(nr_vcpus <= max_vcpus,
				    "Invalid number of vcpus, must be between 1 and %d", max_vcpus);
			break;
		case 'w':
			p.write_percent = atoi_non_negative("Write percentage", optarg);
			TEST_ASSERT(p.write_percent <= 100,
				    "Write percentage must be between 0 and 100");
			break;
		case 'x':
			p.slots = atoi_positive("Number of slots", optarg);
			break;
		default:
			help(argv[0]);
			break;
		}
	}
	if (pcpu_list) {
		kvm_parse_vcpu_pinning(pcpu_list, memstress_args.vcpu_to_pcpu,
				       nr_vcpus);
		memstress_args.pin_vcpus = true;
	}
	TEST_ASSERT(p.iterations >= 2, "The test should have at least two iterations");
	pr_info("Test iterations: %"PRIu64"\n",	p.iterations);
	for_each_guest_mode(run_test, &p);
	return 0;
}
