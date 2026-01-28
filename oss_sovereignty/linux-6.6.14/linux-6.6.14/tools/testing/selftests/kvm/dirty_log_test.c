#define _GNU_SOURCE  
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>
#include <sys/types.h>
#include <signal.h>
#include <errno.h>
#include <linux/bitmap.h>
#include <linux/bitops.h>
#include <linux/atomic.h>
#include <asm/barrier.h>
#include "kvm_util.h"
#include "test_util.h"
#include "guest_modes.h"
#include "processor.h"
#define DIRTY_MEM_BITS 30  
#define PAGE_SHIFT_4K  12
#define TEST_MEM_SLOT_INDEX		1
#define DEFAULT_GUEST_TEST_MEM		0xc0000000
#define TEST_PAGES_PER_LOOP		1024
#define TEST_HOST_LOOP_N		32UL
#define TEST_HOST_LOOP_INTERVAL		10UL
#if defined(__s390x__)
# define BITOP_LE_SWIZZLE	((BITS_PER_LONG-1) & ~0x7)
# define test_bit_le(nr, addr) \
	test_bit((nr) ^ BITOP_LE_SWIZZLE, addr)
# define __set_bit_le(nr, addr) \
	__set_bit((nr) ^ BITOP_LE_SWIZZLE, addr)
# define __clear_bit_le(nr, addr) \
	__clear_bit((nr) ^ BITOP_LE_SWIZZLE, addr)
# define __test_and_set_bit_le(nr, addr) \
	__test_and_set_bit((nr) ^ BITOP_LE_SWIZZLE, addr)
# define __test_and_clear_bit_le(nr, addr) \
	__test_and_clear_bit((nr) ^ BITOP_LE_SWIZZLE, addr)
