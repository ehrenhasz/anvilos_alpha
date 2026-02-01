
 

#include <stdio.h>
#include <stdlib.h>

#include "../event.h"
#include "misc.h"
#include "utils.h"

 
#define p9_EventCode 0x13E35340401e0
#define p10_EventCode 0x35340401e0

extern void thirty_two_instruction_loop_with_ll_sc(u64 loops, u64 *ll_sc_target);

 
static int mmcra_thresh_cmp(void)
{
	struct event event;
	u64 *intr_regs;
	u64 dummy;

	 
	SKIP_IF(check_pvr_for_sampling_tests());

	 
	SKIP_IF(check_for_compat_mode());

	 
	if (!have_hwcap2(PPC_FEATURE2_ARCH_3_1)) {
		event_init_sampling(&event, p9_EventCode);
	} else {
		event_init_sampling(&event, p10_EventCode);
		event.attr.config1 = 1000;
	}

	event.attr.sample_regs_intr = platform_extended_mask;
	FAIL_IF(event_open(&event));
	event.mmap_buffer = event_sample_buf_mmap(event.fd, 1);

	FAIL_IF(event_enable(&event));

	 
	thirty_two_instruction_loop_with_ll_sc(1000000, &dummy);

	FAIL_IF(event_disable(&event));

	 
	FAIL_IF(!collect_samples(event.mmap_buffer));

	intr_regs = get_intr_regs(&event, event.mmap_buffer);

	 
	FAIL_IF(!intr_regs);

	 
	FAIL_IF(get_thresh_cmp_val(event) !=
			get_mmcra_thd_cmp(get_reg_value(intr_regs, "MMCRA"), 4));

	event_close(&event);
	return 0;
}

int main(void)
{
	FAIL_IF(test_harness(mmcra_thresh_cmp, "mmcra_thresh_cmp"));
}
