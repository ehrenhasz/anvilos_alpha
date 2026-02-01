
 

#include <stdio.h>
#include "../event.h"
#include "../sampling_tests/misc.h"

#define PM_RUN_CYC_ALT 0x200f4
#define PM_INST_DISP 0x200f2
#define PM_BR_2PATH 0x20036
#define PM_LD_MISS_L1 0x3e054
#define PM_RUN_INST_CMPL_ALT 0x400fa

#define EventCode_1 0x200fa
#define EventCode_2 0x200fc
#define EventCode_3 0x300fc
#define EventCode_4 0x400fc

 

static int event_alternatives_tests_p9(void)
{
	struct event event, leader;

	 
	SKIP_IF(platform_check_for_tests());

	 
	SKIP_IF(PVR_VER(mfspr(SPRN_PVR)) != POWER9);

	 
	SKIP_IF(check_for_generic_compat_pmu());

	 
	event_init(&leader, PM_RUN_CYC_ALT);
	FAIL_IF(event_open(&leader));

	event_init(&event, EventCode_1);

	 
	FAIL_IF(event_open_with_group(&event, leader.fd));

	event_close(&leader);
	event_close(&event);

	event_init(&leader, PM_INST_DISP);
	FAIL_IF(event_open(&leader));

	event_init(&event, EventCode_2);
	 
	FAIL_IF(event_open_with_group(&event, leader.fd));

	event_close(&leader);
	event_close(&event);

	event_init(&leader, PM_BR_2PATH);
	FAIL_IF(event_open(&leader));

	event_init(&event, EventCode_2);
	 
	FAIL_IF(event_open_with_group(&event, leader.fd));

	event_close(&leader);
	event_close(&event);

	event_init(&leader, PM_LD_MISS_L1);
	FAIL_IF(event_open(&leader));

	event_init(&event, EventCode_3);
	 
	FAIL_IF(event_open_with_group(&event, leader.fd));

	event_close(&leader);
	event_close(&event);

	event_init(&leader, PM_RUN_INST_CMPL_ALT);
	FAIL_IF(event_open(&leader));

	event_init(&event, EventCode_4);
	 
	FAIL_IF(event_open_with_group(&event, leader.fd));

	event_close(&leader);
	event_close(&event);

	return 0;
}

int main(void)
{
	return test_harness(event_alternatives_tests_p9, "event_alternatives_tests_p9");
}
