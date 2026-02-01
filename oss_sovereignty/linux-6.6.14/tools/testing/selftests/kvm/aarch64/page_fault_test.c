
 
#define _GNU_SOURCE
#include <linux/bitmap.h>
#include <fcntl.h>
#include <test_util.h>
#include <kvm_util.h>
#include <processor.h>
#include <asm/sysreg.h>
#include <linux/bitfield.h>
#include "guest_modes.h"
#include "userfaultfd_util.h"

 
#define TEST_GVA				0xc0000000
#define TEST_EXEC_GVA				(TEST_GVA + 0x8)
#define TEST_PTE_GVA				0xb0000000
#define TEST_DATA				0x0123456789ABCDEF

static uint64_t *guest_test_memory = (uint64_t *)TEST_GVA;

#define CMD_NONE				(0)
#define CMD_SKIP_TEST				(1ULL << 1)
#define CMD_HOLE_PT				(1ULL << 2)
#define CMD_HOLE_DATA				(1ULL << 3)
#define CMD_CHECK_WRITE_IN_DIRTY_LOG		(1ULL << 4)
#define CMD_CHECK_S1PTW_WR_IN_DIRTY_LOG		(1ULL << 5)
#define CMD_CHECK_NO_WRITE_IN_DIRTY_LOG		(1ULL << 6)
#define CMD_CHECK_NO_S1PTW_WR_IN_DIRTY_LOG	(1ULL << 7)
#define CMD_SET_PTE_AF				(1ULL << 8)

#define PREPARE_FN_NR				10
#define CHECK_FN_NR				10

static struct event_cnt {
	int mmio_exits;
	int fail_vcpu_runs;
	int uffd_faults;
	 
	pthread_mutex_t uffd_faults_mutex;
} events;

struct test_desc {
	const char *name;
	uint64_t mem_mark_cmd;
	 
	bool (*guest_prepare[PREPARE_FN_NR])(void);
	void (*guest_test)(void);
	void (*guest_test_check[CHECK_FN_NR])(void);
	uffd_handler_t uffd_pt_handler;
	uffd_handler_t uffd_data_handler;
	void (*dabt_handler)(struct ex_regs *regs);
	void (*iabt_handler)(struct ex_regs *regs);
	void (*mmio_handler)(struct kvm_vm *vm, struct kvm_run *run);
	void (*fail_vcpu_run_handler)(int ret);
	uint32_t pt_memslot_flags;
	uint32_t data_memslot_flags;
	bool skip;
	struct event_cnt expected_events;
};

struct test_params {
	enum vm_mem_backing_src_type src_type;
	struct test_desc *test_desc;
};

static inline void flush_tlb_page(uint64_t vaddr)
{
	uint64_t page = vaddr >> 12;

	dsb(ishst);
	asm volatile("tlbi vaae1is, %0" :: "r" (page));
	dsb(ish);
	isb();
}

static void guest_write64(void)
{
	uint64_t val;

	WRITE_ONCE(*guest_test_memory, TEST_DATA);
	val = READ_ONCE(*guest_test_memory);
	GUEST_ASSERT_EQ(val, TEST_DATA);
}

 
static bool guest_check_lse(void)
{
	uint64_t isar0 = read_sysreg(id_aa64isar0_el1);
	uint64_t atomic;

	atomic = FIELD_GET(ARM64_FEATURE_MASK(ID_AA64ISAR0_ATOMICS), isar0);
	return atomic >= 2;
}

static bool guest_check_dc_zva(void)
{
	uint64_t dczid = read_sysreg(dczid_el0);
	uint64_t dzp = FIELD_GET(ARM64_FEATURE_MASK(DCZID_DZP), dczid);

	return dzp == 0;
}

 
static void guest_cas(void)
{
	uint64_t val;

	GUEST_ASSERT(guest_check_lse());
	asm volatile(".arch_extension lse\n"
		     "casal %0, %1, [%2]\n"
		     :: "r" (0ul), "r" (TEST_DATA), "r" (guest_test_memory));
	val = READ_ONCE(*guest_test_memory);
	GUEST_ASSERT_EQ(val, TEST_DATA);
}

static void guest_read64(void)
{
	uint64_t val;

	val = READ_ONCE(*guest_test_memory);
	GUEST_ASSERT_EQ(val, 0);
}

 
static void guest_at(void)
{
	uint64_t par;

	asm volatile("at s1e1r, %0" :: "r" (guest_test_memory));
	par = read_sysreg(par_el1);
	isb();

	 
	GUEST_ASSERT_EQ(par & 1, 0);
}

 
static void guest_dc_zva(void)
{
	uint16_t val;

	asm volatile("dc zva, %0" :: "r" (guest_test_memory));
	dsb(ish);
	val = READ_ONCE(*guest_test_memory);
	GUEST_ASSERT_EQ(val, 0);
}

 
static void guest_ld_preidx(void)
{
	uint64_t val;
	uint64_t addr = TEST_GVA - 8;

	 
	asm volatile("ldr %0, [%1, #8]!"
		     : "=r" (val), "+r" (addr));
	GUEST_ASSERT_EQ(val, 0);
	GUEST_ASSERT_EQ(addr, TEST_GVA);
}

