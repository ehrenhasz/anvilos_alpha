


#ifndef AMD_SFH_INIT_H
#define AMD_SFH_INIT_H

#include "../amd_sfh_common.h"

struct amd_sfh1_1_ops {
	int (*init)(struct amd_mp2_dev *mp2);
};

int amd_sfh1_1_init(struct amd_mp2_dev *mp2);

static const struct amd_sfh1_1_ops __maybe_unused sfh1_1_ops = {
	.init = amd_sfh1_1_init,
};

#endif
