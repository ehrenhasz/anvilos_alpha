
 

#include <stdio.h>
#include "../event.h"
#include "../sampling_tests/misc.h"

 

static int group_constraint_pmc56(void)
{
	struct event event;

	 
	SKIP_IF(platform_check_for_tests());

	 
	event_init(&event, 0x2500fa);
	FAIL_IF(!event_open(&event));

	event_init(&event, 0x2600f4);
	FAIL_IF(!event_open(&event));

	 
	event_init(&event, 0x501e0);
	FAIL_IF(!event_open(&event));

	event_init(&event, 0x6001e);
	FAIL_IF(!event_open(&event));

	event_init(&event, 0x501fa);
	FAIL_IF(!event_open(&event));

	 
	event_init(&event, 0x35340500fa);
	FAIL_IF(!event_open(&event));

	return 0;
}

int main(void)
{
	return test_harness(group_constraint_pmc56, "group_constraint_pmc56");
}