static void guest_st_preidx(void)
{
	uint64_t val = TEST_DATA;
	uint64_t addr = TEST_GVA - 8;

	asm volatile("str %0, [%1, #8]!"
		     : "+r" (val), "+r" (addr));

	GUEST_ASSERT_EQ(addr, TEST_GVA);
	val = READ_ONCE(*guest_test_memory);
}

static bool guest_set_ha(void)
{
	uint64_t mmfr1 = read_sysreg(id_aa64mmfr1_el1);
	uint64_t hadbs, tcr;

	 
	hadbs = FIELD_GET(ARM64_FEATURE_MASK(ID_AA64MMFR1_HADBS), mmfr1);
	if (hadbs == 0)
		return false;

	tcr = read_sysreg(tcr_el1) | TCR_EL1_HA;
	write_sysreg(tcr, tcr_el1);
	isb();

	return true;
}

static bool guest_clear_pte_af(void)
{
	*((uint64_t *)TEST_PTE_GVA) &= ~PTE_AF;
	flush_tlb_page(TEST_GVA);

	return true;
}

static void guest_check_pte_af(void)
{
	dsb(ish);
	GUEST_ASSERT_EQ(*((uint64_t *)TEST_PTE_GVA) & PTE_AF, PTE_AF);
}

static void guest_check_write_in_dirty_log(void)
{
	GUEST_SYNC(CMD_CHECK_WRITE_IN_DIRTY_LOG);
}

static void guest_check_no_write_in_dirty_log(void)
{
	GUEST_SYNC(CMD_CHECK_NO_WRITE_IN_DIRTY_LOG);
}

static void guest_check_s1ptw_wr_in_dirty_log(void)
{
	GUEST_SYNC(CMD_CHECK_S1PTW_WR_IN_DIRTY_LOG);
}

static void guest_check_no_s1ptw_wr_in_dirty_log(void)
{
	GUEST_SYNC(CMD_CHECK_NO_S1PTW_WR_IN_DIRTY_LOG);
}

static void guest_exec(void)
{
	int (*code)(void) = (int (*)(void))TEST_EXEC_GVA;
	int ret;

	ret = code();
	GUEST_ASSERT_EQ(ret, 0x77);
}

static bool guest_prepare(struct test_desc *test)
{
	bool (*prepare_fn)(void);
	int i;

	for (i = 0; i < PREPARE_FN_NR; i++) {
		prepare_fn = test->guest_prepare[i];
		if (prepare_fn && !prepare_fn())
			return false;
	}

	return true;
}

static void guest_test_check(struct test_desc *test)
{
	void (*check_fn)(void);
	int i;

	for (i = 0; i < CHECK_FN_NR; i++) {
		check_fn = test->guest_test_check[i];
		if (check_fn)
			check_fn();
	}
}

static void guest_code(struct test_desc *test)
{
	if (!guest_prepare(test))
		GUEST_SYNC(CMD_SKIP_TEST);

	GUEST_SYNC(test->mem_mark_cmd);

	if (test->guest_test)
		test->guest_test();

	guest_test_check(test);
	GUEST_DONE();
}

static void no_dabt_handler(struct ex_regs *regs)
{
	GUEST_FAIL("Unexpected dabt, far_el1 = 0x%llx", read_sysreg(far_el1));
}

static void no_iabt_handler(struct ex_regs *regs)
{
	GUEST_FAIL("Unexpected iabt, pc = 0x%lx", regs->pc);
}

static struct uffd_args {
	char *copy;
	void *hva;
	uint64_t paging_size;
} pt_args, data_args;

 
static int uffd_generic_handler(int uffd_mode, int uffd, struct uffd_msg *msg,
				struct uffd_args *args)
{
	uint64_t addr = msg->arg.pagefault.address;
	uint64_t flags = msg->arg.pagefault.flags;
	struct uffdio_copy copy;
	int ret;

	TEST_ASSERT(uffd_mode == UFFDIO_REGISTER_MODE_MISSING,
		    "The only expected UFFD mode is MISSING");
	TEST_ASSERT_EQ(addr, (uint64_t)args->hva);

	pr_debug("uffd fault: addr=%p write=%d\n",
		 (void *)addr, !!(flags & UFFD_PAGEFAULT_FLAG_WRITE));

	copy.src = (uint64_t)args->copy;
	copy.dst = addr;
	copy.len = args->paging_size;
	copy.mode = 0;

	ret = ioctl(uffd, UFFDIO_COPY, &copy);
	if (ret == -1) {
		pr_info("Failed UFFDIO_COPY in 0x%lx with errno: %d\n",
			addr, errno);
		return ret;
	}

	pthread_mutex_lock(&events.uffd_faults_mutex);
	events.uffd_faults += 1;
	pthread_mutex_unlock(&events.uffd_faults_mutex);
	return 0;
}

static int uffd_pt_handler(int mode, int uffd, struct uffd_msg *msg)
{
	return uffd_generic_handler(mode, uffd, msg, &pt_args);
}

static int uffd_data_handler(int mode, int uffd, struct uffd_msg *msg)
{
	return uffd_generic_handler(mode, uffd, msg, &data_args);
}

static void setup_uffd_args(struct userspace_mem_region *region,
			    struct uffd_args *args)
{
	args->hva = (void *)region->region.userspace_addr;
	args->paging_size = region->region.memory_size;

	args->copy = malloc(args->paging_size);
	TEST_ASSERT(args->copy, "Failed to allocate data copy.");
	memcpy(args->copy, args->hva, args->paging_size);
}

