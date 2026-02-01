
 

#include <stdio.h>
#include <stdlib.h>

#include "../event.h"
#include "misc.h"
#include "utils.h"

 
#define EventCode 0x010000046080

#define MALLOC_SIZE     (0x10000 * 10)   

 
static int mmcr2_l2l3(void)
{
	struct event event;
	u64 *intr_regs;
	char *p;
	int i;

	 
	SKIP_IF(check_pvr_for_sampling_tests());
	SKIP_IF(!have_hwcap2(PPC_FEATURE2_ARCH_3_1));

	 
	event_init_sampling(&event, EventCode);
	event.attr.sample_regs_intr = platform_extended_mask;
	FAIL_IF(event_open(&event));
	event.mmap_buffer = event_sample_buf_mmap(event.fd, 1);

	FAIL_IF(event_enable(&event));

	 
	p = malloc(MALLOC_SIZE);
	FAIL_IF(!p);

	for (i = 0; i < MALLOC_SIZE; i += 0x10000)
		p[i] = i;

	FAIL_IF(event_disable(&event));

	 
	FAIL_IF(!collect_samples(event.mmap_buffer));

	intr_regs = get_intr_regs(&event, event.mmap_buffer);

	 
	FAIL_IF(!intr_regs);

	 
	FAIL_IF(EV_CODE_EXTRACT(event.attr.config, l2l3) !=
		get_mmcr2_l2l3(get_reg_value(intr_regs, "MMCR2"), 4));

	event_close(&event);
	free(p);

	return 0;
}

int main(void)
{
	return test_harness(mmcr2_l2l3, "mmcr2_l2l3");
}
