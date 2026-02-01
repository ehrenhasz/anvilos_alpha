
 

#include <stdio.h>
#include "../event.h"
#include "../sampling_tests/misc.h"

 
#define EventCode1 0x21C040
#define EventCode2 0x22C040

 

static int group_constraint_repeat(void)
{
	struct event event, leader;

	 
	SKIP_IF(platform_check_for_tests());

	 
	event_init(&leader, EventCode1);
	FAIL_IF(event_open(&leader));

	event_init(&event, EventCode1);

	 
	FAIL_IF(!event_open_with_group(&event, leader.fd));

	event_init(&event, EventCode2);

	 
	FAIL_IF(event_open_with_group(&event, leader.fd));

	event_close(&leader);
	event_close(&event);

	return 0;
}

int main(void)
{
	return test_harness(group_constraint_repeat, "group_constraint_repeat");
}
