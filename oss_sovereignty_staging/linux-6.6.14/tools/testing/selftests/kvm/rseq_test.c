
#define _GNU_SOURCE  
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <syscall.h>
#include <sys/ioctl.h>
#include <sys/sysinfo.h>
#include <asm/barrier.h>
#include <linux/atomic.h>
#include <linux/rseq.h>
#include <linux/unistd.h>

#include "kvm_util.h"
#include "processor.h"
#include "test_util.h"

#include "../rseq/rseq.c"

 
#define NR_TASK_MIGRATIONS 100000

static pthread_t migration_thread;
static cpu_set_t possible_mask;
static int min_cpu, max_cpu;
static bool done;

static atomic_t seq_cnt;

static void guest_code(void)
{
	for (;;)
		GUEST_SYNC(0);
}

static int next_cpu(int cpu)
{
	 
	do {
		cpu++;
		if (cpu > max_cpu) {
			cpu = min_cpu;
			TEST_ASSERT(CPU_ISSET(cpu, &possible_mask),
				    "Min CPU = %d must always be usable", cpu);
			break;
		}
	} while (!CPU_ISSET(cpu, &possible_mask));

	return cpu;
}

static void *migration_worker(void *__rseq_tid)
{
	pid_t rseq_tid = (pid_t)(unsigned long)__rseq_tid;
	cpu_set_t allowed_mask;
	int r, i, cpu;

	CPU_ZERO(&allowed_mask);

	for (i = 0, cpu = min_cpu; i < NR_TASK_MIGRATIONS; i++, cpu = next_cpu(cpu)) {
		CPU_SET(cpu, &allowed_mask);

		 
		atomic_inc(&seq_cnt);

		 
		smp_wmb();
		r = sched_setaffinity(rseq_tid, sizeof(allowed_mask), &allowed_mask);
		TEST_ASSERT(!r, "sched_setaffinity failed, errno = %d (%s)",
			    errno, strerror(errno));
		smp_wmb();
		atomic_inc(&seq_cnt);

		CPU_CLR(cpu, &allowed_mask);

		 
		usleep((i % 10) + 1);
	}
	done = true;
	return NULL;
}

static void calc_min_max_cpu(void)
{
	int i, cnt, nproc;

	TEST_REQUIRE(CPU_COUNT(&possible_mask) >= 2);

	 
	nproc = get_nprocs_conf();
	min_cpu = -1;
	max_cpu = -1;
	cnt = 0;

	for (i = 0; i < nproc; i++) {
		if (!CPU_ISSET(i, &possible_mask))
			continue;
		if (min_cpu == -1)
			min_cpu = i;
		max_cpu = i;
		cnt++;
	}

	__TEST_REQUIRE(cnt >= 2,
		       "Only one usable CPU, task migration not possible");
}

int main(int argc, char *argv[])
{
	int r, i, snapshot;
	struct kvm_vm *vm;
	struct kvm_vcpu *vcpu;
	u32 cpu, rseq_cpu;

	r = sched_getaffinity(0, sizeof(possible_mask), &possible_mask);
	TEST_ASSERT(!r, "sched_getaffinity failed, errno = %d (%s)", errno,
		    strerror(errno));

	calc_min_max_cpu();

	r = rseq_register_current_thread();
	TEST_ASSERT(!r, "rseq_register_current_thread failed, errno = %d (%s)",
		    errno, strerror(errno));

	 
	vm = vm_create_with_one_vcpu(&vcpu, guest_code);

	pthread_create(&migration_thread, NULL, migration_worker,
		       (void *)(unsigned long)syscall(SYS_gettid));

	for (i = 0; !done; i++) {
		vcpu_run(vcpu);
		TEST_ASSERT(get_ucall(vcpu, NULL) == UCALL_SYNC,
			    "Guest failed?");

		 
		do {
			 
			snapshot = atomic_read(&seq_cnt) & ~1;

			 
			smp_rmb();
			r = sys_getcpu(&cpu, NULL);
			TEST_ASSERT(!r, "getcpu failed, errno = %d (%s)",
				    errno, strerror(errno));
			rseq_cpu = rseq_current_cpu_raw();
			smp_rmb();
		} while (snapshot != atomic_read(&seq_cnt));

		TEST_ASSERT(rseq_cpu == cpu,
			    "rseq CPU = %d, sched CPU = %d\n", rseq_cpu, cpu);
	}

	 
	TEST_ASSERT(i > (NR_TASK_MIGRATIONS / 2),
		    "Only performed %d KVM_RUNs, task stalled too much?\n", i);

	pthread_join(migration_thread, NULL);

	kvm_vm_free(vm);

	rseq_unregister_current_thread();

	return 0;
}
