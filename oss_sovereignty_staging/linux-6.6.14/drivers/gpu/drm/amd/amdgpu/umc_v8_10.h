 
#ifndef __UMC_V8_10_H__
#define __UMC_V8_10_H__

#include "soc15_common.h"
#include "amdgpu.h"

 
#define UMC_V8_10_CHANNEL_INSTANCE_NUM		2
 
#define UMC_V8_10_UMC_INSTANCE_NUM		2

 
#define UMC_V8_10_TOTAL_CHANNEL_NUM(adev) \
	(UMC_V8_10_CHANNEL_INSTANCE_NUM * UMC_V8_10_UMC_INSTANCE_NUM * \
	(adev)->gmc.num_umc - hweight32((adev)->gmc.m_half_use) * 2)

 
#define UMC_V8_10_PER_CHANNEL_OFFSET	0x400

 
#define UMC_V8_10_CE_CNT_MAX		0xffff
 
#define UUMC_V8_10_CE_INT_THRESHOLD	0xffff
 
#define UMC_V8_10_CE_CNT_INIT	(UMC_V8_10_CE_CNT_MAX - UUMC_V8_10_CE_INT_THRESHOLD)

#define UMC_V8_10_NA_COL_2BITS_POWER_OF_2_NUM	 4

 
#define UMC_V8_10_NA_C5_BIT	14

 
#define SWIZZLE_MODE_TMP_ADDR(na, ch_num, ch_idx) \
		((((na) >> 10) * (ch_num) + (ch_idx)) << 10)
#define SWIZZLE_MODE_ADDR_HI(addr, col_bit)  \
		(((addr) >> ((col_bit) + 2)) << ((col_bit) + 2))
#define SWIZZLE_MODE_ADDR_MID(na, col_bit) ((((na) >> 8) & 0x3) << (col_bit))
#define SWIZZLE_MODE_ADDR_LOW(addr, col_bit) \
		((((addr) >> 10) & ((0x1ULL << (col_bit - 8)) - 1)) << 8)
#define SWIZZLE_MODE_ADDR_LSB(na) ((na) & 0xFF)

extern struct amdgpu_umc_ras umc_v8_10_ras;
extern const uint32_t
	umc_v8_10_channel_idx_tbl[]
				[UMC_V8_10_UMC_INSTANCE_NUM]
				[UMC_V8_10_CHANNEL_INSTANCE_NUM];

extern const uint32_t
	umc_v8_10_channel_idx_tbl_ext0[]
				[UMC_V8_10_UMC_INSTANCE_NUM]
				[UMC_V8_10_CHANNEL_INSTANCE_NUM];
#endif

