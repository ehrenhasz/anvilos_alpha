
 

#define _GNU_SOURCE  
#include <asm/barrier.h>
#include <pthread.h>
#include <inttypes.h>

#include "kvm_util.h"
#include "processor.h"
#include "hyperv.h"
#include "test_util.h"
#include "vmx.h"

#define WORKER_VCPU_ID_1 2
#define WORKER_VCPU_ID_2 65

#define NTRY 100
#define NTEST_PAGES 2

struct hv_vpset {
	u64 format;
	u64 valid_bank_mask;
	u64 bank_contents[];
};

enum HV_GENERIC_SET_FORMAT {
	HV_GENERIC_SET_SPARSE_4K,
	HV_GENERIC_SET_ALL,
};

#define HV_FLUSH_ALL_PROCESSORS			BIT(0)
#define HV_FLUSH_ALL_VIRTUAL_ADDRESS_SPACES	BIT(1)
#define HV_FLUSH_NON_GLOBAL_MAPPINGS_ONLY	BIT(2)
#define HV_FLUSH_USE_EXTENDED_RANGE_FORMAT	BIT(3)

 
struct hv_tlb_flush {
	u64 address_space;
	u64 flags;
	u64 processor_mask;
	u64 gva_list[];
} __packed;

 
struct hv_tlb_flush_ex {
	u64 address_space;
	u64 flags;
	struct hv_vpset hv_vp_set;
	u64 gva_list[];
} __packed;

 
struct test_data {
	vm_vaddr_t hcall_gva;
	vm_paddr_t hcall_gpa;
	vm_vaddr_t test_pages;
	vm_vaddr_t test_pages_pte[NTEST_PAGES];
};

 
static void worker_guest_code(vm_vaddr_t test_data)
{
	struct test_data *data = (struct test_data *)test_data;
	u32 vcpu_id = rdmsr(HV_X64_MSR_VP_INDEX);
	void *exp_page = (void *)data->test_pages + PAGE_SIZE * NTEST_PAGES;
	u64 *this_cpu = (u64 *)(exp_page + vcpu_id * sizeof(u64));
	u64 expected, val;

	x2apic_enable();
	wrmsr(HV_X64_MSR_GUEST_OS_ID, HYPERV_LINUX_OS_ID);

	for (;;) {
		cpu_relax();

		expected = READ_ONCE(*this_cpu);

		 
		rmb();

		val = READ_ONCE(*(u64 *)data->test_pages);

		 
		rmb();

		 
		if (!expected)
			continue;

		 
		if (expected != READ_ONCE(*this_cpu))
			continue;

		GUEST_ASSERT(val == expected);
	}
}

 
static void set_expected_val(void *addr, u64 val, int vcpu_id)
{
	void *exp_page = addr + PAGE_SIZE * NTEST_PAGES;

	*(u64 *)(exp_page + vcpu_id * sizeof(u64)) = val;
}

 
static void swap_two_test_pages(vm_paddr_t pte_gva1, vm_paddr_t pte_gva2)
{
	uint64_t tmp = *(uint64_t *)pte_gva1;

	*(uint64_t *)pte_gva1 = *(uint64_t *)pte_gva2;
	*(uint64_t *)pte_gva2 = tmp;
}

 
static inline void do_delay(void)
{
	int i;

	for (i = 0; i < 1000000; i++)
		asm volatile("nop");
}

 
static inline void prepare_to_test(struct test_data *data)
{
	 
	memset((void *)data->hcall_gva, 0, PAGE_SIZE);

	 
	set_expected_val((void *)data->test_pages, 0x0, WORKER_VCPU_ID_1);
	set_expected_val((void *)data->test_pages, 0x0, WORKER_VCPU_ID_2);

	 
	wmb();

	 
	do_delay();

	 
	swap_two_test_pages(data->test_pages_pte[0], data->test_pages_pte[1]);
}

 
static inline void post_test(struct test_data *data, u64 exp1, u64 exp2)
{
	 
	wmb();

	 
	set_expected_val((void *)data->test_pages, exp1, WORKER_VCPU_ID_1);
	set_expected_val((void *)data->test_pages, exp2, WORKER_VCPU_ID_2);

	 
	do_delay();
}

