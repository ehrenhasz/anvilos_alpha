
 

#include <stdio.h>
#include <stdlib.h>

#include "../event.h"
#include "utils.h"
#include "../sampling_tests/misc.h"

 
#define EventCode_1 0x1100fc
 
#define EventCode_2 0x23e054
 
#define EventCode_3 0x13e054

 
static int group_constraint_cache(void)
{
	struct event event, leader;

	 
	SKIP_IF(platform_check_for_tests());

	 
	event_init(&leader, EventCode_1);
	FAIL_IF(event_open(&leader));

	event_init(&event, EventCode_2);

	 
	FAIL_IF(!event_open_with_group(&event, leader.fd));

	event_close(&event);

	 
	event_init(&event, EventCode_3);

	 
	FAIL_IF(event_open_with_group(&event, leader.fd));

	event_close(&leader);
	event_close(&event);

	return 0;
}

int main(void)
{
	return test_harness(group_constraint_cache, "group_constraint_cache");
}
