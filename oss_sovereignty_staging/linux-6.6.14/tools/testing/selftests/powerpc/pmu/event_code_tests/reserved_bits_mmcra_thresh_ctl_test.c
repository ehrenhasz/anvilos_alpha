
 

#include <stdio.h>
#include "../event.h"
#include "../sampling_tests/misc.h"

 

static int reserved_bits_mmcra_thresh_ctl(void)
{
	struct event event;

	 
	SKIP_IF(platform_check_for_tests());

	 
	SKIP_IF(check_for_generic_compat_pmu());

	 
	event_init(&event, 0xf0340401e0);
	FAIL_IF(!event_open(&event));

	event_init(&event, 0x0f340401e0);
	FAIL_IF(!event_open(&event));

	return 0;
}

int main(void)
{
	return test_harness(reserved_bits_mmcra_thresh_ctl, "reserved_bits_mmcra_thresh_ctl");
}