static void setup_uffd(struct kvm_vm *vm, struct test_params *p,
		       struct uffd_desc **pt_uffd, struct uffd_desc **data_uffd)
{
	struct test_desc *test = p->test_desc;
	int uffd_mode = UFFDIO_REGISTER_MODE_MISSING;

	setup_uffd_args(vm_get_mem_region(vm, MEM_REGION_PT), &pt_args);
	setup_uffd_args(vm_get_mem_region(vm, MEM_REGION_TEST_DATA), &data_args);

	*pt_uffd = NULL;
	if (test->uffd_pt_handler)
		*pt_uffd = uffd_setup_demand_paging(uffd_mode, 0,
						    pt_args.hva,
						    pt_args.paging_size,
						    test->uffd_pt_handler);

	*data_uffd = NULL;
	if (test->uffd_data_handler)
		*data_uffd = uffd_setup_demand_paging(uffd_mode, 0,
						      data_args.hva,
						      data_args.paging_size,
						      test->uffd_data_handler);
}

static void free_uffd(struct test_desc *test, struct uffd_desc *pt_uffd,
		      struct uffd_desc *data_uffd)
{
	if (test->uffd_pt_handler)
		uffd_stop_demand_paging(pt_uffd);
	if (test->uffd_data_handler)
		uffd_stop_demand_paging(data_uffd);

	free(pt_args.copy);
	free(data_args.copy);
}

static int uffd_no_handler(int mode, int uffd, struct uffd_msg *msg)
{
	TEST_FAIL("There was no UFFD fault expected.");
	return -1;
}

 
static bool punch_hole_in_backing_store(struct kvm_vm *vm,
					struct userspace_mem_region *region)
{
	void *hva = (void *)region->region.userspace_addr;
	uint64_t paging_size = region->region.memory_size;
	int ret, fd = region->fd;

	if (fd != -1) {
		ret = fallocate(fd, FALLOC_FL_PUNCH_HOLE | FALLOC_FL_KEEP_SIZE,
				0, paging_size);
		TEST_ASSERT(ret == 0, "fallocate failed\n");
	} else {
		ret = madvise(hva, paging_size, MADV_DONTNEED);
		TEST_ASSERT(ret == 0, "madvise failed\n");
	}

	return true;
}

static void mmio_on_test_gpa_handler(struct kvm_vm *vm, struct kvm_run *run)
{
	struct userspace_mem_region *region;
	void *hva;

	region = vm_get_mem_region(vm, MEM_REGION_TEST_DATA);
	hva = (void *)region->region.userspace_addr;

	TEST_ASSERT_EQ(run->mmio.phys_addr, region->region.guest_phys_addr);

	memcpy(hva, run->mmio.data, run->mmio.len);
	events.mmio_exits += 1;
}

static void mmio_no_handler(struct kvm_vm *vm, struct kvm_run *run)
{
	uint64_t data;

	memcpy(&data, run->mmio.data, sizeof(data));
	pr_debug("addr=%lld len=%d w=%d data=%lx\n",
		 run->mmio.phys_addr, run->mmio.len,
		 run->mmio.is_write, data);
	TEST_FAIL("There was no MMIO exit expected.");
}

static bool check_write_in_dirty_log(struct kvm_vm *vm,
				     struct userspace_mem_region *region,
				     uint64_t host_pg_nr)
{
	unsigned long *bmap;
	bool first_page_dirty;
	uint64_t size = region->region.memory_size;

	 
	bmap = bitmap_zalloc(size / getpagesize());
	kvm_vm_get_dirty_log(vm, region->region.slot, bmap);
	first_page_dirty = test_bit(host_pg_nr, bmap);
	free(bmap);
	return first_page_dirty;
}

 
static bool handle_cmd(struct kvm_vm *vm, int cmd)
{
	struct userspace_mem_region *data_region, *pt_region;
	bool continue_test = true;
	uint64_t pte_gpa, pte_pg;

	data_region = vm_get_mem_region(vm, MEM_REGION_TEST_DATA);
	pt_region = vm_get_mem_region(vm, MEM_REGION_PT);
	pte_gpa = addr_hva2gpa(vm, virt_get_pte_hva(vm, TEST_GVA));
	pte_pg = (pte_gpa - pt_region->region.guest_phys_addr) / getpagesize();

	if (cmd == CMD_SKIP_TEST)
		continue_test = false;

	if (cmd & CMD_HOLE_PT)
		continue_test = punch_hole_in_backing_store(vm, pt_region);
	if (cmd & CMD_HOLE_DATA)
		continue_test = punch_hole_in_backing_store(vm, data_region);
	if (cmd & CMD_CHECK_WRITE_IN_DIRTY_LOG)
		TEST_ASSERT(check_write_in_dirty_log(vm, data_region, 0),
			    "Missing write in dirty log");
	if (cmd & CMD_CHECK_S1PTW_WR_IN_DIRTY_LOG)
		TEST_ASSERT(check_write_in_dirty_log(vm, pt_region, pte_pg),
			    "Missing s1ptw write in dirty log");
	if (cmd & CMD_CHECK_NO_WRITE_IN_DIRTY_LOG)
		TEST_ASSERT(!check_write_in_dirty_log(vm, data_region, 0),
			    "Unexpected write in dirty log");
	if (cmd & CMD_CHECK_NO_S1PTW_WR_IN_DIRTY_LOG)
		TEST_ASSERT(!check_write_in_dirty_log(vm, pt_region, pte_pg),
			    "Unexpected s1ptw write in dirty log");

	return continue_test;
}

