
 

#include <stdio.h>
#include "../event.h"
#include <sys/prctl.h>
#include <limits.h>
#include "../sampling_tests/misc.h"

 

static int group_pmc56_exclude_constraints(void)
{
	struct event *e, events[3];
	int i;

	 
	SKIP_IF(platform_check_for_tests());

	 
	e = &events[0];
	event_init(e, 0x500fa);

	e = &events[1];
	event_init(e, 0x600f4);

	e = &events[2];
	event_init(e, 0x22C040);

	FAIL_IF(event_open(&events[0]));

	 
	for (i = 1; i < 3; i++)
		FAIL_IF(event_open_with_group(&events[i], events[0].fd));

	for (i = 0; i < 3; i++)
		event_close(&events[i]);

	return 0;
}

int main(void)
{
	return test_harness(group_pmc56_exclude_constraints, "group_pmc56_exclude_constraints");
}
