
 

#define _GNU_SOURCE  
#include <getopt.h>
#include <pthread.h>
#include <inttypes.h>
#include <string.h>
#include <time.h>

#include "kvm_util.h"
#include "numaif.h"
#include "processor.h"
#include "test_util.h"
#include "vmx.h"

 
#define DEFAULT_RUN_SECS 3

 
#define DEFAULT_DELAY_USECS 500000

 
#define IPI_VECTOR	 0xa5

 
static volatile uint64_t ipis_rcvd;

 
struct test_data_page {
	uint32_t halter_apic_id;
	volatile uint64_t hlt_count;
	volatile uint64_t wake_count;
	uint64_t ipis_sent;
	uint64_t migrations_attempted;
	uint64_t migrations_completed;
	uint32_t icr;
	uint32_t icr2;
	uint32_t halter_tpr;
	uint32_t halter_ppr;

	 
	uint32_t halter_lvr;
};

struct thread_params {
	struct test_data_page *data;
	struct kvm_vcpu *vcpu;
	uint64_t *pipis_rcvd;  
};

void verify_apic_base_addr(void)
{
	uint64_t msr = rdmsr(MSR_IA32_APICBASE);
	uint64_t base = GET_APIC_BASE(msr);

	GUEST_ASSERT(base == APIC_DEFAULT_GPA);
}

static void halter_guest_code(struct test_data_page *data)
{
	verify_apic_base_addr();
	xapic_enable();

	data->halter_apic_id = GET_APIC_ID_FIELD(xapic_read_reg(APIC_ID));
	data->halter_lvr = xapic_read_reg(APIC_LVR);

	 
	for (;;) {
		data->halter_tpr = xapic_read_reg(APIC_TASKPRI);
		data->halter_ppr = xapic_read_reg(APIC_PROCPRI);
		data->hlt_count++;
		asm volatile("sti; hlt; cli");
		data->wake_count++;
	}
}

 
static void guest_ipi_handler(struct ex_regs *regs)
{
	ipis_rcvd++;
	xapic_write_reg(APIC_EOI, 77);
}

static void sender_guest_code(struct test_data_page *data)
{
	uint64_t last_wake_count;
	uint64_t last_hlt_count;
	uint64_t last_ipis_rcvd_count;
	uint32_t icr_val;
	uint32_t icr2_val;
	uint64_t tsc_start;

	verify_apic_base_addr();
	xapic_enable();

	 
	icr_val = (APIC_DEST_PHYSICAL | APIC_DM_FIXED | IPI_VECTOR);
	icr2_val = SET_APIC_DEST_FIELD(data->halter_apic_id);
	data->icr = icr_val;
	data->icr2 = icr2_val;

	last_wake_count = data->wake_count;
	last_hlt_count = data->hlt_count;
	last_ipis_rcvd_count = ipis_rcvd;
	for (;;) {
		 
		xapic_write_reg(APIC_ICR2, icr2_val);
		xapic_write_reg(APIC_ICR, icr_val);
		data->ipis_sent++;

		 
		tsc_start = rdtsc();
		while (rdtsc() - tsc_start < 2000000000) {
			if ((ipis_rcvd != last_ipis_rcvd_count) &&
			    (data->wake_count != last_wake_count) &&
			    (data->hlt_count != last_hlt_count))
				break;
		}

		GUEST_ASSERT((ipis_rcvd != last_ipis_rcvd_count) &&
			     (data->wake_count != last_wake_count) &&
			     (data->hlt_count != last_hlt_count));

		last_wake_count = data->wake_count;
		last_hlt_count = data->hlt_count;
		last_ipis_rcvd_count = ipis_rcvd;
	}
}

static void *vcpu_thread(void *arg)
{
	struct thread_params *params = (struct thread_params *)arg;
	struct kvm_vcpu *vcpu = params->vcpu;
	struct ucall uc;
	int old;
	int r;

	r = pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, &old);
	TEST_ASSERT(r == 0,
		    "pthread_setcanceltype failed on vcpu_id=%u with errno=%d",
		    vcpu->id, r);

	fprintf(stderr, "vCPU thread running vCPU %u\n", vcpu->id);
	vcpu_run(vcpu);

	TEST_ASSERT_KVM_EXIT_REASON(vcpu, KVM_EXIT_IO);

	if (get_ucall(vcpu, &uc) == UCALL_ABORT) {
		TEST_ASSERT(false,
			    "vCPU %u exited with error: %s.\n"
			    "Sending vCPU sent %lu IPIs to halting vCPU\n"
			    "Halting vCPU halted %lu times, woke %lu times, received %lu IPIs.\n"
			    "Halter TPR=%#x PPR=%#x LVR=%#x\n"
			    "Migrations attempted: %lu\n"
			    "Migrations completed: %lu\n",
			    vcpu->id, (const char *)uc.args[0],
			    params->data->ipis_sent, params->data->hlt_count,
			    params->data->wake_count,
			    *params->pipis_rcvd, params->data->halter_tpr,
			    params->data->halter_ppr, params->data->halter_lvr,
			    params->data->migrations_attempted,
			    params->data->migrations_completed);
	}

	return NULL;
}