void fail_vcpu_run_no_handler(int ret)
{
	TEST_FAIL("Unexpected vcpu run failure\n");
}

void fail_vcpu_run_mmio_no_syndrome_handler(int ret)
{
	TEST_ASSERT(errno == ENOSYS,
		    "The mmio handler should have returned not implemented.");
	events.fail_vcpu_runs += 1;
}

typedef uint32_t aarch64_insn_t;
extern aarch64_insn_t __exec_test[2];

noinline void __return_0x77(void)
{
	asm volatile("__exec_test: mov x0, #0x77\n"
		     "ret\n");
}

 
static void load_exec_code_for_test(struct kvm_vm *vm)
{
	uint64_t *code;
	struct userspace_mem_region *region;
	void *hva;

	region = vm_get_mem_region(vm, MEM_REGION_TEST_DATA);
	hva = (void *)region->region.userspace_addr;

	assert(TEST_EXEC_GVA > TEST_GVA);
	code = hva + TEST_EXEC_GVA - TEST_GVA;
	memcpy(code, __exec_test, sizeof(__exec_test));
}

static void setup_abort_handlers(struct kvm_vm *vm, struct kvm_vcpu *vcpu,
				 struct test_desc *test)
{
	vm_init_descriptor_tables(vm);
	vcpu_init_descriptor_tables(vcpu);

	vm_install_sync_handler(vm, VECTOR_SYNC_CURRENT,
				ESR_EC_DABT, no_dabt_handler);
	vm_install_sync_handler(vm, VECTOR_SYNC_CURRENT,
				ESR_EC_IABT, no_iabt_handler);
}

static void setup_gva_maps(struct kvm_vm *vm)
{
	struct userspace_mem_region *region;
	uint64_t pte_gpa;

	region = vm_get_mem_region(vm, MEM_REGION_TEST_DATA);
	 
	virt_pg_map(vm, TEST_GVA, region->region.guest_phys_addr);
	 
	pte_gpa = addr_hva2gpa(vm, virt_get_pte_hva(vm, TEST_GVA));
	virt_pg_map(vm, TEST_PTE_GVA, pte_gpa);
}

enum pf_test_memslots {
	CODE_AND_DATA_MEMSLOT,
	PAGE_TABLE_MEMSLOT,
	TEST_DATA_MEMSLOT,
};

 
static void setup_memslots(struct kvm_vm *vm, struct test_params *p)
{
	uint64_t backing_src_pagesz = get_backing_src_pagesz(p->src_type);
	uint64_t guest_page_size = vm->page_size;
	uint64_t max_gfn = vm_compute_max_gfn(vm);
	 
	uint64_t code_npages = 512;
	uint64_t pt_size, data_size, data_gpa;

	 
	pt_size = 26 * guest_page_size;

	 
	pt_size = align_up(pt_size, backing_src_pagesz);
	data_size = align_up(guest_page_size, backing_src_pagesz);
	data_gpa = (max_gfn * guest_page_size) - data_size;
	data_gpa = align_down(data_gpa, backing_src_pagesz);

	vm_userspace_mem_region_add(vm, VM_MEM_SRC_ANONYMOUS, 0,
				    CODE_AND_DATA_MEMSLOT, code_npages, 0);
	vm->memslots[MEM_REGION_CODE] = CODE_AND_DATA_MEMSLOT;
	vm->memslots[MEM_REGION_DATA] = CODE_AND_DATA_MEMSLOT;

	vm_userspace_mem_region_add(vm, p->src_type, data_gpa - pt_size,
				    PAGE_TABLE_MEMSLOT, pt_size / guest_page_size,
				    p->test_desc->pt_memslot_flags);
	vm->memslots[MEM_REGION_PT] = PAGE_TABLE_MEMSLOT;

	vm_userspace_mem_region_add(vm, p->src_type, data_gpa, TEST_DATA_MEMSLOT,
				    data_size / guest_page_size,
				    p->test_desc->data_memslot_flags);
	vm->memslots[MEM_REGION_TEST_DATA] = TEST_DATA_MEMSLOT;
}

static void setup_ucall(struct kvm_vm *vm)
{
	struct userspace_mem_region *region = vm_get_mem_region(vm, MEM_REGION_TEST_DATA);

	ucall_init(vm, region->region.guest_phys_addr + region->region.memory_size);
}

static void setup_default_handlers(struct test_desc *test)
{
	if (!test->mmio_handler)
		test->mmio_handler = mmio_no_handler;

	if (!test->fail_vcpu_run_handler)
		test->fail_vcpu_run_handler = fail_vcpu_run_no_handler;
}

static void check_event_counts(struct test_desc *test)
{
	TEST_ASSERT_EQ(test->expected_events.uffd_faults, events.uffd_faults);
	TEST_ASSERT_EQ(test->expected_events.mmio_exits, events.mmio_exits);
	TEST_ASSERT_EQ(test->expected_events.fail_vcpu_runs, events.fail_vcpu_runs);
}

static void print_test_banner(enum vm_guest_mode mode, struct test_params *p)
{
	struct test_desc *test = p->test_desc;

	pr_debug("Test: %s\n", test->name);
	pr_debug("Testing guest mode: %s\n", vm_guest_mode_string(mode));
	pr_debug("Testing memory backing src type: %s\n",
		 vm_mem_backing_src_alias(p->src_type)->name);
}

