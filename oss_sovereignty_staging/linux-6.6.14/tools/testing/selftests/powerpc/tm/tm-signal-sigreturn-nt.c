
 

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>

#include "utils.h"
#include "tm.h"

void trap_signal_handler(int signo, siginfo_t *si, void *uc)
{
	ucontext_t *ucp = (ucontext_t *) uc;

	asm("tbegin.; tsuspend.;");

	 
	ucp->uc_mcontext.regs->nip += 4;
}

int tm_signal_sigreturn_nt(void)
{
	struct sigaction trap_sa;

	SKIP_IF(!have_htm());
	SKIP_IF(htm_is_synthetic());

	trap_sa.sa_flags = SA_SIGINFO;
	trap_sa.sa_sigaction = trap_signal_handler;

	sigaction(SIGTRAP, &trap_sa, NULL);

	raise(SIGTRAP);

	return EXIT_SUCCESS;
}

int main(int argc, char **argv)
{
	test_harness(tm_signal_sigreturn_nt, "tm_signal_sigreturn_nt");
}

