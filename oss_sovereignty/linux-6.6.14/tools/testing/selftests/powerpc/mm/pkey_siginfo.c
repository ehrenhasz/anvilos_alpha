

 

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>

#include <unistd.h>
#include <pthread.h>
#include <sys/mman.h>

#include "pkeys.h"

#define PPC_INST_NOP	0x60000000
#define PPC_INST_BLR	0x4e800020
#define PROT_RWX	(PROT_READ | PROT_WRITE | PROT_EXEC)

#define NUM_ITERATIONS	1000000

static volatile sig_atomic_t perm_pkey, rest_pkey;
static volatile sig_atomic_t rights, fault_count;
static volatile unsigned int *volatile fault_addr;
static pthread_barrier_t iteration_barrier;

static void segv_handler(int signum, siginfo_t *sinfo, void *ctx)
{
	void *pgstart;
	size_t pgsize;
	int pkey;

	pkey = siginfo_pkey(sinfo);

	 
	if (sinfo->si_code != SEGV_PKUERR) {
		sigsafe_err("got a fault for an unexpected reason\n");
		_exit(1);
	}

	 
	if (sinfo->si_addr != (void *) fault_addr) {
		sigsafe_err("got a fault for an unexpected address\n");
		_exit(1);
	}

	 
	if (pkey != rest_pkey) {
		sigsafe_err("got a fault for an unexpected pkey\n");
		_exit(1);
	}

	 
	if (fault_count > 0) {
		sigsafe_err("got too many faults for the same address\n");
		_exit(1);
	}

	pgsize = getpagesize();
	pgstart = (void *) ((unsigned long) fault_addr & ~(pgsize - 1));

	 
	if (rights == PKEY_DISABLE_EXECUTE &&
	    mprotect(pgstart, pgsize, PROT_EXEC))
		_exit(1);
	else
		pkey_set_rights(pkey, 0);

	fault_count++;
}

struct region {
	unsigned long rights;
	unsigned int *base;
	size_t size;
};

static void *protect(void *p)
{
	unsigned long rights;
	unsigned int *base;
	size_t size;
	int tid, i;

	tid = gettid();
	base = ((struct region *) p)->base;
	size = ((struct region *) p)->size;
	FAIL_IF_EXIT(!base);

	 
	rights = 0;

	printf("tid %d, pkey permissions are %s\n", tid, pkey_rights(rights));

	 
	perm_pkey = sys_pkey_alloc(0, rights);
	FAIL_IF_EXIT(perm_pkey < 0);

	 
	for (i = 0; i < NUM_ITERATIONS; i++) {
		 
		pthread_barrier_wait(&iteration_barrier);

		 
		FAIL_IF_EXIT(sys_pkey_mprotect(base, size, PROT_RWX,
					       perm_pkey));
	}

	 
	sys_pkey_free(perm_pkey);

	return NULL;
}

static void *protect_access(void *p)
{
	size_t size, numinsns;
	unsigned int *base;
	int tid, i;

	tid = gettid();
	base = ((struct region *) p)->base;
	size = ((struct region *) p)->size;
	rights = ((struct region *) p)->rights;
	numinsns = size / sizeof(base[0]);
	FAIL_IF_EXIT(!base);

	 
	rest_pkey = sys_pkey_alloc(0, rights);
	FAIL_IF_EXIT(rest_pkey < 0);

	printf("tid %d, pkey permissions are %s\n", tid, pkey_rights(rights));
	printf("tid %d, %s randomly in range [%p, %p]\n", tid,
	       (rights == PKEY_DISABLE_EXECUTE) ? "execute" :
	       (rights == PKEY_DISABLE_WRITE)  ? "write" : "read",
	       base, base + numinsns);

	 
	for (i = 0; i < NUM_ITERATIONS; i++) {
		 
		pthread_barrier_wait(&iteration_barrier);

		 
		FAIL_IF_EXIT(sys_pkey_mprotect(base, size, PROT_RWX,
					       rest_pkey));

		 
		fault_addr = base + (rand() % numinsns);
		fault_count = 0;

		switch (rights) {
		 
		case PKEY_DISABLE_ACCESS:
			 
			FAIL_IF_EXIT(*fault_addr != PPC_INST_NOP &&
				     *fault_addr != PPC_INST_BLR);
			break;

		 
		case PKEY_DISABLE_WRITE:
			 
			*fault_addr = PPC_INST_BLR;
			FAIL_IF_EXIT(*fault_addr != PPC_INST_BLR);
			break;

		 
		case PKEY_DISABLE_EXECUTE:
			 
			asm volatile(
				"mtctr	%0; bctrl"
				: : "r"(fault_addr) : "ctr", "lr");
			break;
		}

		 
		pkey_set_rights(rest_pkey, rights);
	}

	 
	sys_pkey_free(rest_pkey);

	return NULL;
}

