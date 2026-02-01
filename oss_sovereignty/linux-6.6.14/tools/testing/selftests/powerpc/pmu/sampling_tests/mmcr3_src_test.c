
 

#include <stdio.h>
#include <stdlib.h>

#include "../event.h"
#include "misc.h"
#include "utils.h"

extern void thirty_two_instruction_loop_with_ll_sc(u64 loops, u64 *ll_sc_target);

 
#define EventCode 0x1340000001c040

 
static int mmcr3_src(void)
{
	struct event event;
	u64 *intr_regs;
	u64 dummy;

	 
	SKIP_IF(check_pvr_for_sampling_tests());
	SKIP_IF(!have_hwcap2(PPC_FEATURE2_ARCH_3_1));

	 
	event_init_sampling(&event, EventCode);
	event.attr.sample_regs_intr = platform_extended_mask;
	FAIL_IF(event_open(&event));
	event.mmap_buffer = event_sample_buf_mmap(event.fd, 1);

	FAIL_IF(event_enable(&event));

	 
	thirty_two_instruction_loop_with_ll_sc(1000000, &dummy);

	FAIL_IF(event_disable(&event));

	 
	FAIL_IF(!collect_samples(event.mmap_buffer));

	intr_regs = get_intr_regs(&event, event.mmap_buffer);

	 
	FAIL_IF(!intr_regs);

	 
	FAIL_IF(EV_CODE_EXTRACT(event.attr.config, mmcr3_src) !=
		get_mmcr3_src(get_reg_value(intr_regs, "MMCR3"), 1));

	event_close(&event);
	return 0;
}

int main(void)
{
	return test_harness(mmcr3_src, "mmcr3_src");
}
