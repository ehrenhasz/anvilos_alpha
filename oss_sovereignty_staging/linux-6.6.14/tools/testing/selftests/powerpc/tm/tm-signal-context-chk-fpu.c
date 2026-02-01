
 

#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <unistd.h>

#include <altivec.h>

#include "utils.h"
#include "tm.h"

#define MAX_ATTEMPT 500000

#define NV_FPU_REGS 18  
#define FPR14 14  

long tm_signal_self_context_load(pid_t pid, long *gprs, double *fps, vector int *vms, vector int *vss);

 
static double fps[] = {
	 
	 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18,
	 
	-1,-2,-3,-4,-5,-6,-7,-8,-9,-10,-11,-12,-13,-14,-15,-16,-17,-18
};

static sig_atomic_t fail, broken;

static void signal_usr1(int signum, siginfo_t *info, void *uc)
{
	int i;
	ucontext_t *ucp = uc;
	ucontext_t *tm_ucp = ucp->uc_link;

	for (i = 0; i < NV_FPU_REGS; i++) {
		 
		fail = (ucp->uc_mcontext.fp_regs[FPR14 + i] != fps[i]);
		if (fail) {
			broken = 1;
			printf("FPR%d (1st context) == %g instead of %g (expected)\n",
				FPR14 + i, ucp->uc_mcontext.fp_regs[FPR14 + i], fps[i]);
		}
	}

	for (i = 0; i < NV_FPU_REGS; i++) {
		 
		fail = (tm_ucp->uc_mcontext.fp_regs[FPR14 + i] != fps[NV_FPU_REGS + i]);
		if (fail) {
			broken = 1;
			printf("FPR%d (2nd context) == %g instead of %g (expected)\n",
				FPR14 + i, tm_ucp->uc_mcontext.fp_regs[FPR14 + i], fps[NV_FPU_REGS + i]);
		}
	}
}

static int tm_signal_context_chk_fpu()
{
	struct sigaction act;
	int i;
	long rc;
	pid_t pid = getpid();

	SKIP_IF(!have_htm());
	SKIP_IF(htm_is_synthetic());

	act.sa_sigaction = signal_usr1;
	sigemptyset(&act.sa_mask);
	act.sa_flags = SA_SIGINFO;
	if (sigaction(SIGUSR1, &act, NULL) < 0) {
		perror("sigaction sigusr1");
		exit(1);
	}

	i = 0;
	while (i < MAX_ATTEMPT && !broken) {
		 
		rc = tm_signal_self_context_load(pid, NULL, fps, NULL, NULL);
		FAIL_IF(rc != pid);
		i++;
	}

	return (broken);
}

int main(void)
{
	return test_harness(tm_signal_context_chk_fpu, "tm_signal_context_chk_fpu");
}