static void reset_pkeys(unsigned long rights)
{
	int pkeys[NR_PKEYS], i;

	 
	for (i = 0; i < NR_PKEYS; i++)
		pkeys[i] = sys_pkey_alloc(0, rights);

	 
	for (i = 0; i < NR_PKEYS; i++)
		sys_pkey_free(pkeys[i]);
}

static int test(void)
{
	pthread_t prot_thread, pacc_thread;
	struct sigaction act;
	pthread_attr_t attr;
	size_t numinsns;
	struct region r;
	int ret, i;

	srand(time(NULL));
	ret = pkeys_unsupported();
	if (ret)
		return ret;

	 
	r.size = getpagesize();
	r.base = mmap(NULL, r.size, PROT_RWX,
		      MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	FAIL_IF(r.base == MAP_FAILED);

	 
	numinsns = r.size / sizeof(r.base[0]);
	for (i = 0; i < numinsns - 1; i++)
		r.base[i] = PPC_INST_NOP;
	r.base[i] = PPC_INST_BLR;

	 
	act.sa_handler = 0;
	act.sa_sigaction = segv_handler;
	FAIL_IF(sigprocmask(SIG_SETMASK, 0, &act.sa_mask) != 0);
	act.sa_flags = SA_SIGINFO;
	act.sa_restorer = 0;
	FAIL_IF(sigaction(SIGSEGV, &act, NULL) != 0);

	 
	reset_pkeys(0);

	 
	FAIL_IF(pthread_attr_init(&attr) != 0);
	FAIL_IF(pthread_barrier_init(&iteration_barrier, NULL, 2) != 0);

	 
	puts("starting thread pair (protect, protect-and-read)");
	r.rights = PKEY_DISABLE_ACCESS;
	FAIL_IF(pthread_create(&prot_thread, &attr, &protect, &r) != 0);
	FAIL_IF(pthread_create(&pacc_thread, &attr, &protect_access, &r) != 0);
	FAIL_IF(pthread_join(prot_thread, NULL) != 0);
	FAIL_IF(pthread_join(pacc_thread, NULL) != 0);

	 
	puts("starting thread pair (protect, protect-and-write)");
	r.rights = PKEY_DISABLE_WRITE;
	FAIL_IF(pthread_create(&prot_thread, &attr, &protect, &r) != 0);
	FAIL_IF(pthread_create(&pacc_thread, &attr, &protect_access, &r) != 0);
	FAIL_IF(pthread_join(prot_thread, NULL) != 0);
	FAIL_IF(pthread_join(pacc_thread, NULL) != 0);

	 
	puts("starting thread pair (protect, protect-and-execute)");
	r.rights = PKEY_DISABLE_EXECUTE;
	FAIL_IF(pthread_create(&prot_thread, &attr, &protect, &r) != 0);
	FAIL_IF(pthread_create(&pacc_thread, &attr, &protect_access, &r) != 0);
	FAIL_IF(pthread_join(prot_thread, NULL) != 0);
	FAIL_IF(pthread_join(pacc_thread, NULL) != 0);

	 
	FAIL_IF(pthread_attr_destroy(&attr) != 0);
	FAIL_IF(pthread_barrier_destroy(&iteration_barrier) != 0);
	munmap(r.base, r.size);

	return 0;
}

int main(void)
{
	return test_harness(test, "pkey_siginfo");
}