#define TESTVAL1 0x0101010101010101
#define TESTVAL2 0x0202020202020202

 
static void sender_guest_code(vm_vaddr_t test_data)
{
	struct test_data *data = (struct test_data *)test_data;
	struct hv_tlb_flush *flush = (struct hv_tlb_flush *)data->hcall_gva;
	struct hv_tlb_flush_ex *flush_ex = (struct hv_tlb_flush_ex *)data->hcall_gva;
	vm_paddr_t hcall_gpa = data->hcall_gpa;
	int i, stage = 1;

	wrmsr(HV_X64_MSR_GUEST_OS_ID, HYPERV_LINUX_OS_ID);
	wrmsr(HV_X64_MSR_HYPERCALL, data->hcall_gpa);

	 

	GUEST_SYNC(stage++);

	 
	for (i = 0; i < NTRY; i++) {
		prepare_to_test(data);
		flush->flags = HV_FLUSH_ALL_VIRTUAL_ADDRESS_SPACES;
		flush->processor_mask = BIT(WORKER_VCPU_ID_1);
		hyperv_hypercall(HVCALL_FLUSH_VIRTUAL_ADDRESS_SPACE, hcall_gpa,
				 hcall_gpa + PAGE_SIZE);
		post_test(data, i % 2 ? TESTVAL1 : TESTVAL2, 0x0);
	}

	GUEST_SYNC(stage++);

	 
	for (i = 0; i < NTRY; i++) {
		prepare_to_test(data);
		flush->flags = HV_FLUSH_ALL_VIRTUAL_ADDRESS_SPACES;
		flush->processor_mask = BIT(WORKER_VCPU_ID_1);
		flush->gva_list[0] = (u64)data->test_pages;
		hyperv_hypercall(HVCALL_FLUSH_VIRTUAL_ADDRESS_LIST |
				 (1UL << HV_HYPERCALL_REP_COMP_OFFSET),
				 hcall_gpa, hcall_gpa + PAGE_SIZE);
		post_test(data, i % 2 ? TESTVAL1 : TESTVAL2, 0x0);
	}

	GUEST_SYNC(stage++);

	 
	for (i = 0; i < NTRY; i++) {
		prepare_to_test(data);
		flush->flags = HV_FLUSH_ALL_VIRTUAL_ADDRESS_SPACES |
			HV_FLUSH_ALL_PROCESSORS;
		flush->processor_mask = 0;
		hyperv_hypercall(HVCALL_FLUSH_VIRTUAL_ADDRESS_SPACE, hcall_gpa,
				 hcall_gpa + PAGE_SIZE);
		post_test(data, i % 2 ? TESTVAL1 : TESTVAL2, i % 2 ? TESTVAL1 : TESTVAL2);
	}

	GUEST_SYNC(stage++);

	 
	for (i = 0; i < NTRY; i++) {
		prepare_to_test(data);
		flush->flags = HV_FLUSH_ALL_VIRTUAL_ADDRESS_SPACES |
			HV_FLUSH_ALL_PROCESSORS;
		flush->gva_list[0] = (u64)data->test_pages;
		hyperv_hypercall(HVCALL_FLUSH_VIRTUAL_ADDRESS_LIST |
				 (1UL << HV_HYPERCALL_REP_COMP_OFFSET),
				 hcall_gpa, hcall_gpa + PAGE_SIZE);
		post_test(data, i % 2 ? TESTVAL1 : TESTVAL2,
			  i % 2 ? TESTVAL1 : TESTVAL2);
	}

	GUEST_SYNC(stage++);

	 
	for (i = 0; i < NTRY; i++) {
		prepare_to_test(data);
		flush_ex->flags = HV_FLUSH_ALL_VIRTUAL_ADDRESS_SPACES;
		flush_ex->hv_vp_set.format = HV_GENERIC_SET_SPARSE_4K;
		flush_ex->hv_vp_set.valid_bank_mask = BIT_ULL(WORKER_VCPU_ID_2 / 64);
		flush_ex->hv_vp_set.bank_contents[0] = BIT_ULL(WORKER_VCPU_ID_2 % 64);
		hyperv_hypercall(HVCALL_FLUSH_VIRTUAL_ADDRESS_SPACE_EX |
				 (1 << HV_HYPERCALL_VARHEAD_OFFSET),
				 hcall_gpa, hcall_gpa + PAGE_SIZE);
		post_test(data, 0x0, i % 2 ? TESTVAL1 : TESTVAL2);
	}

	GUEST_SYNC(stage++);

	 
	for (i = 0; i < NTRY; i++) {
		prepare_to_test(data);
		flush_ex->flags = HV_FLUSH_ALL_VIRTUAL_ADDRESS_SPACES;
		flush_ex->hv_vp_set.format = HV_GENERIC_SET_SPARSE_4K;
		flush_ex->hv_vp_set.valid_bank_mask = BIT_ULL(WORKER_VCPU_ID_2 / 64);
		flush_ex->hv_vp_set.bank_contents[0] = BIT_ULL(WORKER_VCPU_ID_2 % 64);
		 
		flush_ex->gva_list[1] = (u64)data->test_pages;
		hyperv_hypercall(HVCALL_FLUSH_VIRTUAL_ADDRESS_LIST_EX |
				 (1 << HV_HYPERCALL_VARHEAD_OFFSET) |
				 (1UL << HV_HYPERCALL_REP_COMP_OFFSET),
				 hcall_gpa, hcall_gpa + PAGE_SIZE);
		post_test(data, 0x0, i % 2 ? TESTVAL1 : TESTVAL2);
	}

	GUEST_SYNC(stage++);

	 
	for (i = 0; i < NTRY; i++) {
		prepare_to_test(data);
		flush_ex->flags = HV_FLUSH_ALL_VIRTUAL_ADDRESS_SPACES;
		flush_ex->hv_vp_set.format = HV_GENERIC_SET_SPARSE_4K;
		flush_ex->hv_vp_set.valid_bank_mask = BIT_ULL(WORKER_VCPU_ID_2 / 64) |
			BIT_ULL(WORKER_VCPU_ID_1 / 64);
		flush_ex->hv_vp_set.bank_contents[0] = BIT_ULL(WORKER_VCPU_ID_1 % 64);
		flush_ex->hv_vp_set.bank_contents[1] = BIT_ULL(WORKER_VCPU_ID_2 % 64);
		hyperv_hypercall(HVCALL_FLUSH_VIRTUAL_ADDRESS_SPACE_EX |
				 (2 << HV_HYPERCALL_VARHEAD_OFFSET),
				 hcall_gpa, hcall_gpa + PAGE_SIZE);
		post_test(data, i % 2 ? TESTVAL1 : TESTVAL2,
			  i % 2 ? TESTVAL1 : TESTVAL2);
	}

	GUEST_SYNC(stage++);

	 
	for (i = 0; i < NTRY; i++) {
		prepare_to_test(data);
		flush_ex->flags = HV_FLUSH_ALL_VIRTUAL_ADDRESS_SPACES;
		flush_ex->hv_vp_set.format = HV_GENERIC_SET_SPARSE_4K;
		flush_ex->hv_vp_set.valid_bank_mask = BIT_ULL(WORKER_VCPU_ID_1 / 64) |
			BIT_ULL(WORKER_VCPU_ID_2 / 64);
		flush_ex->hv_vp_set.bank_contents[0] = BIT_ULL(WORKER_VCPU_ID_1 % 64);
		flush_ex->hv_vp_set.bank_contents[1] = BIT_ULL(WORKER_VCPU_ID_2 % 64);
		 
		flush_ex->gva_list[2] = (u64)data->test_pages;
		hyperv_hypercall(HVCALL_FLUSH_VIRTUAL_ADDRESS_LIST_EX |
				 (2 << HV_HYPERCALL_VARHEAD_OFFSET) |
				 (1UL << HV_HYPERCALL_REP_COMP_OFFSET),
				 hcall_gpa, hcall_gpa + PAGE_SIZE);
		post_test(data, i % 2 ? TESTVAL1 : TESTVAL2,
			  i % 2 ? TESTVAL1 : TESTVAL2);
	}

	GUEST_SYNC(stage++);

	 
	for (i = 0; i < NTRY; i++) {
		prepare_to_test(data);
		flush_ex->flags = HV_FLUSH_ALL_VIRTUAL_ADDRESS_SPACES;
		flush_ex->hv_vp_set.format = HV_GENERIC_SET_ALL;
		hyperv_hypercall(HVCALL_FLUSH_VIRTUAL_ADDRESS_SPACE_EX,
				 hcall_gpa, hcall_gpa + PAGE_SIZE);
		post_test(data, i % 2 ? TESTVAL1 : TESTVAL2,
			  i % 2 ? TESTVAL1 : TESTVAL2);
	}

	GUEST_SYNC(stage++);

	 
	for (i = 0; i < NTRY; i++) {
		prepare_to_test(data);
		flush_ex->flags = HV_FLUSH_ALL_VIRTUAL_ADDRESS_SPACES;
		flush_ex->hv_vp_set.format = HV_GENERIC_SET_ALL;
		flush_ex->gva_list[0] = (u64)data->test_pages;
		hyperv_hypercall(HVCALL_FLUSH_VIRTUAL_ADDRESS_LIST_EX |
				 (1UL << HV_HYPERCALL_REP_COMP_OFFSET),
				 hcall_gpa, hcall_gpa + PAGE_SIZE);
		post_test(data, i % 2 ? TESTVAL1 : TESTVAL2,
			  i % 2 ? TESTVAL1 : TESTVAL2);
	}

	 

	GUEST_SYNC(stage++);

	 
	for (i = 0; i < NTRY; i++) {
		prepare_to_test(data);
		flush->processor_mask = BIT(WORKER_VCPU_ID_1);
		hyperv_write_xmm_input(&flush->processor_mask, 1);
		hyperv_hypercall(HVCALL_FLUSH_VIRTUAL_ADDRESS_SPACE |
				 HV_HYPERCALL_FAST_BIT, 0x0,
				 HV_FLUSH_ALL_VIRTUAL_ADDRESS_SPACES);
		post_test(data, i % 2 ? TESTVAL1 : TESTVAL2, 0x0);
	}

	GUEST_SYNC(stage++);

	 
	for (i = 0; i < NTRY; i++) {
		prepare_to_test(data);
		flush->processor_mask = BIT(WORKER_VCPU_ID_1);
		flush->gva_list[0] = (u64)data->test_pages;
		hyperv_write_xmm_input(&flush->processor_mask, 1);
		hyperv_hypercall(HVCALL_FLUSH_VIRTUAL_ADDRESS_LIST |
				 HV_HYPERCALL_FAST_BIT |
				 (1UL << HV_HYPERCALL_REP_COMP_OFFSET),
				 0x0, HV_FLUSH_ALL_VIRTUAL_ADDRESS_SPACES);
		post_test(data, i % 2 ? TESTVAL1 : TESTVAL2, 0x0);
	}

	GUEST_SYNC(stage++);

	 
	for (i = 0; i < NTRY; i++) {
		prepare_to_test(data);
		hyperv_write_xmm_input(&flush->processor_mask, 1);
		hyperv_hypercall(HVCALL_FLUSH_VIRTUAL_ADDRESS_SPACE |
				 HV_HYPERCALL_FAST_BIT, 0x0,
				 HV_FLUSH_ALL_VIRTUAL_ADDRESS_SPACES |
				 HV_FLUSH_ALL_PROCESSORS);
		post_test(data, i % 2 ? TESTVAL1 : TESTVAL2,
			  i % 2 ? TESTVAL1 : TESTVAL2);
	}

	GUEST_SYNC(stage++);

	 
	for (i = 0; i < NTRY; i++) {
		prepare_to_test(data);
		flush->gva_list[0] = (u64)data->test_pages;
		hyperv_write_xmm_input(&flush->processor_mask, 1);
		hyperv_hypercall(HVCALL_FLUSH_VIRTUAL_ADDRESS_LIST |
				 HV_HYPERCALL_FAST_BIT |
				 (1UL << HV_HYPERCALL_REP_COMP_OFFSET), 0x0,
				 HV_FLUSH_ALL_VIRTUAL_ADDRESS_SPACES |
				 HV_FLUSH_ALL_PROCESSORS);
		post_test(data, i % 2 ? TESTVAL1 : TESTVAL2,
			  i % 2 ? TESTVAL1 : TESTVAL2);
	}

	GUEST_SYNC(stage++);

	 
	for (i = 0; i < NTRY; i++) {
		prepare_to_test(data);
		flush_ex->hv_vp_set.format = HV_GENERIC_SET_SPARSE_4K;
		flush_ex->hv_vp_set.valid_bank_mask = BIT_ULL(WORKER_VCPU_ID_2 / 64);
		flush_ex->hv_vp_set.bank_contents[0] = BIT_ULL(WORKER_VCPU_ID_2 % 64);
		hyperv_write_xmm_input(&flush_ex->hv_vp_set, 2);
		hyperv_hypercall(HVCALL_FLUSH_VIRTUAL_ADDRESS_SPACE_EX |
				 HV_HYPERCALL_FAST_BIT |
				 (1 << HV_HYPERCALL_VARHEAD_OFFSET),
				 0x0, HV_FLUSH_ALL_VIRTUAL_ADDRESS_SPACES);
		post_test(data, 0x0, i % 2 ? TESTVAL1 : TESTVAL2);
	}

	GUEST_SYNC(stage++);

	 
	for (i = 0; i < NTRY; i++) {
		prepare_to_test(data);
		flush_ex->hv_vp_set.format = HV_GENERIC_SET_SPARSE_4K;
		flush_ex->hv_vp_set.valid_bank_mask = BIT_ULL(WORKER_VCPU_ID_2 / 64);
		flush_ex->hv_vp_set.bank_contents[0] = BIT_ULL(WORKER_VCPU_ID_2 % 64);
		 
		flush_ex->gva_list[1] = (u64)data->test_pages;
		hyperv_write_xmm_input(&flush_ex->hv_vp_set, 2);
		hyperv_hypercall(HVCALL_FLUSH_VIRTUAL_ADDRESS_LIST_EX |
				 HV_HYPERCALL_FAST_BIT |
				 (1 << HV_HYPERCALL_VARHEAD_OFFSET) |
				 (1UL << HV_HYPERCALL_REP_COMP_OFFSET),
				 0x0, HV_FLUSH_ALL_VIRTUAL_ADDRESS_SPACES);
		post_test(data, 0x0, i % 2 ? TESTVAL1 : TESTVAL2);
	}

	GUEST_SYNC(stage++);

	 
	for (i = 0; i < NTRY; i++) {
		prepare_to_test(data);
		flush_ex->hv_vp_set.format = HV_GENERIC_SET_SPARSE_4K;
		flush_ex->hv_vp_set.valid_bank_mask = BIT_ULL(WORKER_VCPU_ID_2 / 64) |
			BIT_ULL(WORKER_VCPU_ID_1 / 64);
		flush_ex->hv_vp_set.bank_contents[0] = BIT_ULL(WORKER_VCPU_ID_1 % 64);
		flush_ex->hv_vp_set.bank_contents[1] = BIT_ULL(WORKER_VCPU_ID_2 % 64);
		hyperv_write_xmm_input(&flush_ex->hv_vp_set, 2);
		hyperv_hypercall(HVCALL_FLUSH_VIRTUAL_ADDRESS_SPACE_EX |
				 HV_HYPERCALL_FAST_BIT |
				 (2 << HV_HYPERCALL_VARHEAD_OFFSET),
				 0x0, HV_FLUSH_ALL_VIRTUAL_ADDRESS_SPACES);
		post_test(data, i % 2 ? TESTVAL1 :
			  TESTVAL2, i % 2 ? TESTVAL1 : TESTVAL2);
	}

	GUEST_SYNC(stage++);

	 
	for (i = 0; i < NTRY; i++) {
		prepare_to_test(data);
		flush_ex->hv_vp_set.format = HV_GENERIC_SET_SPARSE_4K;
		flush_ex->hv_vp_set.valid_bank_mask = BIT_ULL(WORKER_VCPU_ID_1 / 64) |
			BIT_ULL(WORKER_VCPU_ID_2 / 64);
		flush_ex->hv_vp_set.bank_contents[0] = BIT_ULL(WORKER_VCPU_ID_1 % 64);
		flush_ex->hv_vp_set.bank_contents[1] = BIT_ULL(WORKER_VCPU_ID_2 % 64);
		 
		flush_ex->gva_list[2] = (u64)data->test_pages;
		hyperv_write_xmm_input(&flush_ex->hv_vp_set, 3);
		hyperv_hypercall(HVCALL_FLUSH_VIRTUAL_ADDRESS_LIST_EX |
				 HV_HYPERCALL_FAST_BIT |
				 (2 << HV_HYPERCALL_VARHEAD_OFFSET) |
				 (1UL << HV_HYPERCALL_REP_COMP_OFFSET),
				 0x0, HV_FLUSH_ALL_VIRTUAL_ADDRESS_SPACES);
		post_test(data, i % 2 ? TESTVAL1 : TESTVAL2,
			  i % 2 ? TESTVAL1 : TESTVAL2);
	}

	GUEST_SYNC(stage++);

	 
	for (i = 0; i < NTRY; i++) {
		prepare_to_test(data);
		flush_ex->flags = HV_FLUSH_ALL_VIRTUAL_ADDRESS_SPACES;
		flush_ex->hv_vp_set.format = HV_GENERIC_SET_ALL;
		hyperv_write_xmm_input(&flush_ex->hv_vp_set, 2);
		hyperv_hypercall(HVCALL_FLUSH_VIRTUAL_ADDRESS_SPACE_EX |
				 HV_HYPERCALL_FAST_BIT,
				 0x0, HV_FLUSH_ALL_VIRTUAL_ADDRESS_SPACES);
		post_test(data, i % 2 ? TESTVAL1 : TESTVAL2,
			  i % 2 ? TESTVAL1 : TESTVAL2);
	}

	GUEST_SYNC(stage++);

	 
	for (i = 0; i < NTRY; i++) {
		prepare_to_test(data);
		flush_ex->flags = HV_FLUSH_ALL_VIRTUAL_ADDRESS_SPACES;
		flush_ex->hv_vp_set.format = HV_GENERIC_SET_ALL;
		flush_ex->gva_list[0] = (u64)data->test_pages;
		hyperv_write_xmm_input(&flush_ex->hv_vp_set, 2);
		hyperv_hypercall(HVCALL_FLUSH_VIRTUAL_ADDRESS_LIST_EX |
				 HV_HYPERCALL_FAST_BIT |
				 (1UL << HV_HYPERCALL_REP_COMP_OFFSET),
				 0x0, HV_FLUSH_ALL_VIRTUAL_ADDRESS_SPACES);
		post_test(data, i % 2 ? TESTVAL1 : TESTVAL2,
			  i % 2 ? TESTVAL1 : TESTVAL2);
	}

	GUEST_DONE();
}

