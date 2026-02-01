
 

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <setjmp.h>

#include "ebb.h"


 

static struct event event;

static int child(void)
{
	 
	FAIL_IF(mfspr(SPRN_BESCR) != 0);
	FAIL_IF(mfspr(SPRN_EBBHR) != 0);
	FAIL_IF(mfspr(SPRN_EBBRR) != 0);

	FAIL_IF(catch_sigill(write_pmc1));

	 
	FAIL_IF(event_read(&event));

	return 0;
}

 
int fork_cleanup(void)
{
	pid_t pid;

	SKIP_IF(!ebb_is_supported());

	event_init_named(&event, 0x1001e, "cycles");
	event_leader_ebb_init(&event);

	FAIL_IF(event_open(&event));

	ebb_enable_pmc_counting(1);
	setup_ebb_handler(standard_ebb_callee);
	ebb_global_enable();

	FAIL_IF(ebb_event_enable(&event));

	mtspr(SPRN_MMCR0, MMCR0_FC);
	mtspr(SPRN_PMC1, pmc_sample_period(sample_period));

	 

	pid = fork();
	if (pid == 0)
		exit(child());

	 
	FAIL_IF(wait_for_child(pid));

	 
	event_close(&event);

	return 0;
}

int main(void)
{
	return test_harness(fork_cleanup, "fork_cleanup");
}
