
 

#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <unistd.h>

#include "utils.h"
#include "tm.h"

int segv_expected = 0;

void signal_segv(int signum)
{
	if (segv_expected && (signum == SIGSEGV))
		_exit(0);
	_exit(1);
}

void signal_usr1(int signum, siginfo_t *info, void *uc)
{
	ucontext_t *ucp = uc;

	 
	ucp->uc_link = ucp;
	 
#ifdef __powerpc64__
	ucp->uc_mcontext.gp_regs[PT_MSR] |= (7ULL << 32);
#else
	ucp->uc_mcontext.uc_regs->gregs[PT_MSR] |= (7ULL);
#endif
	 
	segv_expected = 1;
}

int tm_signal_msr_resv()
{
	struct sigaction act;

	SKIP_IF(!have_htm());

	act.sa_sigaction = signal_usr1;
	sigemptyset(&act.sa_mask);
	act.sa_flags = SA_SIGINFO;
	if (sigaction(SIGUSR1, &act, NULL) < 0) {
		perror("sigaction sigusr1");
		exit(1);
	}
	if (signal(SIGSEGV, signal_segv) == SIG_ERR)
		exit(1);

	raise(SIGUSR1);

	 
	return 1;
}

int main(void)
{
	return test_harness(tm_signal_msr_resv, "tm_signal_msr_resv");
}
