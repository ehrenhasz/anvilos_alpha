#ifndef __UMC_V8_7_H__
#define __UMC_V8_7_H__
#include "soc15_common.h"
#include "amdgpu.h"
#define UMC_V8_7_HBM_MEMORY_CHANNEL_WIDTH	128
#define UMC_V8_7_CHANNEL_INSTANCE_NUM		2
#define UMC_V8_7_UMC_INSTANCE_NUM		8
#define UMC_V8_7_TOTAL_CHANNEL_NUM	(UMC_V8_7_CHANNEL_INSTANCE_NUM * UMC_V8_7_UMC_INSTANCE_NUM)
#define UMC_V8_7_PER_CHANNEL_OFFSET_SIENNA	0x400
#define UMC_V8_7_CE_CNT_MAX		0xffff
#define UMC_V8_7_CE_INT_THRESHOLD	0xffff
#define UMC_V8_7_CE_CNT_INIT	(UMC_V8_7_CE_CNT_MAX - UMC_V8_7_CE_INT_THRESHOLD)
extern struct amdgpu_umc_ras umc_v8_7_ras;
extern const uint32_t
	umc_v8_7_channel_idx_tbl[UMC_V8_7_UMC_INSTANCE_NUM][UMC_V8_7_CHANNEL_INSTANCE_NUM];
#endif