#else
# define test_bit_le			test_bit
# define __set_bit_le			__set_bit
# define __clear_bit_le			__clear_bit
# define __test_and_set_bit_le		__test_and_set_bit
# define __test_and_clear_bit_le	__test_and_clear_bit
#endif
#define TEST_DIRTY_RING_COUNT		65536
#define SIG_IPI SIGUSR1
static uint64_t host_page_size;
static uint64_t guest_page_size;
static uint64_t guest_num_pages;
static uint64_t random_array[TEST_PAGES_PER_LOOP];
static uint64_t iteration;
static uint64_t guest_test_phys_mem;
static uint64_t guest_test_virt_mem = DEFAULT_GUEST_TEST_MEM;
static void guest_code(void)
{
	uint64_t addr;
	int i;
	for (i = 0; i < guest_num_pages; i++) {
		addr = guest_test_virt_mem + i * guest_page_size;
		*(uint64_t *)addr = READ_ONCE(iteration);
	}
	while (true) {
		for (i = 0; i < TEST_PAGES_PER_LOOP; i++) {
			addr = guest_test_virt_mem;
			addr += (READ_ONCE(random_array[i]) % guest_num_pages)
				* guest_page_size;
			addr = align_down(addr, host_page_size);
			*(uint64_t *)addr = READ_ONCE(iteration);
		}
		GUEST_SYNC(1);
	}
}
static bool host_quit;
static void *host_test_mem;
static uint64_t host_num_pages;
static uint64_t host_dirty_count;
static uint64_t host_clear_count;
static uint64_t host_track_next_count;
static sem_t sem_vcpu_stop;
static sem_t sem_vcpu_cont;
static atomic_t vcpu_sync_stop_requested;
static bool dirty_ring_vcpu_ring_full;
static uint64_t dirty_ring_last_page;
enum log_mode_t {
	LOG_MODE_DIRTY_LOG = 0,
	LOG_MODE_CLEAR_LOG = 1,
	LOG_MODE_DIRTY_RING = 2,
	LOG_MODE_NUM,
	LOG_MODE_ALL = LOG_MODE_NUM,
};
static enum log_mode_t host_log_mode_option = LOG_MODE_ALL;
static enum log_mode_t host_log_mode;
static pthread_t vcpu_thread;
static uint32_t test_dirty_ring_count = TEST_DIRTY_RING_COUNT;
static void vcpu_kick(void)
{
	pthread_kill(vcpu_thread, SIG_IPI);
}
static void sem_wait_until(sem_t *sem)
{
	int ret;
	do
		ret = sem_wait(sem);
	while (ret == -1 && errno == EINTR);
}
static bool clear_log_supported(void)
{
	return kvm_has_cap(KVM_CAP_MANUAL_DIRTY_LOG_PROTECT2);
}
static void clear_log_create_vm_done(struct kvm_vm *vm)
{
	u64 manual_caps;
	manual_caps = kvm_check_cap(KVM_CAP_MANUAL_DIRTY_LOG_PROTECT2);
	TEST_ASSERT(manual_caps, "MANUAL_CAPS is zero!");
	manual_caps &= (KVM_DIRTY_LOG_MANUAL_PROTECT_ENABLE |
			KVM_DIRTY_LOG_INITIALLY_SET);
	vm_enable_cap(vm, KVM_CAP_MANUAL_DIRTY_LOG_PROTECT2, manual_caps);
}
static void dirty_log_collect_dirty_pages(struct kvm_vcpu *vcpu, int slot,
					  void *bitmap, uint32_t num_pages,
					  uint32_t *unused)
{
	kvm_vm_get_dirty_log(vcpu->vm, slot, bitmap);
}
static void clear_log_collect_dirty_pages(struct kvm_vcpu *vcpu, int slot,
					  void *bitmap, uint32_t num_pages,
					  uint32_t *unused)
{
	kvm_vm_get_dirty_log(vcpu->vm, slot, bitmap);
	kvm_vm_clear_dirty_log(vcpu->vm, slot, bitmap, 0, num_pages);
}
static void vcpu_handle_sync_stop(void)
{
	if (atomic_read(&vcpu_sync_stop_requested)) {
		atomic_set(&vcpu_sync_stop_requested, false);
		sem_post(&sem_vcpu_stop);
		sem_wait_until(&sem_vcpu_cont);
	}
}
static void default_after_vcpu_run(struct kvm_vcpu *vcpu, int ret, int err)
{
	struct kvm_run *run = vcpu->run;
	TEST_ASSERT(ret == 0 || (ret == -1 && err == EINTR),
		    "vcpu run failed: errno=%d", err);
	TEST_ASSERT(get_ucall(vcpu, NULL) == UCALL_SYNC,
		    "Invalid guest sync status: exit_reason=%s\n",
		    exit_reason_str(run->exit_reason));
	vcpu_handle_sync_stop();
}
static bool dirty_ring_supported(void)
{
	return (kvm_has_cap(KVM_CAP_DIRTY_LOG_RING) ||
		kvm_has_cap(KVM_CAP_DIRTY_LOG_RING_ACQ_REL));
}
static void dirty_ring_create_vm_done(struct kvm_vm *vm)
{
	uint64_t pages;
	uint32_t limit;
	pages = (1ul << (DIRTY_MEM_BITS - vm->page_shift)) + 3;
	pages = vm_adjust_num_guest_pages(vm->mode, pages);
	if (vm->page_size < getpagesize())
		pages = vm_num_host_pages(vm->mode, pages);
	limit = 1 << (31 - __builtin_clz(pages));
	test_dirty_ring_count = 1 << (31 - __builtin_clz(test_dirty_ring_count));
	test_dirty_ring_count = min(limit, test_dirty_ring_count);
	pr_info("dirty ring count: 0x%x\n", test_dirty_ring_count);
	vm_enable_dirty_ring(vm, test_dirty_ring_count *
			     sizeof(struct kvm_dirty_gfn));
}
static inline bool dirty_gfn_is_dirtied(struct kvm_dirty_gfn *gfn)
{
	return smp_load_acquire(&gfn->flags) == KVM_DIRTY_GFN_F_DIRTY;
}
static inline void dirty_gfn_set_collected(struct kvm_dirty_gfn *gfn)
{
	smp_store_release(&gfn->flags, KVM_DIRTY_GFN_F_RESET);
}
static uint32_t dirty_ring_collect_one(struct kvm_dirty_gfn *dirty_gfns,
				       int slot, void *bitmap,
				       uint32_t num_pages, uint32_t *fetch_index)
{
	struct kvm_dirty_gfn *cur;
	uint32_t count = 0;
	while (true) {
		cur = &dirty_gfns[*fetch_index % test_dirty_ring_count];
		if (!dirty_gfn_is_dirtied(cur))
			break;
		TEST_ASSERT(cur->slot == slot, "Slot number didn't match: "
			    "%u != %u", cur->slot, slot);
		TEST_ASSERT(cur->offset < num_pages, "Offset overflow: "
			    "0x%llx >= 0x%x", cur->offset, num_pages);
		__set_bit_le(cur->offset, bitmap);
		dirty_ring_last_page = cur->offset;
		dirty_gfn_set_collected(cur);
		(*fetch_index)++;
		count++;
	}
	return count;
}
static void dirty_ring_wait_vcpu(void)
{
	vcpu_kick();
	sem_wait_until(&sem_vcpu_stop);
}
static void dirty_ring_continue_vcpu(void)
{
	pr_info("Notifying vcpu to continue\n");
	sem_post(&sem_vcpu_cont);
}
static void dirty_ring_collect_dirty_pages(struct kvm_vcpu *vcpu, int slot,
					   void *bitmap, uint32_t num_pages,
					   uint32_t *ring_buf_idx)
{
	uint32_t count = 0, cleared;
	bool continued_vcpu = false;
	dirty_ring_wait_vcpu();
	if (!dirty_ring_vcpu_ring_full) {
		dirty_ring_continue_vcpu();
		continued_vcpu = true;
	}
	count = dirty_ring_collect_one(vcpu_map_dirty_ring(vcpu),
				       slot, bitmap, num_pages,
				       ring_buf_idx);
	cleared = kvm_vm_reset_dirty_ring(vcpu->vm);
	TEST_ASSERT(cleared == count, "Reset dirty pages (%u) mismatch "
		    "with collected (%u)", cleared, count);
	if (!continued_vcpu) {
		TEST_ASSERT(dirty_ring_vcpu_ring_full,
			    "Didn't continue vcpu even without ring full");
		dirty_ring_continue_vcpu();
	}
	pr_info("Iteration %ld collected %u pages\n", iteration, count);
}
static void dirty_ring_after_vcpu_run(struct kvm_vcpu *vcpu, int ret, int err)
{
	struct kvm_run *run = vcpu->run;
	if (get_ucall(vcpu, NULL) == UCALL_SYNC) {
		;
	} else if (run->exit_reason == KVM_EXIT_DIRTY_RING_FULL ||
		   (ret == -1 && err == EINTR)) {
		WRITE_ONCE(dirty_ring_vcpu_ring_full,
			   run->exit_reason == KVM_EXIT_DIRTY_RING_FULL);
		sem_post(&sem_vcpu_stop);
		pr_info("vcpu stops because %s...\n",
			dirty_ring_vcpu_ring_full ?
			"dirty ring is full" : "vcpu is kicked out");
		sem_wait_until(&sem_vcpu_cont);
		pr_info("vcpu continues now.\n");
	} else {
		TEST_ASSERT(false, "Invalid guest sync status: "
			    "exit_reason=%s\n",
			    exit_reason_str(run->exit_reason));
	}
}
static void dirty_ring_before_vcpu_join(void)
{
	sem_post(&sem_vcpu_cont);
}
struct log_mode {
	const char *name;
	bool (*supported)(void);
	void (*create_vm_done)(struct kvm_vm *vm);
	void (*collect_dirty_pages) (struct kvm_vcpu *vcpu, int slot,
				     void *bitmap, uint32_t num_pages,
				     uint32_t *ring_buf_idx);
	void (*after_vcpu_run)(struct kvm_vcpu *vcpu, int ret, int err);
	void (*before_vcpu_join) (void);
} log_modes[LOG_MODE_NUM] = {
	{
		.name = "dirty-log",
		.collect_dirty_pages = dirty_log_collect_dirty_pages,
		.after_vcpu_run = default_after_vcpu_run,
	},
	{
		.name = "clear-log",
		.supported = clear_log_supported,
		.create_vm_done = clear_log_create_vm_done,
		.collect_dirty_pages = clear_log_collect_dirty_pages,
		.after_vcpu_run = default_after_vcpu_run,
	},
	{
		.name = "dirty-ring",
		.supported = dirty_ring_supported,
		.create_vm_done = dirty_ring_create_vm_done,
		.collect_dirty_pages = dirty_ring_collect_dirty_pages,
		.before_vcpu_join = dirty_ring_before_vcpu_join,
		.after_vcpu_run = dirty_ring_after_vcpu_run,
	},
};
static unsigned long *host_bmap_track;
static void log_modes_dump(void)
{
	int i;
	printf("all");
	for (i = 0; i < LOG_MODE_NUM; i++)
		printf(", %s", log_modes[i].name);
	printf("\n");
}
static bool log_mode_supported(void)
{
	struct log_mode *mode = &log_modes[host_log_mode];
	if (mode->supported)
		return mode->supported();
	return true;
}
static void log_mode_create_vm_done(struct kvm_vm *vm)
{
	struct log_mode *mode = &log_modes[host_log_mode];
	if (mode->create_vm_done)
		mode->create_vm_done(vm);
}
static void log_mode_collect_dirty_pages(struct kvm_vcpu *vcpu, int slot,
					 void *bitmap, uint32_t num_pages,
					 uint32_t *ring_buf_idx)
{
	struct log_mode *mode = &log_modes[host_log_mode];
	TEST_ASSERT(mode->collect_dirty_pages != NULL,
		    "collect_dirty_pages() is required for any log mode!");
	mode->collect_dirty_pages(vcpu, slot, bitmap, num_pages, ring_buf_idx);
}
static void log_mode_after_vcpu_run(struct kvm_vcpu *vcpu, int ret, int err)
{
	struct log_mode *mode = &log_modes[host_log_mode];
	if (mode->after_vcpu_run)
		mode->after_vcpu_run(vcpu, ret, err);
}
static void log_mode_before_vcpu_join(void)
{
	struct log_mode *mode = &log_modes[host_log_mode];
	if (mode->before_vcpu_join)
		mode->before_vcpu_join();
}
static void generate_random_array(uint64_t *guest_array, uint64_t size)
{
	uint64_t i;
	for (i = 0; i < size; i++)
		guest_array[i] = random();
}
static void *vcpu_worker(void *data)
{
	int ret;
	struct kvm_vcpu *vcpu = data;
	struct kvm_vm *vm = vcpu->vm;
	uint64_t *guest_array;
	uint64_t pages_count = 0;
	struct kvm_signal_mask *sigmask = alloca(offsetof(struct kvm_signal_mask, sigset)
						 + sizeof(sigset_t));
	sigset_t *sigset = (sigset_t *) &sigmask->sigset;
	sigmask->len = 8;
	pthread_sigmask(0, NULL, sigset);
	sigdelset(sigset, SIG_IPI);
	vcpu_ioctl(vcpu, KVM_SET_SIGNAL_MASK, sigmask);
	sigemptyset(sigset);
	sigaddset(sigset, SIG_IPI);
	guest_array = addr_gva2hva(vm, (vm_vaddr_t)random_array);
	while (!READ_ONCE(host_quit)) {
		generate_random_array(guest_array, TEST_PAGES_PER_LOOP);
		pages_count += TEST_PAGES_PER_LOOP;
		ret = __vcpu_run(vcpu);
		if (ret == -1 && errno == EINTR) {
			int sig = -1;
			sigwait(sigset, &sig);
			assert(sig == SIG_IPI);
		}
		log_mode_after_vcpu_run(vcpu, ret, errno);
	}
	pr_info("Dirtied %"PRIu64" pages\n", pages_count);
	return NULL;
}
static void vm_dirty_log_verify(enum vm_guest_mode mode, unsigned long *bmap)
{
	uint64_t step = vm_num_host_pages(mode, 1);
	uint64_t page;
	uint64_t *value_ptr;
	uint64_t min_iter = 0;
	for (page = 0; page < host_num_pages; page += step) {
		value_ptr = host_test_mem + page * host_page_size;
		if (__test_and_clear_bit_le(page, host_bmap_track)) {
			host_track_next_count++;
			TEST_ASSERT(test_bit_le(page, bmap),
				    "Page %"PRIu64" should have its dirty bit "
				    "set in this iteration but it is missing",
				    page);
		}
		if (__test_and_clear_bit_le(page, bmap)) {
			bool matched;
			host_dirty_count++;
			matched = (*value_ptr == iteration ||
				   *value_ptr == iteration - 1);
			if (host_log_mode == LOG_MODE_DIRTY_RING && !matched) {
				if (*value_ptr == iteration - 2 && min_iter <= iteration - 2) {
					min_iter = iteration - 1;
					continue;
				} else if (page == dirty_ring_last_page) {
					continue;
				}
			}
			TEST_ASSERT(matched,
				    "Set page %"PRIu64" value %"PRIu64
				    " incorrect (iteration=%"PRIu64")",
				    page, *value_ptr, iteration);
		} else {
			host_clear_count++;
			TEST_ASSERT(*value_ptr <= iteration,
				    "Clear page %"PRIu64" value %"PRIu64
				    " incorrect (iteration=%"PRIu64")",
				    page, *value_ptr, iteration);
			if (*value_ptr == iteration) {
				__set_bit_le(page, host_bmap_track);
			}
		}
	}
}
static struct kvm_vm *create_vm(enum vm_guest_mode mode, struct kvm_vcpu **vcpu,
				uint64_t extra_mem_pages, void *guest_code)
{
	struct kvm_vm *vm;
	pr_info("Testing guest mode: %s\n", vm_guest_mode_string(mode));
	vm = __vm_create(mode, 1, extra_mem_pages);
	log_mode_create_vm_done(vm);
	*vcpu = vm_vcpu_add(vm, 0, guest_code);
	return vm;
}
struct test_params {
	unsigned long iterations;
	unsigned long interval;
	uint64_t phys_offset;
};
static void run_test(enum vm_guest_mode mode, void *arg)
{
	struct test_params *p = arg;
	struct kvm_vcpu *vcpu;
	struct kvm_vm *vm;
	unsigned long *bmap;
	uint32_t ring_buf_idx = 0;
	if (!log_mode_supported()) {
		print_skip("Log mode '%s' not supported",
			   log_modes[host_log_mode].name);
		return;
	}
	vm = create_vm(mode, &vcpu,
		       2ul << (DIRTY_MEM_BITS - PAGE_SHIFT_4K), guest_code);
	guest_page_size = vm->page_size;
	guest_num_pages = (1ul << (DIRTY_MEM_BITS - vm->page_shift)) + 3;
	guest_num_pages = vm_adjust_num_guest_pages(mode, guest_num_pages);
	host_page_size = getpagesize();
	host_num_pages = vm_num_host_pages(mode, guest_num_pages);
	if (!p->phys_offset) {
		guest_test_phys_mem = (vm->max_gfn - guest_num_pages) *
				      guest_page_size;
		guest_test_phys_mem = align_down(guest_test_phys_mem, host_page_size);
	} else {
		guest_test_phys_mem = p->phys_offset;
	}
#ifdef __s390x__
	guest_test_phys_mem = align_down(guest_test_phys_mem, 1 << 20);
#endif
	pr_info("guest physical test memory offset: 0x%lx\n", guest_test_phys_mem);
	bmap = bitmap_zalloc(host_num_pages);
	host_bmap_track = bitmap_zalloc(host_num_pages);
	vm_userspace_mem_region_add(vm, VM_MEM_SRC_ANONYMOUS,
				    guest_test_phys_mem,
				    TEST_MEM_SLOT_INDEX,
				    guest_num_pages,
				    KVM_MEM_LOG_DIRTY_PAGES);
	virt_map(vm, guest_test_virt_mem, guest_test_phys_mem, guest_num_pages);
	host_test_mem = addr_gpa2hva(vm, (vm_paddr_t)guest_test_phys_mem);
	sync_global_to_guest(vm, host_page_size);
	sync_global_to_guest(vm, guest_page_size);
	sync_global_to_guest(vm, guest_test_virt_mem);
	sync_global_to_guest(vm, guest_num_pages);
	iteration = 1;
	sync_global_to_guest(vm, iteration);
	host_quit = false;
	host_dirty_count = 0;
	host_clear_count = 0;
	host_track_next_count = 0;
	WRITE_ONCE(dirty_ring_vcpu_ring_full, false);
	pthread_create(&vcpu_thread, NULL, vcpu_worker, vcpu);
	while (iteration < p->iterations) {
		usleep(p->interval * 1000);
		log_mode_collect_dirty_pages(vcpu, TEST_MEM_SLOT_INDEX,
					     bmap, host_num_pages,
					     &ring_buf_idx);
		atomic_set(&vcpu_sync_stop_requested, true);
		sem_wait_until(&sem_vcpu_stop);
		assert(host_log_mode == LOG_MODE_DIRTY_RING ||
		       atomic_read(&vcpu_sync_stop_requested) == false);
		vm_dirty_log_verify(mode, bmap);
		sem_post(&sem_vcpu_cont);
		iteration++;
		sync_global_to_guest(vm, iteration);
	}
	host_quit = true;
	log_mode_before_vcpu_join();
	pthread_join(vcpu_thread, NULL);
	pr_info("Total bits checked: dirty (%"PRIu64"), clear (%"PRIu64"), "
		"track_next (%"PRIu64")\n", host_dirty_count, host_clear_count,
		host_track_next_count);
	free(bmap);
	free(host_bmap_track);
	kvm_vm_free(vm);
}
static void help(char *name)
{
	puts("");
	printf("usage: %s [-h] [-i iterations] [-I interval] "
	       "[-p offset] [-m mode]\n", name);
	puts("");
	printf(" -c: hint to dirty ring size, in number of entries\n");
	printf("     (only useful for dirty-ring test; default: %"PRIu32")\n",
	       TEST_DIRTY_RING_COUNT);
	printf(" -i: specify iteration counts (default: %"PRIu64")\n",
	       TEST_HOST_LOOP_N);
	printf(" -I: specify interval in ms (default: %"PRIu64" ms)\n",
	       TEST_HOST_LOOP_INTERVAL);
	printf(" -p: specify guest physical test memory offset\n"
	       "     Warning: a low offset can conflict with the loaded test code.\n");
	printf(" -M: specify the host logging mode "
	       "(default: run all log modes).  Supported modes: \n\t");
	log_modes_dump();
	guest_modes_help();
	puts("");
	exit(0);
}
int main(int argc, char *argv[])
{
	struct test_params p = {
		.iterations = TEST_HOST_LOOP_N,
		.interval = TEST_HOST_LOOP_INTERVAL,
	};
	int opt, i;
	sigset_t sigset;
	sem_init(&sem_vcpu_stop, 0, 0);
	sem_init(&sem_vcpu_cont, 0, 0);
	guest_modes_append_default();
	while ((opt = getopt(argc, argv, "c:hi:I:p:m:M:")) != -1) {
		switch (opt) {
		case 'c':
			test_dirty_ring_count = strtol(optarg, NULL, 10);
			break;
		case 'i':
			p.iterations = strtol(optarg, NULL, 10);
			break;
		case 'I':
			p.interval = strtol(optarg, NULL, 10);
			break;
		case 'p':
			p.phys_offset = strtoull(optarg, NULL, 0);
			break;
		case 'm':
			guest_modes_cmdline(optarg);
			break;
		case 'M':
			if (!strcmp(optarg, "all")) {
				host_log_mode_option = LOG_MODE_ALL;
				break;
			}
			for (i = 0; i < LOG_MODE_NUM; i++) {
				if (!strcmp(optarg, log_modes[i].name)) {
					pr_info("Setting log mode to: '%s'\n",
						optarg);
					host_log_mode_option = i;
					break;
				}
			}
			if (i == LOG_MODE_NUM) {
				printf("Log mode '%s' invalid. Please choose "
				       "from: ", optarg);
				log_modes_dump();
				exit(1);
			}
			break;
		case 'h':
		default:
			help(argv[0]);
			break;
		}
	}
	TEST_ASSERT(p.iterations > 2, "Iterations must be greater than two");
	TEST_ASSERT(p.interval > 0, "Interval must be greater than zero");
	pr_info("Test iterations: %"PRIu64", interval: %"PRIu64" (ms)\n",
		p.iterations, p.interval);
	srandom(time(0));
	sigemptyset(&sigset);
	sigaddset(&sigset, SIG_IPI);
	pthread_sigmask(SIG_BLOCK, &sigset, NULL);
	if (host_log_mode_option == LOG_MODE_ALL) {
		for (i = 0; i < LOG_MODE_NUM; i++) {
			pr_info("Testing Log Mode '%s'\n", log_modes[i].name);
			host_log_mode = i;
			for_each_guest_mode(run_test, &p);
		}
	} else {
		host_log_mode = host_log_mode_option;
		for_each_guest_mode(run_test, &p);
	}
	return 0;
}
