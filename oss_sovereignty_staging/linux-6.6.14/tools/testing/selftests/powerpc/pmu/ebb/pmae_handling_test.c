
 

#include <sched.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include "ebb.h"


 

static bool mmcr0_mismatch;
static uint64_t before, after;

static void syscall_ebb_callee(void)
{
	uint64_t val;

	val = mfspr(SPRN_BESCR);
	if (!(val & BESCR_PMEO)) {
		ebb_state.stats.spurious++;
		goto out;
	}

	ebb_state.stats.ebb_count++;
	count_pmc(1, sample_period);

	before = mfspr(SPRN_MMCR0);

	 
	sched_yield();

	after = mfspr(SPRN_MMCR0);
	if (before != after)
		mmcr0_mismatch = true;

out:
	reset_ebb();
}

static int test_body(void)
{
	struct event event;

	SKIP_IF(!ebb_is_supported());

	event_init_named(&event, 0x1001e, "cycles");
	event_leader_ebb_init(&event);

	event.attr.exclude_kernel = 1;
	event.attr.exclude_hv = 1;
	event.attr.exclude_idle = 1;

	FAIL_IF(event_open(&event));

	setup_ebb_handler(syscall_ebb_callee);
	ebb_global_enable();

	FAIL_IF(ebb_event_enable(&event));

	mtspr(SPRN_PMC1, pmc_sample_period(sample_period));

	while (ebb_state.stats.ebb_count < 20 && !mmcr0_mismatch)
		FAIL_IF(core_busy_loop());

	ebb_global_disable();
	ebb_freeze_pmcs();

	dump_ebb_state();

	if (mmcr0_mismatch)
		printf("Saw MMCR0 before 0x%lx after 0x%lx\n", before, after);

	event_close(&event);

	FAIL_IF(ebb_state.stats.ebb_count == 0);
	FAIL_IF(mmcr0_mismatch);

	return 0;
}

int pmae_handling(void)
{
	return eat_cpu(test_body);
}

int main(void)
{
	return test_harness(pmae_handling, "pmae_handling");
}
