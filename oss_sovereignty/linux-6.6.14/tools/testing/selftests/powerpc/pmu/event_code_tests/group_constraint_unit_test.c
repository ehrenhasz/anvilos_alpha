
 

#include <stdio.h>
#include <stdlib.h>

#include "../event.h"
#include "utils.h"
#include "../sampling_tests/misc.h"

 
#define EventCode_1 0x26080
 
#define EventCode_2 0x46080
 
#define EventCode_3 0x36880

 
static int group_constraint_unit(void)
{
	struct event *e, events[3];

	 
	SKIP_IF(platform_check_for_tests());
	SKIP_IF(have_hwcap2(PPC_FEATURE2_ARCH_3_1));

	 
	e = &events[0];
	event_init(e, EventCode_1);

	  
	FAIL_IF(!event_open(&events[0]));

	 
	e = &events[1];
	event_init(e, EventCode_2);

	 
	FAIL_IF(event_open(&events[1]));

	 
	e = &events[2];
	event_init(e, EventCode_3);

	 
	FAIL_IF(!event_open_with_group(&events[2], events[0].fd));

	 
	FAIL_IF(event_open_with_group(&events[2], events[1].fd));

	event_close(&events[0]);
	event_close(&events[1]);
	event_close(&events[2]);

	return 0;
}

int main(void)
{
	return test_harness(group_constraint_unit, "group_constraint_unit");
}
