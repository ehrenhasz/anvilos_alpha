
 

#include <stdio.h>
#include <stdlib.h>
#include <setjmp.h>
#include <signal.h>

#include "ebb.h"


 

static int no_handler_test(void)
{
	struct event event;
	u64 val;
	int i;

	SKIP_IF(!ebb_is_supported());

	event_init_named(&event, 0x1001e, "cycles");
	event_leader_ebb_init(&event);

	event.attr.exclude_kernel = 1;
	event.attr.exclude_hv = 1;
	event.attr.exclude_idle = 1;

	FAIL_IF(event_open(&event));
	FAIL_IF(ebb_event_enable(&event));

	val = mfspr(SPRN_EBBHR);
	FAIL_IF(val != 0);

	 
	sample_period = 1000;
	mtspr(SPRN_PMC1, pmc_sample_period(sample_period));

	 
	for (i = 0; i < 1000; i++)
		mb();

	dump_ebb_state();

	 
	val = mfspr(SPRN_MMCR0);
	FAIL_IF(val != 0x0000000080000080);

	event_close(&event);

	 

	return 0;
}

int main(void)
{
	return test_harness(no_handler_test,"no_handler_test");
}
