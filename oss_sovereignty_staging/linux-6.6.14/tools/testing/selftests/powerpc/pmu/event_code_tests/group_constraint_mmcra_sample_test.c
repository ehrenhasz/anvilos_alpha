
 

#include <stdio.h>
#include "../event.h"
#include "../sampling_tests/misc.h"

#define EventCode_1 0x35340401e0
#define EventCode_2 0x353c0101ec
#define EventCode_3 0x35340101ec
 

static int group_constraint_mmcra_sample(void)
{
	struct event event, leader;

	SKIP_IF(platform_check_for_tests());

	 
	event_init(&leader, EventCode_1);
	FAIL_IF(event_open(&leader));

	event_init(&event, EventCode_2);

	 
	FAIL_IF(!event_open_with_group(&event, leader.fd));

	event_init(&event, EventCode_3);

	 
	FAIL_IF(event_open_with_group(&event, leader.fd));

	event_close(&leader);
	event_close(&event);

	return 0;
}

int main(void)
{
	return test_harness(group_constraint_mmcra_sample, "group_constraint_mmcra_sample");
}
