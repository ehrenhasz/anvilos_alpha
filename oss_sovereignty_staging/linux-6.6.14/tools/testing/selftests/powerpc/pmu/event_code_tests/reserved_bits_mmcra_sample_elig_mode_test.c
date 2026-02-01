
 

#include <stdio.h>
#include "../event.h"
#include "../sampling_tests/misc.h"

 

static int reserved_bits_mmcra_sample_elig_mode(void)
{
	struct event event;

	 
	SKIP_IF(platform_check_for_tests());

	 
	SKIP_IF(check_for_generic_compat_pmu());

	 
	event_init(&event, 0x50401e0);
	FAIL_IF(!event_open(&event));

	event_init(&event, 0x90401e0);
	FAIL_IF(!event_open(&event));

	event_init(&event, 0xD0401e0);
	FAIL_IF(!event_open(&event));

	event_init(&event, 0x190401e0);
	FAIL_IF(!event_open(&event));

	event_init(&event, 0x1D0401e0);
	FAIL_IF(!event_open(&event));

	event_init(&event, 0x1A0401e0);
	FAIL_IF(!event_open(&event));

	event_init(&event, 0x1E0401e0);
	FAIL_IF(!event_open(&event));

	 
	if (PVR_VER(mfspr(SPRN_PVR)) == POWER10) {
		event_init(&event, 0x100401e0);
		FAIL_IF(!event_open(&event));
	} else if (PVR_VER(mfspr(SPRN_PVR)) == POWER9) {
		event_init(&event, 0xC0401e0);
		FAIL_IF(!event_open(&event));
	}

	return 0;
}

int main(void)
{
	return test_harness(reserved_bits_mmcra_sample_elig_mode,
			    "reserved_bits_mmcra_sample_elig_mode");
}