static void *vcpu_thread(void *arg)
{
	struct kvm_vcpu *vcpu = (struct kvm_vcpu *)arg;
	struct ucall uc;
	int old;
	int r;

	r = pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, &old);
	TEST_ASSERT(!r, "pthread_setcanceltype failed on vcpu_id=%u with errno=%d",
		    vcpu->id, r);

	vcpu_run(vcpu);
	TEST_ASSERT_KVM_EXIT_REASON(vcpu, KVM_EXIT_IO);

	switch (get_ucall(vcpu, &uc)) {
	case UCALL_ABORT:
		REPORT_GUEST_ASSERT(uc);
		 
	default:
		TEST_FAIL("Unexpected ucall %lu, vCPU %d", uc.cmd, vcpu->id);
	}

	return NULL;
}

static void cancel_join_vcpu_thread(pthread_t thread, struct kvm_vcpu *vcpu)
{
	void *retval;
	int r;

	r = pthread_cancel(thread);
	TEST_ASSERT(!r, "pthread_cancel on vcpu_id=%d failed with errno=%d",
		    vcpu->id, r);

	r = pthread_join(thread, &retval);
	TEST_ASSERT(!r, "pthread_join on vcpu_id=%d failed with errno=%d",
		    vcpu->id, r);
	TEST_ASSERT(retval == PTHREAD_CANCELED,
		    "expected retval=%p, got %p", PTHREAD_CANCELED,
		    retval);
}

