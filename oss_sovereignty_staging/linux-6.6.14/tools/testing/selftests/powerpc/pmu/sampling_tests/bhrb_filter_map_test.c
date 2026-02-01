
 

#include <stdio.h>
#include <stdlib.h>

#include "../event.h"
#include "misc.h"
#include "utils.h"

 

 
 
int bhrb_filter_map_valid_common[] = {
	PERF_SAMPLE_BRANCH_ANY,
	PERF_SAMPLE_BRANCH_ANY_CALL,
};

 
int bhrb_filter_map_valid_p10[] = {
	PERF_SAMPLE_BRANCH_IND_CALL,
	PERF_SAMPLE_BRANCH_COND,
};

#define EventCode 0x1001e

static int bhrb_filter_map_test(void)
{
	struct event event;
	int i;

	 
	SKIP_IF(platform_check_for_tests());

	 
	SKIP_IF(check_for_generic_compat_pmu());

	 
	event_init(&event, EventCode);

	event.attr.sample_period = 1000;
	event.attr.sample_type = PERF_SAMPLE_BRANCH_STACK;
	event.attr.disabled = 1;

	 
	for (i = PERF_SAMPLE_BRANCH_USER_SHIFT; i < PERF_SAMPLE_BRANCH_MAX_SHIFT; i++) {
		 
		if (i == PERF_SAMPLE_BRANCH_ANY_SHIFT || i == PERF_SAMPLE_BRANCH_ANY_CALL_SHIFT \
			|| i == PERF_SAMPLE_BRANCH_IND_CALL_SHIFT || i == PERF_SAMPLE_BRANCH_COND_SHIFT)
			continue;
		event.attr.branch_sample_type = 1U << i;
		FAIL_IF(!event_open(&event));
	}

	 
	for (i = 0; i < ARRAY_SIZE(bhrb_filter_map_valid_common); i++) {
		event.attr.branch_sample_type = bhrb_filter_map_valid_common[i];
		FAIL_IF(event_open(&event));
		event_close(&event);
	}

	 
	if (PVR_VER(mfspr(SPRN_PVR)) == POWER10) {
		for (i = 0; i < ARRAY_SIZE(bhrb_filter_map_valid_p10); i++) {
			event.attr.branch_sample_type = bhrb_filter_map_valid_p10[i];
			FAIL_IF(event_open(&event));
			event_close(&event);
		}
	} else {
		for (i = 0; i < ARRAY_SIZE(bhrb_filter_map_valid_p10); i++) {
			event.attr.branch_sample_type = bhrb_filter_map_valid_p10[i];
			FAIL_IF(!event_open(&event));
		}
	}

	 
	event.attr.branch_sample_type = PERF_SAMPLE_BRANCH_ANY | PERF_SAMPLE_BRANCH_ANY_CALL;
	FAIL_IF(!event_open(&event));

	return 0;
}

int main(void)
{
	return test_harness(bhrb_filter_map_test, "bhrb_filter_map_test");
}