static void reset_event_counts(void)
{
	memset(&events, 0, sizeof(events));
}

 
static void vcpu_run_loop(struct kvm_vm *vm, struct kvm_vcpu *vcpu,
			  struct test_desc *test)
{
	struct kvm_run *run;
	struct ucall uc;
	int ret;

	run = vcpu->run;

	for (;;) {
		ret = _vcpu_run(vcpu);
		if (ret) {
			test->fail_vcpu_run_handler(ret);
			goto done;
		}

		switch (get_ucall(vcpu, &uc)) {
		case UCALL_SYNC:
			if (!handle_cmd(vm, uc.args[1])) {
				test->skip = true;
				goto done;
			}
			break;
		case UCALL_ABORT:
			REPORT_GUEST_ASSERT(uc);
			break;
		case UCALL_DONE:
			goto done;
		case UCALL_NONE:
			if (run->exit_reason == KVM_EXIT_MMIO)
				test->mmio_handler(vm, run);
			break;
		default:
			TEST_FAIL("Unknown ucall %lu", uc.cmd);
		}
	}

done:
	pr_debug(test->skip ? "Skipped.\n" : "Done.\n");
}

static void run_test(enum vm_guest_mode mode, void *arg)
{
	struct test_params *p = (struct test_params *)arg;
	struct test_desc *test = p->test_desc;
	struct kvm_vm *vm;
	struct kvm_vcpu *vcpu;
	struct uffd_desc *pt_uffd, *data_uffd;

	print_test_banner(mode, p);

	vm = ____vm_create(mode);
	setup_memslots(vm, p);
	kvm_vm_elf_load(vm, program_invocation_name);
	setup_ucall(vm);
	vcpu = vm_vcpu_add(vm, 0, guest_code);

	setup_gva_maps(vm);

	reset_event_counts();

	 
	load_exec_code_for_test(vm);
	setup_uffd(vm, p, &pt_uffd, &data_uffd);
	setup_abort_handlers(vm, vcpu, test);
	setup_default_handlers(test);
	vcpu_args_set(vcpu, 1, test);

	vcpu_run_loop(vm, vcpu, test);

	kvm_vm_free(vm);
	free_uffd(test, pt_uffd, data_uffd);

	 
	if (!test->skip)
		check_event_counts(test);
}

static void help(char *name)
{
	puts("");
	printf("usage: %s [-h] [-s mem-type]\n", name);
	puts("");
	guest_modes_help();
	backing_src_help("-s");
	puts("");
}

#define SNAME(s)			#s
#define SCAT2(a, b)			SNAME(a ## _ ## b)
#define SCAT3(a, b, c)			SCAT2(a, SCAT2(b, c))
#define SCAT4(a, b, c, d)		SCAT2(a, SCAT3(b, c, d))

#define _CHECK(_test)			_CHECK_##_test
#define _PREPARE(_test)			_PREPARE_##_test
#define _PREPARE_guest_read64		NULL
#define _PREPARE_guest_ld_preidx	NULL
#define _PREPARE_guest_write64		NULL
#define _PREPARE_guest_st_preidx	NULL
#define _PREPARE_guest_exec		NULL
#define _PREPARE_guest_at		NULL
#define _PREPARE_guest_dc_zva		guest_check_dc_zva
#define _PREPARE_guest_cas		guest_check_lse

 
#define _PREPARE_with_af		guest_set_ha, guest_clear_pte_af
#define _PREPARE_no_af			NULL
#define _CHECK_with_af			guest_check_pte_af
#define _CHECK_no_af			NULL

 
#define TEST_ACCESS(_access, _with_af, _mark_cmd)				\
{										\
	.name			= SCAT3(_access, _with_af, #_mark_cmd),		\
	.guest_prepare		= { _PREPARE(_with_af),				\
				    _PREPARE(_access) },			\
	.mem_mark_cmd		= _mark_cmd,					\
	.guest_test		= _access,					\
	.guest_test_check	= { _CHECK(_with_af) },				\
	.expected_events	= { 0 },					\
}

#define TEST_UFFD(_access, _with_af, _mark_cmd,					\
		  _uffd_data_handler, _uffd_pt_handler, _uffd_faults)		\
{										\
	.name			= SCAT4(uffd, _access, _with_af, #_mark_cmd),	\
	.guest_prepare		= { _PREPARE(_with_af),				\
				    _PREPARE(_access) },			\
	.guest_test		= _access,					\
	.mem_mark_cmd		= _mark_cmd,					\
	.guest_test_check	= { _CHECK(_with_af) },				\
	.uffd_data_handler	= _uffd_data_handler,				\
	.uffd_pt_handler	= _uffd_pt_handler,				\
	.expected_events	= { .uffd_faults = _uffd_faults, },		\
}

#define TEST_DIRTY_LOG(_access, _with_af, _test_check, _pt_check)		\
{										\
	.name			= SCAT3(dirty_log, _access, _with_af),		\
	.data_memslot_flags	= KVM_MEM_LOG_DIRTY_PAGES,			\
	.pt_memslot_flags	= KVM_MEM_LOG_DIRTY_PAGES,			\
	.guest_prepare		= { _PREPARE(_with_af),				\
				    _PREPARE(_access) },			\
	.guest_test		= _access,					\
	.guest_test_check	= { _CHECK(_with_af), _test_check, _pt_check },	\
	.expected_events	= { 0 },					\
}

#define TEST_UFFD_AND_DIRTY_LOG(_access, _with_af, _uffd_data_handler,		\
				_uffd_faults, _test_check, _pt_check)		\
{										\
	.name			= SCAT3(uffd_and_dirty_log, _access, _with_af),	\
	.data_memslot_flags	= KVM_MEM_LOG_DIRTY_PAGES,			\
	.pt_memslot_flags	= KVM_MEM_LOG_DIRTY_PAGES,			\
	.guest_prepare		= { _PREPARE(_with_af),				\
				    _PREPARE(_access) },			\
	.guest_test		= _access,					\
	.mem_mark_cmd		= CMD_HOLE_DATA | CMD_HOLE_PT,			\
	.guest_test_check	= { _CHECK(_with_af), _test_check, _pt_check },	\
	.uffd_data_handler	= _uffd_data_handler,				\
	.uffd_pt_handler	= uffd_pt_handler,				\
	.expected_events	= { .uffd_faults = _uffd_faults, },		\
}

