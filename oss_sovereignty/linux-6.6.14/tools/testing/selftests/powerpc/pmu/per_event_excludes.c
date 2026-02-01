
 

#define _GNU_SOURCE

#include <elf.h>
#include <limits.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <sys/prctl.h>

#include "event.h"
#include "lib.h"
#include "utils.h"

 

static int per_event_excludes(void)
{
	struct event *e, events[4];
	int i;

	SKIP_IF(!have_hwcap2(PPC_FEATURE2_ARCH_2_07));

	 
	e = &events[0];
	event_init_opts(e, PERF_COUNT_HW_INSTRUCTIONS,
			PERF_TYPE_HARDWARE, "instructions");
	e->attr.disabled = 1;

	e = &events[1];
	event_init_opts(e, PERF_COUNT_HW_INSTRUCTIONS,
			PERF_TYPE_HARDWARE, "instructions(k)");
	e->attr.disabled = 1;
	e->attr.exclude_user = 1;
	e->attr.exclude_hv = 1;

	e = &events[2];
	event_init_opts(e, PERF_COUNT_HW_INSTRUCTIONS,
			PERF_TYPE_HARDWARE, "instructions(h)");
	e->attr.disabled = 1;
	e->attr.exclude_user = 1;
	e->attr.exclude_kernel = 1;

	e = &events[3];
	event_init_opts(e, PERF_COUNT_HW_INSTRUCTIONS,
			PERF_TYPE_HARDWARE, "instructions(u)");
	e->attr.disabled = 1;
	e->attr.exclude_hv = 1;
	e->attr.exclude_kernel = 1;

	FAIL_IF(event_open(&events[0]));

	 
	for (i = 1; i < 4; i++)
		FAIL_IF(event_open_with_group(&events[i], events[0].fd));

	 
	prctl(PR_TASK_PERF_EVENTS_ENABLE);

	 
	for (i = 0; i < INT_MAX; i++)
		asm volatile("" : : : "memory");

	prctl(PR_TASK_PERF_EVENTS_DISABLE);

	for (i = 0; i < 4; i++) {
		FAIL_IF(event_read(&events[i]));
		event_report(&events[i]);
	}

	 
	for (i = 0; i < 4; i++)
		FAIL_IF(events[i].result.running != events[i].result.enabled);

	 
	for (i = 1; i < 4; i++)
		FAIL_IF(events[0].result.value < events[i].result.value);

	for (i = 0; i < 4; i++)
		event_close(&events[i]);

	return 0;
}

int main(void)
{
	return test_harness(per_event_excludes, "per_event_excludes");
}
