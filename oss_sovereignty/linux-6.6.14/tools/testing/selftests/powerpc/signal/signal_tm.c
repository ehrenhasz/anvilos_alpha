
 

#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>

#include <altivec.h>

#include "utils.h"
#include "../tm/tm.h"

#define MAX_ATTEMPT 500000
#define TIMEOUT 10

extern long tm_signal_self(pid_t pid, int sig, long *ret);

static sig_atomic_t signaled;
static sig_atomic_t fail;

static void signal_handler(int sig)
{
	if (tcheck_active()) {
		fail = 2;
		return;
	}

	if (sig == SIGUSR1)
		signaled = 1;
	else
		fail = 1;
}

static int test_signal_tm()
{
	int i;
	struct sigaction act;

	act.sa_handler = signal_handler;
	act.sa_flags = 0;
	sigemptyset(&act.sa_mask);
	if (sigaction(SIGUSR1, &act, NULL) < 0) {
		perror("sigaction SIGUSR1");
		exit(1);
	}
	if (sigaction(SIGALRM, &act, NULL) < 0) {
		perror("sigaction SIGALRM");
		exit(1);
	}

	SKIP_IF(!have_htm());
	SKIP_IF(htm_is_synthetic());

	for (i = 0; i < MAX_ATTEMPT; i++) {
		 
		long ret = 0xdead;
		long rc = 0xbeef;

		alarm(0);  
		signaled = 0;
		alarm(TIMEOUT);
		FAIL_IF(tcheck_transactional());
		rc = tm_signal_self(getpid(), SIGUSR1, &ret);
		if (ret == 0xdead)
			 
			continue;

		if (rc || ret) {
			 
			printf("TEXASR 0x%016lx, TFIAR 0x%016lx\n",
					__builtin_get_texasr(), __builtin_get_tfiar());
			fprintf(stderr, "(%d) Fail reason: %d rc=0x%lx ret=0x%lx\n",
					i, fail, rc, ret);
			FAIL_IF(ret);
		}
		while(!signaled && !fail)
			asm volatile("": : :"memory");
		if (!signaled) {
			fprintf(stderr, "(%d) Fail reason: %d rc=0x%lx ret=0x%lx\n",
					i, fail, rc, ret);
			FAIL_IF(fail);  
		}
	}

	return 0;
}

int main(void)
{
	return test_harness(test_signal_tm, "signal_tm");
}