int main(int argc, char *argv[])
{
	struct kvm_vm *vm;
	struct kvm_vcpu *vcpu[3];
	pthread_t threads[2];
	vm_vaddr_t test_data_page, gva;
	vm_paddr_t gpa;
	uint64_t *pte;
	struct test_data *data;
	struct ucall uc;
	int stage = 1, r, i;

	vm = vm_create_with_one_vcpu(&vcpu[0], sender_guest_code);

	 
	test_data_page = vm_vaddr_alloc_page(vm);
	data = (struct test_data *)addr_gva2hva(vm, test_data_page);

	 
	data->hcall_gva = vm_vaddr_alloc_pages(vm, 2);
	data->hcall_gpa = addr_gva2gpa(vm, data->hcall_gva);
	memset(addr_gva2hva(vm, data->hcall_gva), 0x0, 2 * PAGE_SIZE);

	 
	data->test_pages = vm_vaddr_alloc_pages(vm, NTEST_PAGES + 1);
	for (i = 0; i < NTEST_PAGES; i++)
		memset(addr_gva2hva(vm, data->test_pages + PAGE_SIZE * i),
		       (u8)(i + 1), PAGE_SIZE);
	set_expected_val(addr_gva2hva(vm, data->test_pages), 0x0, WORKER_VCPU_ID_1);
	set_expected_val(addr_gva2hva(vm, data->test_pages), 0x0, WORKER_VCPU_ID_2);

	 
	gva = vm_vaddr_unused_gap(vm, NTEST_PAGES * PAGE_SIZE, KVM_UTIL_MIN_VADDR);
	for (i = 0; i < NTEST_PAGES; i++) {
		pte = vm_get_page_table_entry(vm, data->test_pages + i * PAGE_SIZE);
		gpa = addr_hva2gpa(vm, pte);
		__virt_pg_map(vm, gva + PAGE_SIZE * i, gpa & PAGE_MASK, PG_LEVEL_4K);
		data->test_pages_pte[i] = gva + (gpa & ~PAGE_MASK);
	}

	 
	vcpu_args_set(vcpu[0], 1, test_data_page);
	vcpu_set_hv_cpuid(vcpu[0]);

	 
	vcpu[1] = vm_vcpu_add(vm, WORKER_VCPU_ID_1, worker_guest_code);
	vcpu_args_set(vcpu[1], 1, test_data_page);
	vcpu_set_msr(vcpu[1], HV_X64_MSR_VP_INDEX, WORKER_VCPU_ID_1);
	vcpu_set_hv_cpuid(vcpu[1]);

	vcpu[2] = vm_vcpu_add(vm, WORKER_VCPU_ID_2, worker_guest_code);
	vcpu_args_set(vcpu[2], 1, test_data_page);
	vcpu_set_msr(vcpu[2], HV_X64_MSR_VP_INDEX, WORKER_VCPU_ID_2);
	vcpu_set_hv_cpuid(vcpu[2]);

	r = pthread_create(&threads[0], NULL, vcpu_thread, vcpu[1]);
	TEST_ASSERT(!r, "pthread_create() failed");

	r = pthread_create(&threads[1], NULL, vcpu_thread, vcpu[2]);
	TEST_ASSERT(!r, "pthread_create() failed");

	while (true) {
		vcpu_run(vcpu[0]);
		TEST_ASSERT_KVM_EXIT_REASON(vcpu[0], KVM_EXIT_IO);

		switch (get_ucall(vcpu[0], &uc)) {
		case UCALL_SYNC:
			TEST_ASSERT(uc.args[1] == stage,
				    "Unexpected stage: %ld (%d expected)\n",
				    uc.args[1], stage);
			break;
		case UCALL_ABORT:
			REPORT_GUEST_ASSERT(uc);
			 
		case UCALL_DONE:
			goto done;
		default:
			TEST_FAIL("Unknown ucall %lu", uc.cmd);
		}

		stage++;
	}

done:
	cancel_join_vcpu_thread(threads[0], vcpu[1]);
	cancel_join_vcpu_thread(threads[1], vcpu[2]);
	kvm_vm_free(vm);

	return 0;
}
