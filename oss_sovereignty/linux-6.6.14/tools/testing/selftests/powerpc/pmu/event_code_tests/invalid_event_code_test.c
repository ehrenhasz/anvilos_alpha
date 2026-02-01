
 

#include <stdio.h>
#include <sys/prctl.h>
#include <limits.h>
#include "../event.h"
#include "../sampling_tests/misc.h"

 
#define EventCode_1 0x1340000001c040
 
#define EventCode_2 0x14242
 
#define EventCode_3 0xf00000000000001e

 

static int invalid_event_code(void)
{
	struct event event;

	 
	SKIP_IF(platform_check_for_tests());

	 
	if (have_hwcap2(PPC_FEATURE2_ARCH_3_1)) {
		event_init(&event, EventCode_1);
		FAIL_IF(event_open(&event));
		event_close(&event);

		event_init(&event, EventCode_2);
		FAIL_IF(event_open(&event));
		event_close(&event);
	} else {
		event_init(&event, EventCode_1);
		FAIL_IF(!event_open(&event));

		event_init(&event, EventCode_2);
		FAIL_IF(!event_open(&event));
	}

	return 0;
}

int main(void)
{
	return test_harness(invalid_event_code, "invalid_event_code");
}
