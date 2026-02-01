
 

#include <stdio.h>
#include <stdlib.h>

#include "../event.h"
#include "misc.h"
#include "utils.h"

 
#define EventCode 0x46880

extern void thirty_two_instruction_loop_with_ll_sc(u64 loops, u64 *ll_sc_target);

 
static int mmcr1_comb(void)
{
	struct event event;
	u64 *intr_regs;
	u64 dummy;

	 
	SKIP_IF(check_pvr_for_sampling_tests());

	 
	event_init_sampling(&event, EventCode);
	event.attr.sample_regs_intr = platform_extended_mask;
	FAIL_IF(event_open(&event));
	event.mmap_buffer = event_sample_buf_mmap(event.fd, 1);

	FAIL_IF(event_enable(&event));

	 
	thirty_two_instruction_loop_with_ll_sc(10000000, &dummy);

	FAIL_IF(event_disable(&event));

	 
	FAIL_IF(!collect_samples(event.mmap_buffer));

	intr_regs = get_intr_regs(&event, event.mmap_buffer);

	 
	FAIL_IF(!intr_regs);

	 
	FAIL_IF(EV_CODE_EXTRACT(event.attr.config, comb) !=
		get_mmcr1_comb(get_reg_value(intr_regs, "MMCR1"), 4));

	event_close(&event);
	return 0;
}

int main(void)
{
	return test_harness(mmcr1_comb, "mmcr1_comb");
}