#define TEST_RO_MEMSLOT(_access, _mmio_handler, _mmio_exits)			\
{										\
	.name			= SCAT2(ro_memslot, _access),			\
	.data_memslot_flags	= KVM_MEM_READONLY,				\
	.pt_memslot_flags	= KVM_MEM_READONLY,				\
	.guest_prepare		= { _PREPARE(_access) },			\
	.guest_test		= _access,					\
	.mmio_handler		= _mmio_handler,				\
	.expected_events	= { .mmio_exits = _mmio_exits },		\
}

#define TEST_RO_MEMSLOT_NO_SYNDROME(_access)					\
{										\
	.name			= SCAT2(ro_memslot_no_syndrome, _access),	\
	.data_memslot_flags	= KVM_MEM_READONLY,				\
	.pt_memslot_flags	= KVM_MEM_READONLY,				\
	.guest_test		= _access,					\
	.fail_vcpu_run_handler	= fail_vcpu_run_mmio_no_syndrome_handler,	\
	.expected_events	= { .fail_vcpu_runs = 1 },			\
}

#define TEST_RO_MEMSLOT_AND_DIRTY_LOG(_access, _mmio_handler, _mmio_exits,	\
				      _test_check)				\
{										\
	.name			= SCAT2(ro_memslot, _access),			\
	.data_memslot_flags	= KVM_MEM_READONLY | KVM_MEM_LOG_DIRTY_PAGES,	\
	.pt_memslot_flags	= KVM_MEM_READONLY | KVM_MEM_LOG_DIRTY_PAGES,	\
	.guest_prepare		= { _PREPARE(_access) },			\
	.guest_test		= _access,					\
	.guest_test_check	= { _test_check },				\
	.mmio_handler		= _mmio_handler,				\
	.expected_events	= { .mmio_exits = _mmio_exits},			\
}

#define TEST_RO_MEMSLOT_NO_SYNDROME_AND_DIRTY_LOG(_access, _test_check)		\
{										\
	.name			= SCAT2(ro_memslot_no_syn_and_dlog, _access),	\
	.data_memslot_flags	= KVM_MEM_READONLY | KVM_MEM_LOG_DIRTY_PAGES,	\
	.pt_memslot_flags	= KVM_MEM_READONLY | KVM_MEM_LOG_DIRTY_PAGES,	\
	.guest_test		= _access,					\
	.guest_test_check	= { _test_check },				\
	.fail_vcpu_run_handler	= fail_vcpu_run_mmio_no_syndrome_handler,	\
	.expected_events	= { .fail_vcpu_runs = 1 },			\
}

#define TEST_RO_MEMSLOT_AND_UFFD(_access, _mmio_handler, _mmio_exits,		\
				 _uffd_data_handler, _uffd_faults)		\
{										\
	.name			= SCAT2(ro_memslot_uffd, _access),		\
	.data_memslot_flags	= KVM_MEM_READONLY,				\
	.pt_memslot_flags	= KVM_MEM_READONLY,				\
	.mem_mark_cmd		= CMD_HOLE_DATA | CMD_HOLE_PT,			\
	.guest_prepare		= { _PREPARE(_access) },			\
	.guest_test		= _access,					\
	.uffd_data_handler	= _uffd_data_handler,				\
	.uffd_pt_handler	= uffd_pt_handler,				\
	.mmio_handler		= _mmio_handler,				\
	.expected_events	= { .mmio_exits = _mmio_exits,			\
				    .uffd_faults = _uffd_faults },		\
}

#define TEST_RO_MEMSLOT_NO_SYNDROME_AND_UFFD(_access, _uffd_data_handler,	\
					     _uffd_faults)			\
{										\
	.name			= SCAT2(ro_memslot_no_syndrome, _access),	\
	.data_memslot_flags	= KVM_MEM_READONLY,				\
	.pt_memslot_flags	= KVM_MEM_READONLY,				\
	.mem_mark_cmd		= CMD_HOLE_DATA | CMD_HOLE_PT,			\
	.guest_test		= _access,					\
	.uffd_data_handler	= _uffd_data_handler,				\
	.uffd_pt_handler	= uffd_pt_handler,			\
	.fail_vcpu_run_handler	= fail_vcpu_run_mmio_no_syndrome_handler,	\
	.expected_events	= { .fail_vcpu_runs = 1,			\
				    .uffd_faults = _uffd_faults },		\
}

