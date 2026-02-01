
 

#include <stdio.h>
#include <stdlib.h>

#include "../event.h"
#include "misc.h"
#include "utils.h"

extern void thirty_two_instruction_loop(int loops);

 
static int mmcr0_pmcjce(void)
{
	struct event event;
	u64 *intr_regs;

	 
	SKIP_IF(check_pvr_for_sampling_tests());

	 
	event_init_sampling(&event, 0x500fa);
	event.attr.sample_regs_intr = platform_extended_mask;
	FAIL_IF(event_open(&event));
	event.mmap_buffer = event_sample_buf_mmap(event.fd, 1);

	FAIL_IF(event_enable(&event));

	 
	thirty_two_instruction_loop(10000);

	FAIL_IF(event_disable(&event));

	 
	FAIL_IF(!collect_samples(event.mmap_buffer));

	intr_regs = get_intr_regs(&event, event.mmap_buffer);

	 
	FAIL_IF(!intr_regs);

	 
	FAIL_IF(!get_mmcr0_pmcjce(get_reg_value(intr_regs, "MMCR0"), 5));

	event_close(&event);
	return 0;
}

int main(void)
{
	return test_harness(mmcr0_pmcjce, "mmcr0_pmcjce");
}