static void cancel_join_vcpu_thread(pthread_t thread, struct kvm_vcpu *vcpu)
{
	void *retval;
	int r;

	r = pthread_cancel(thread);
	TEST_ASSERT(r == 0,
		    "pthread_cancel on vcpu_id=%d failed with errno=%d",
		    vcpu->id, r);

	r = pthread_join(thread, &retval);
	TEST_ASSERT(r == 0,
		    "pthread_join on vcpu_id=%d failed with errno=%d",
		    vcpu->id, r);
	TEST_ASSERT(retval == PTHREAD_CANCELED,
		    "expected retval=%p, got %p", PTHREAD_CANCELED,
		    retval);
}

void do_migrations(struct test_data_page *data, int run_secs, int delay_usecs,
		   uint64_t *pipis_rcvd)
{
	long pages_not_moved;
	unsigned long nodemask = 0;
	unsigned long nodemasks[sizeof(nodemask) * 8];
	int nodes = 0;
	time_t start_time, last_update, now;
	time_t interval_secs = 1;
	int i, r;
	int from, to;
	unsigned long bit;
	uint64_t hlt_count;
	uint64_t wake_count;
	uint64_t ipis_sent;

	fprintf(stderr, "Calling migrate_pages every %d microseconds\n",
		delay_usecs);

	 
	r = get_mempolicy(NULL, &nodemask, sizeof(nodemask) * 8,
			  0, MPOL_F_MEMS_ALLOWED);
	TEST_ASSERT(r == 0, "get_mempolicy failed errno=%d", errno);

	fprintf(stderr, "Numa nodes found amongst first %lu possible nodes "
		"(each 1-bit indicates node is present): %#lx\n",
		sizeof(nodemask) * 8, nodemask);

	 
	for (i = 0, bit = 1; i < sizeof(nodemask) * 8; i++, bit <<= 1) {
		if (nodemask & bit) {
			nodemasks[nodes] = nodemask & bit;
			nodes++;
		}
	}

	TEST_ASSERT(nodes > 1,
		    "Did not find at least 2 numa nodes. Can't do migration\n");

	fprintf(stderr, "Migrating amongst %d nodes found\n", nodes);

	from = 0;
	to = 1;
	start_time = time(NULL);
	last_update = start_time;

	ipis_sent = data->ipis_sent;
	hlt_count = data->hlt_count;
	wake_count = data->wake_count;

	while ((int)(time(NULL) - start_time) < run_secs) {
		data->migrations_attempted++;

		 
		pages_not_moved = migrate_pages(0, sizeof(nodemasks[from]),
						&nodemasks[from],
						&nodemasks[to]);
		if (pages_not_moved < 0)
			fprintf(stderr,
				"migrate_pages failed, errno=%d\n", errno);
		else if (pages_not_moved > 0)
			fprintf(stderr,
				"migrate_pages could not move %ld pages\n",
				pages_not_moved);
		else
			data->migrations_completed++;

		from = to;
		to++;
		if (to == nodes)
			to = 0;

		now = time(NULL);
		if (((now - start_time) % interval_secs == 0) &&
		    (now != last_update)) {
			last_update = now;
			fprintf(stderr,
				"%lu seconds: Migrations attempted=%lu completed=%lu, "
				"IPIs sent=%lu received=%lu, HLTs=%lu wakes=%lu\n",
				now - start_time, data->migrations_attempted,
				data->migrations_completed,
				data->ipis_sent, *pipis_rcvd,
				data->hlt_count, data->wake_count);

			TEST_ASSERT(ipis_sent != data->ipis_sent &&
				    hlt_count != data->hlt_count &&
				    wake_count != data->wake_count,
				    "IPI, HLT and wake count have not increased "
				    "in the last %lu seconds. "
				    "HLTer is likely hung.\n", interval_secs);

			ipis_sent = data->ipis_sent;
			hlt_count = data->hlt_count;
			wake_count = data->wake_count;
		}
		usleep(delay_usecs);
	}
}