static struct test_desc tests[] = {

	 
	TEST_ACCESS(guest_read64, with_af, CMD_NONE),
	TEST_ACCESS(guest_ld_preidx, with_af, CMD_NONE),
	TEST_ACCESS(guest_cas, with_af, CMD_NONE),
	TEST_ACCESS(guest_write64, with_af, CMD_NONE),
	TEST_ACCESS(guest_st_preidx, with_af, CMD_NONE),
	TEST_ACCESS(guest_dc_zva, with_af, CMD_NONE),
	TEST_ACCESS(guest_exec, with_af, CMD_NONE),

	 
	TEST_ACCESS(guest_read64, no_af, CMD_HOLE_DATA),
	TEST_ACCESS(guest_cas, no_af, CMD_HOLE_DATA),
	TEST_ACCESS(guest_ld_preidx, no_af, CMD_HOLE_DATA),
	TEST_ACCESS(guest_write64, no_af, CMD_HOLE_DATA),
	TEST_ACCESS(guest_st_preidx, no_af, CMD_HOLE_DATA),
	TEST_ACCESS(guest_at, no_af, CMD_HOLE_DATA),
	TEST_ACCESS(guest_dc_zva, no_af, CMD_HOLE_DATA),

	 
	TEST_UFFD(guest_read64, with_af, CMD_HOLE_DATA | CMD_HOLE_PT,
		  uffd_data_handler, uffd_pt_handler, 2),
	TEST_UFFD(guest_read64, no_af, CMD_HOLE_DATA | CMD_HOLE_PT,
		  uffd_data_handler, uffd_pt_handler, 2),
	TEST_UFFD(guest_cas, with_af, CMD_HOLE_DATA | CMD_HOLE_PT,
		  uffd_data_handler, uffd_pt_handler, 2),
	 
	TEST_UFFD(guest_at, no_af, CMD_HOLE_DATA | CMD_HOLE_PT,
		  uffd_no_handler, uffd_pt_handler, 1),
	TEST_UFFD(guest_ld_preidx, with_af, CMD_HOLE_DATA | CMD_HOLE_PT,
		  uffd_data_handler, uffd_pt_handler, 2),
	TEST_UFFD(guest_write64, with_af, CMD_HOLE_DATA | CMD_HOLE_PT,
		  uffd_data_handler, uffd_pt_handler, 2),
	TEST_UFFD(guest_dc_zva, with_af, CMD_HOLE_DATA | CMD_HOLE_PT,
		  uffd_data_handler, uffd_pt_handler, 2),
	TEST_UFFD(guest_st_preidx, with_af, CMD_HOLE_DATA | CMD_HOLE_PT,
		  uffd_data_handler, uffd_pt_handler, 2),
	TEST_UFFD(guest_exec, with_af, CMD_HOLE_DATA | CMD_HOLE_PT,
		  uffd_data_handler, uffd_pt_handler, 2),

	 
	TEST_DIRTY_LOG(guest_read64, with_af, guest_check_no_write_in_dirty_log,
		       guest_check_s1ptw_wr_in_dirty_log),
	TEST_DIRTY_LOG(guest_read64, no_af, guest_check_no_write_in_dirty_log,
		       guest_check_no_s1ptw_wr_in_dirty_log),
	TEST_DIRTY_LOG(guest_ld_preidx, with_af,
		       guest_check_no_write_in_dirty_log,
		       guest_check_s1ptw_wr_in_dirty_log),
	TEST_DIRTY_LOG(guest_at, no_af, guest_check_no_write_in_dirty_log,
		       guest_check_no_s1ptw_wr_in_dirty_log),
	TEST_DIRTY_LOG(guest_exec, with_af, guest_check_no_write_in_dirty_log,
		       guest_check_s1ptw_wr_in_dirty_log),
	TEST_DIRTY_LOG(guest_write64, with_af, guest_check_write_in_dirty_log,
		       guest_check_s1ptw_wr_in_dirty_log),
	TEST_DIRTY_LOG(guest_cas, with_af, guest_check_write_in_dirty_log,
		       guest_check_s1ptw_wr_in_dirty_log),
	TEST_DIRTY_LOG(guest_dc_zva, with_af, guest_check_write_in_dirty_log,
		       guest_check_s1ptw_wr_in_dirty_log),
	TEST_DIRTY_LOG(guest_st_preidx, with_af, guest_check_write_in_dirty_log,
		       guest_check_s1ptw_wr_in_dirty_log),

	 
	TEST_UFFD_AND_DIRTY_LOG(guest_read64, with_af,
				uffd_data_handler, 2,
				guest_check_no_write_in_dirty_log,
				guest_check_s1ptw_wr_in_dirty_log),
	TEST_UFFD_AND_DIRTY_LOG(guest_read64, no_af,
				uffd_data_handler, 2,
				guest_check_no_write_in_dirty_log,
				guest_check_no_s1ptw_wr_in_dirty_log),
	TEST_UFFD_AND_DIRTY_LOG(guest_ld_preidx, with_af,
				uffd_data_handler,
				2, guest_check_no_write_in_dirty_log,
				guest_check_s1ptw_wr_in_dirty_log),
	TEST_UFFD_AND_DIRTY_LOG(guest_at, with_af, uffd_no_handler, 1,
				guest_check_no_write_in_dirty_log,
				guest_check_s1ptw_wr_in_dirty_log),
	TEST_UFFD_AND_DIRTY_LOG(guest_exec, with_af,
				uffd_data_handler, 2,
				guest_check_no_write_in_dirty_log,
				guest_check_s1ptw_wr_in_dirty_log),
	TEST_UFFD_AND_DIRTY_LOG(guest_write64, with_af,
				uffd_data_handler,
				2, guest_check_write_in_dirty_log,
				guest_check_s1ptw_wr_in_dirty_log),
	TEST_UFFD_AND_DIRTY_LOG(guest_cas, with_af,
				uffd_data_handler, 2,
				guest_check_write_in_dirty_log,
				guest_check_s1ptw_wr_in_dirty_log),
	TEST_UFFD_AND_DIRTY_LOG(guest_dc_zva, with_af,
				uffd_data_handler,
				2, guest_check_write_in_dirty_log,
				guest_check_s1ptw_wr_in_dirty_log),
	TEST_UFFD_AND_DIRTY_LOG(guest_st_preidx, with_af,
				uffd_data_handler, 2,
				guest_check_write_in_dirty_log,
				guest_check_s1ptw_wr_in_dirty_log),
	 
