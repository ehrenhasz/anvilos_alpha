
 

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <ucontext.h>
#include <unistd.h>
#include <sys/mman.h>

#include "tm.h"
#include "utils.h"
#include "reg.h"

#define COUNT_MAX       5000		 

 
#ifndef __powerpc64__
#undef  MSR_TS_S
#define MSR_TS_S	0
#endif

 
ucontext_t init_context;

 
static volatile int count;

void usr_signal_handler(int signo, siginfo_t *si, void *uc)
{
	ucontext_t *ucp = uc;
	int ret;

	 
	ucp->uc_link = mmap(NULL, sizeof(ucontext_t),
			    PROT_READ | PROT_WRITE,
			    MAP_PRIVATE | MAP_ANONYMOUS, 0, 0);
	if (ucp->uc_link == (void *)-1) {
		perror("Mmap failed");
		exit(-1);
	}

	 
	ret = madvise(ucp->uc_link, sizeof(ucontext_t), MADV_DONTNEED);
	if (ret) {
		perror("madvise failed");
		exit(-1);
	}

	memcpy(&ucp->uc_link->uc_mcontext, &ucp->uc_mcontext,
		sizeof(ucp->uc_mcontext));

	 
	UCONTEXT_MSR(ucp) |= MSR_TS_S;

	 
	if (fork() == 0) {
		 
		count = COUNT_MAX;
	}

	 
}

void seg_signal_handler(int signo, siginfo_t *si, void *uc)
{
	count++;

	 
	setcontext(&init_context);
}

void tm_trap_test(void)
{
	struct sigaction usr_sa, seg_sa;
	stack_t ss;

	usr_sa.sa_flags = SA_SIGINFO | SA_ONSTACK;
	usr_sa.sa_sigaction = usr_signal_handler;

	seg_sa.sa_flags = SA_SIGINFO;
	seg_sa.sa_sigaction = seg_signal_handler;

	 
	getcontext(&init_context);

	while (count < COUNT_MAX) {
		 
		ss.ss_sp = mmap(NULL, SIGSTKSZ, PROT_READ | PROT_WRITE,
				MAP_PRIVATE | MAP_ANONYMOUS, 0, 0);
		ss.ss_size = SIGSTKSZ;
		ss.ss_flags = 0;

		if (ss.ss_sp == (void *)-1) {
			perror("mmap error\n");
			exit(-1);
		}

		 
		if (madvise(ss.ss_sp, SIGSTKSZ, MADV_DONTNEED)) {
			perror("madvise\n");
			exit(-1);
		}

		 
		if (sigaltstack(&ss, NULL)) {
			perror("sigaltstack\n");
			exit(-1);
		}

		 
		sigaction(SIGUSR1, &usr_sa, NULL);
		 
		sigaction(SIGSEGV, &seg_sa, NULL);

		raise(SIGUSR1);
		count++;
	}
}

int tm_signal_context_force_tm(void)
{
	SKIP_IF(!have_htm());
	 
	SKIP_IF(!is_ppc64le());

	tm_trap_test();

	return EXIT_SUCCESS;
}

int main(int argc, char **argv)
{
	test_harness(tm_signal_context_force_tm, "tm_signal_context_force_tm");
}