void get_cmdline_args(int argc, char *argv[], int *run_secs,
		      bool *migrate, int *delay_usecs)
{
	for (;;) {
		int opt = getopt(argc, argv, "s:d:m");

		if (opt == -1)
			break;
		switch (opt) {
		case 's':
			*run_secs = parse_size(optarg);
			break;
		case 'm':
			*migrate = true;
			break;
		case 'd':
			*delay_usecs = parse_size(optarg);
			break;
		default:
			TEST_ASSERT(false,
				    "Usage: -s <runtime seconds>. Default is %d seconds.\n"
				    "-m adds calls to migrate_pages while vCPUs are running."
				    " Default is no migrations.\n"
				    "-d <delay microseconds> - delay between migrate_pages() calls."
				    " Default is %d microseconds.\n",
				    DEFAULT_RUN_SECS, DEFAULT_DELAY_USECS);
		}
	}
}

int main(int argc, char *argv[])
{
	int r;
	int wait_secs;
	const int max_halter_wait = 10;
	int run_secs = 0;
	int delay_usecs = 0;
	struct test_data_page *data;
	vm_vaddr_t test_data_page_vaddr;
	bool migrate = false;
	pthread_t threads[2];
	struct thread_params params[2];
	struct kvm_vm *vm;
	uint64_t *pipis_rcvd;

	get_cmdline_args(argc, argv, &run_secs, &migrate, &delay_usecs);
	if (run_secs <= 0)
		run_secs = DEFAULT_RUN_SECS;
	if (delay_usecs <= 0)
		delay_usecs = DEFAULT_DELAY_USECS;

	vm = vm_create_with_one_vcpu(&params[0].vcpu, halter_guest_code);

	vm_init_descriptor_tables(vm);
	vcpu_init_descriptor_tables(params[0].vcpu);
	vm_install_exception_handler(vm, IPI_VECTOR, guest_ipi_handler);

	virt_pg_map(vm, APIC_DEFAULT_GPA, APIC_DEFAULT_GPA);

	params[1].vcpu = vm_vcpu_add(vm, 1, sender_guest_code);

	test_data_page_vaddr = vm_vaddr_alloc_page(vm);
	data = addr_gva2hva(vm, test_data_page_vaddr);
	memset(data, 0, sizeof(*data));
	params[0].data = data;
	params[1].data = data;

	vcpu_args_set(params[0].vcpu, 1, test_data_page_vaddr);
	vcpu_args_set(params[1].vcpu, 1, test_data_page_vaddr);

	pipis_rcvd = (uint64_t *)addr_gva2hva(vm, (uint64_t)&ipis_rcvd);
	params[0].pipis_rcvd = pipis_rcvd;
	params[1].pipis_rcvd = pipis_rcvd;

	 
	r = pthread_create(&threads[0], NULL, vcpu_thread, &params[0]);
	TEST_ASSERT(r == 0,
		    "pthread_create halter failed errno=%d", errno);
	fprintf(stderr, "Halter vCPU thread started\n");

	wait_secs = 0;
	while ((wait_secs < max_halter_wait) && !data->hlt_count) {
		sleep(1);
		wait_secs++;
	}

	TEST_ASSERT(data->hlt_count,
		    "Halter vCPU did not execute first HLT within %d seconds",
		    max_halter_wait);

	fprintf(stderr,
		"Halter vCPU thread reported its APIC ID: %u after %d seconds.\n",
		data->halter_apic_id, wait_secs);

	r = pthread_create(&threads[1], NULL, vcpu_thread, &params[1]);
	TEST_ASSERT(r == 0, "pthread_create sender failed errno=%d", errno);

	fprintf(stderr,
		"IPI sender vCPU thread started. Letting vCPUs run for %d seconds.\n",
		run_secs);

	if (!migrate)
		sleep(run_secs);
	else
		do_migrations(data, run_secs, delay_usecs, pipis_rcvd);

	 
	cancel_join_vcpu_thread(threads[0], params[0].vcpu);
	cancel_join_vcpu_thread(threads[1], params[1].vcpu);

	fprintf(stderr,
		"Test successful after running for %d seconds.\n"
		"Sending vCPU sent %lu IPIs to halting vCPU\n"
		"Halting vCPU halted %lu times, woke %lu times, received %lu IPIs.\n"
		"Halter APIC ID=%#x\n"
		"Sender ICR value=%#x ICR2 value=%#x\n"
		"Halter TPR=%#x PPR=%#x LVR=%#x\n"
		"Migrations attempted: %lu\n"
		"Migrations completed: %lu\n",
		run_secs, data->ipis_sent,
		data->hlt_count, data->wake_count, *pipis_rcvd,
		data->halter_apic_id,
		data->icr, data->icr2,
		data->halter_tpr, data->halter_ppr, data->halter_lvr,
		data->migrations_attempted, data->migrations_completed);

	kvm_vm_free(vm);

	return 0;
}
