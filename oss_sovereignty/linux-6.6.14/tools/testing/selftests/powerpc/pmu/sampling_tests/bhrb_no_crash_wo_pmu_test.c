
 

#include <stdio.h>
#include <stdlib.h>

#include "../event.h"
#include "misc.h"
#include "utils.h"

 
static int bhrb_no_crash_wo_pmu_test(void)
{
	struct event event;

	 
	event_init_opts(&event, 0, PERF_TYPE_SOFTWARE, "cycles");

	event.attr.sample_period = 1000;
	event.attr.sample_type = PERF_SAMPLE_BRANCH_STACK;
	event.attr.disabled = 1;

	 
	event_open(&event);

	event_close(&event);
	return 0;
}

int main(void)
{
	return test_harness(bhrb_no_crash_wo_pmu_test, "bhrb_no_crash_wo_pmu_test");
}
