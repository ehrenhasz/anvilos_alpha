 

#ifndef _PP_OVERDRIVER_H_
#define _PP_OVERDRIVER_H_

#include <linux/types.h>
#include <linux/kernel.h>

struct phm_fuses_default {
	uint64_t key;
	uint32_t VFT2_m1;
	uint32_t VFT2_m2;
	uint32_t VFT2_b;
	uint32_t VFT1_m1;
	uint32_t VFT1_m2;
	uint32_t VFT1_b;
	uint32_t VFT0_m1;
	uint32_t VFT0_m2;
	uint32_t VFT0_b;
};

extern int pp_override_get_default_fuse_value(uint64_t key,
			struct phm_fuses_default *result);

#endif
