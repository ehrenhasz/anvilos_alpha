
 

#include <stdio.h>
#include <stdlib.h>

#include "../event.h"
#include "misc.h"
#include "utils.h"

#define MALLOC_SIZE     (0x10000 * 10)   

 
#define EventCode 0x21c040

 
static int mmcr1_sel_unit_cache(void)
{
	struct event event;
	u64 *intr_regs;
	char *p;
	int i;

	 
	SKIP_IF(check_pvr_for_sampling_tests());

	p = malloc(MALLOC_SIZE);
	FAIL_IF(!p);

	 
	event_init_sampling(&event, EventCode);
	event.attr.sample_regs_intr = platform_extended_mask;
	event.attr.sample_period = 1;
	FAIL_IF(event_open(&event));
	event.mmap_buffer = event_sample_buf_mmap(event.fd, 1);

	event_enable(&event);

	 
	for (i = 0; i < MALLOC_SIZE; i += 0x10000)
		p[i] = i;

	event_disable(&event);

	 
	FAIL_IF(!collect_samples(event.mmap_buffer));

	intr_regs = get_intr_regs(&event, event.mmap_buffer);

	 
	FAIL_IF(!intr_regs);

	 
	FAIL_IF(EV_CODE_EXTRACT(event.attr.config, pmcxsel) !=
			get_mmcr1_pmcxsel(get_reg_value(intr_regs, "MMCR1"), 1));
	FAIL_IF(EV_CODE_EXTRACT(event.attr.config, unit) !=
			get_mmcr1_unit(get_reg_value(intr_regs, "MMCR1"), 1));
	FAIL_IF(EV_CODE_EXTRACT(event.attr.config, cache) !=
			get_mmcr1_cache(get_reg_value(intr_regs, "MMCR1"), 1));

	free(p);
	event_close(&event);
	return 0;
}

int main(void)
{
	FAIL_IF(test_harness(mmcr1_sel_unit_cache, "mmcr1_sel_unit_cache"));
}
