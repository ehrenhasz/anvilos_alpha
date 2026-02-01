
 

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>

#include <altivec.h>

#include "utils.h"
#include "tm.h"

#define MAX_ATTEMPT 500000

#define NV_VSX_REGS 12  
#define VSX20 20  
#define FPR20 20  

long tm_signal_self_context_load(pid_t pid, long *gprs, double *fps, vector int *vms, vector int *vss);

static sig_atomic_t fail, broken;

 
vector int vsxs[] = {
	 
	 
	{ 1, 2, 3, 4},{ 5, 6, 7, 8},{ 9,10,11,12},
	{13,14,15,16},{17,18,19,20},{21,22,23,24},
	{25,26,27,28},{29,30,31,32},{33,34,35,36},
	{37,38,39,40},{41,42,43,44},{45,46,47,48},
	 
	 
	{-1, -2, -3, -4 },{-5, -6, -7, -8 },{-9, -10,-11,-12},
	{-13,-14,-15,-16},{-17,-18,-19,-20},{-21,-22,-23,-24},
	{-25,-26,-27,-28},{-29,-30,-31,-32},{-33,-34,-35,-36},
	{-37,-38,-39,-40},{-41,-42,-43,-44},{-45,-46,-47,-48}
};

static void signal_usr1(int signum, siginfo_t *info, void *uc)
{
	int i, j;
	uint8_t vsx[sizeof(vector int)];
	uint8_t vsx_tm[sizeof(vector int)];
	ucontext_t *ucp = uc;
	ucontext_t *tm_ucp = ucp->uc_link;

	 
	 
	long *vsx_ptr = (long *)(ucp->uc_mcontext.v_regs + 1);
	long *tm_vsx_ptr = (long *)(tm_ucp->uc_mcontext.v_regs + 1);

	 
	for (i = 0; i < NV_VSX_REGS; i++) {
		 
		memcpy(vsx, &ucp->uc_mcontext.fp_regs[FPR20 + i], 8);
		memcpy(vsx + 8, &vsx_ptr[VSX20 + i], 8);

		fail = memcmp(vsx, &vsxs[i], sizeof(vector int));

		if (fail) {
			broken = 1;
			printf("VSX%d (1st context) == 0x", VSX20 + i);
			for (j = 0; j < 16; j++)
				printf("%02x", vsx[j]);
			printf(" instead of 0x");
			for (j = 0; j < 4; j++)
				printf("%08x", vsxs[i][j]);
			printf(" (expected)\n");
		}
	}

	 
	for (i = 0; i < NV_VSX_REGS; i++) {
		 
		memcpy(vsx_tm, &tm_ucp->uc_mcontext.fp_regs[FPR20 + i], 8);
		memcpy(vsx_tm + 8, &tm_vsx_ptr[VSX20 + i], 8);

		fail = memcmp(vsx_tm, &vsxs[NV_VSX_REGS + i], sizeof(vector int));

		if (fail) {
			broken = 1;
			printf("VSX%d (2nd context) == 0x", VSX20 + i);
			for (j = 0; j < 16; j++)
				printf("%02x", vsx_tm[j]);
			printf(" instead of 0x");
			for (j = 0; j < 4; j++)
				printf("%08x", vsxs[NV_VSX_REGS + i][j]);
			printf("(expected)\n");
		}
	}
}

static int tm_signal_context_chk()
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
                
		rc = tm_signal_self_context_load(pid, NULL, NULL, NULL, vsxs);
		FAIL_IF(rc != pid);
		i++;
	}

	return (broken);
}

int main(void)
{
	return test_harness(tm_signal_context_chk, "tm_signal_context_chk_vsx");
}
