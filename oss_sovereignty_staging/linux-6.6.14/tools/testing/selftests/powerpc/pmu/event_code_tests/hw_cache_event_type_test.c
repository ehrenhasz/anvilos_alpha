
 

#include <stdio.h>
#include <stdlib.h>

#include "../event.h"
#include "utils.h"
#include "../sampling_tests/misc.h"

 
#define EventCode_1 0x10000
 
#define EventCode_2 0x0100
 
#define EventCode_3 0x0103
 
#define EventCode_4 0x030000

 
static int hw_cache_event_type_test(void)
{
	struct event event;

	 
	SKIP_IF(platform_check_for_tests());

	 
	SKIP_IF(check_for_generic_compat_pmu());

	 
	event_init_opts(&event, EventCode_1, PERF_TYPE_HW_CACHE, "event");

	 
	FAIL_IF(event_open(&event));
	event_close(&event);

	 
	event_init_opts(&event, EventCode_2, PERF_TYPE_HW_CACHE, "event");

	 
	FAIL_IF(!event_open(&event));
	event_close(&event);

	 
	event_init_opts(&event, EventCode_3, PERF_TYPE_HW_CACHE, "event");

	 
	FAIL_IF(!event_open(&event));
	event_close(&event);

	 
	event_init_opts(&event, EventCode_4, PERF_TYPE_HW_CACHE, "event");

	 
	FAIL_IF(!event_open(&event));
	event_close(&event);

	return 0;
}

int main(void)
{
	return test_harness(hw_cache_event_type_test, "hw_cache_event_type_test");
}
