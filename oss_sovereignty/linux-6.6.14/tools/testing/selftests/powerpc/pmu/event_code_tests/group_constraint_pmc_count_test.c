
 

#include <stdio.h>
#include "../event.h"
#include "../sampling_tests/misc.h"

 

static int group_constraint_pmc_count(void)
{
	struct event *e, events[5];
	int i;

	 
	SKIP_IF(platform_check_for_tests());

	 
	e = &events[0];
	event_init(e, 0x1001a);

	e = &events[1];
	event_init(e, 0x200fc);

	e = &events[2];
	event_init(e, 0x30080);

	e = &events[3];
	event_init(e, 0x40054);

	e = &events[4];
	event_init(e, 0x0002c);

	FAIL_IF(event_open(&events[0]));

	 
	for (i = 1; i < 5; i++) {
		if (i == 4)
			FAIL_IF(!event_open_with_group(&events[i], events[0].fd));
		else
			FAIL_IF(event_open_with_group(&events[i], events[0].fd));
	}

	for (i = 1; i < 4; i++)
		event_close(&events[i]);

	return 0;
}

int main(void)
{
	return test_harness(group_constraint_pmc_count, "group_constraint_pmc_count");
}
