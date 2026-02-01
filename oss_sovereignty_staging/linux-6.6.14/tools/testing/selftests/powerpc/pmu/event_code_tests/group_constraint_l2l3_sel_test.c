
 

#include <stdio.h>
#include <stdlib.h>

#include "../event.h"
#include "utils.h"
#include "../sampling_tests/misc.h"

 
#define EventCode_1 0x010000046080
 
#define EventCode_2 0x26880
 
#define EventCode_3 0x010000026880

 
static int group_constraint_l2l3_sel(void)
{
	struct event event, leader;

	 
	SKIP_IF(platform_check_for_tests());
	SKIP_IF(!have_hwcap2(PPC_FEATURE2_ARCH_3_1));

	 
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
	return test_harness(group_constraint_l2l3_sel, "group_constraint_l2l3_sel");
}
