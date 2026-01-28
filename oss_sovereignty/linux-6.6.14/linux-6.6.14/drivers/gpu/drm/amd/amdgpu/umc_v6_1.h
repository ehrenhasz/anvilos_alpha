#ifndef __UMC_V6_1_H__
#define __UMC_V6_1_H__
#include "soc15_common.h"
#include "amdgpu.h"
#define UMC_V6_1_HBM_MEMORY_CHANNEL_WIDTH	128
#define UMC_V6_1_CHANNEL_INSTANCE_NUM		4
#define UMC_V6_1_UMC_INSTANCE_NUM		8
#define UMC_V6_1_TOTAL_CHANNEL_NUM	(UMC_V6_1_CHANNEL_INSTANCE_NUM * UMC_V6_1_UMC_INSTANCE_NUM)
#define UMC_V6_1_PER_CHANNEL_OFFSET_VG20	0x800
#define UMC_V6_1_PER_CHANNEL_OFFSET_ARCT	0x400
#define UMC_V6_1_CE_CNT_MAX		0xffff
#define UMC_V6_1_CE_INT_THRESHOLD	0xffff
#define UMC_V6_1_CE_CNT_INIT	(UMC_V6_1_CE_CNT_MAX - UMC_V6_1_CE_INT_THRESHOLD)
extern struct amdgpu_umc_ras umc_v6_1_ras;
extern const uint32_t
	umc_v6_1_channel_idx_tbl[UMC_V6_1_UMC_INSTANCE_NUM][UMC_V6_1_CHANNEL_INSTANCE_NUM];
#endif
