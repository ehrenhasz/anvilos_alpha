
 

#include <stdio.h>
#include <stdlib.h>

#include "../event.h"
#include "utils.h"
#include "../sampling_tests/misc.h"

 
#define p9_EventCode_1 0x13e35340401e0
#define p9_EventCode_2 0x17d34340101ec
#define p9_EventCode_3 0x13e35340101ec
#define p10_EventCode_1 0x35340401e0
#define p10_EventCode_2 0x35340101ec

 
static int group_constraint_thresh_cmp(void)
{
	struct event event, leader;

	 
	SKIP_IF(platform_check_for_tests());

	if (have_hwcap2(PPC_FEATURE2_ARCH_3_1)) {
		 
		event_init(&leader, p10_EventCode_1);

		 
		leader.attr.config1 = 1000;
		FAIL_IF(event_open(&leader));

		event_init(&event, p10_EventCode_2);

		 
		event.attr.config1 = 2000;

		 
		FAIL_IF(!event_open_with_group(&event, leader.fd));

		event_close(&event);

		 
		event_init(&event, p10_EventCode_2);

		 
		event.attr.config1 = 1000;

		 
		FAIL_IF(event_open_with_group(&event, leader.fd));

		event_close(&leader);
		event_close(&event);
	} else {
		 
		event_init(&leader, p9_EventCode_1);
		FAIL_IF(event_open(&leader));

		event_init(&event, p9_EventCode_2);

		 
		FAIL_IF(!event_open_with_group(&event, leader.fd));

		event_close(&event);

		 
		event_init(&event, p9_EventCode_3);

		 
		FAIL_IF(event_open_with_group(&event, leader.fd));

		event_close(&leader);
		event_close(&event);
	}

	return 0;
}

int main(void)
{
	return test_harness(group_constraint_thresh_cmp, "group_constraint_thresh_cmp");
}