	TEST_RO_MEMSLOT(guest_read64, 0, 0),
	TEST_RO_MEMSLOT(guest_ld_preidx, 0, 0),
	TEST_RO_MEMSLOT(guest_at, 0, 0),
	TEST_RO_MEMSLOT(guest_exec, 0, 0),
	TEST_RO_MEMSLOT(guest_write64, mmio_on_test_gpa_handler, 1),
	TEST_RO_MEMSLOT_NO_SYNDROME(guest_dc_zva),
	TEST_RO_MEMSLOT_NO_SYNDROME(guest_cas),
	TEST_RO_MEMSLOT_NO_SYNDROME(guest_st_preidx),

	 
	TEST_RO_MEMSLOT_AND_DIRTY_LOG(guest_read64, 0, 0,
				      guest_check_no_write_in_dirty_log),
	TEST_RO_MEMSLOT_AND_DIRTY_LOG(guest_ld_preidx, 0, 0,
				      guest_check_no_write_in_dirty_log),
	TEST_RO_MEMSLOT_AND_DIRTY_LOG(guest_at, 0, 0,
				      guest_check_no_write_in_dirty_log),
	TEST_RO_MEMSLOT_AND_DIRTY_LOG(guest_exec, 0, 0,
				      guest_check_no_write_in_dirty_log),
	TEST_RO_MEMSLOT_AND_DIRTY_LOG(guest_write64, mmio_on_test_gpa_handler,
				      1, guest_check_no_write_in_dirty_log),
	TEST_RO_MEMSLOT_NO_SYNDROME_AND_DIRTY_LOG(guest_dc_zva,
						  guest_check_no_write_in_dirty_log),
	TEST_RO_MEMSLOT_NO_SYNDROME_AND_DIRTY_LOG(guest_cas,
						  guest_check_no_write_in_dirty_log),
	TEST_RO_MEMSLOT_NO_SYNDROME_AND_DIRTY_LOG(guest_st_preidx,
						  guest_check_no_write_in_dirty_log),

	 
	TEST_RO_MEMSLOT_AND_UFFD(guest_read64, 0, 0, uffd_data_handler, 2),
	TEST_RO_MEMSLOT_AND_UFFD(guest_ld_preidx, 0, 0, uffd_data_handler, 2),
	TEST_RO_MEMSLOT_AND_UFFD(guest_at, 0, 0, uffd_no_handler, 1),
	TEST_RO_MEMSLOT_AND_UFFD(guest_exec, 0, 0, uffd_data_handler, 2),
	TEST_RO_MEMSLOT_AND_UFFD(guest_write64, mmio_on_test_gpa_handler, 1,
				 uffd_data_handler, 2),
	TEST_RO_MEMSLOT_NO_SYNDROME_AND_UFFD(guest_cas, uffd_data_handler, 2),
	TEST_RO_MEMSLOT_NO_SYNDROME_AND_UFFD(guest_dc_zva, uffd_no_handler, 1),
	TEST_RO_MEMSLOT_NO_SYNDROME_AND_UFFD(guest_st_preidx, uffd_no_handler, 1),

	{ 0 }
};

static void for_each_test_and_guest_mode(enum vm_mem_backing_src_type src_type)
{
	struct test_desc *t;

	for (t = &tests[0]; t->name; t++) {
		if (t->skip)
			continue;

		struct test_params p = {
			.src_type = src_type,
			.test_desc = t,
		};

		for_each_guest_mode(run_test, &p);
	}
}

int main(int argc, char *argv[])
{
	enum vm_mem_backing_src_type src_type;
	int opt;

	src_type = DEFAULT_VM_MEM_SRC;

	while ((opt = getopt(argc, argv, "hm:s:")) != -1) {
		switch (opt) {
		case 'm':
			guest_modes_cmdline(optarg);
			break;
		case 's':
			src_type = parse_backing_src_type(optarg);
			break;
		case 'h':
		default:
			help(argv[0]);
			exit(0);
		}
	}

	for_each_test_and_guest_mode(src_type);
	return 0;
}
