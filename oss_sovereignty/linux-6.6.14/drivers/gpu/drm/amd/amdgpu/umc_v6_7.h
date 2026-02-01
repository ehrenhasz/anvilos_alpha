 
#ifndef __UMC_V6_7_H__
#define __UMC_V6_7_H__

#include "soc15_common.h"
#include "amdgpu.h"

 
#define UMC_V6_7_CE_CNT_MAX		0xffff
 
#define UMC_V6_7_CE_INT_THRESHOLD	0xffff
 
#define UMC_V6_7_CE_CNT_INIT	(UMC_V6_7_CE_CNT_MAX - UMC_V6_7_CE_INT_THRESHOLD)

#define UMC_V6_7_INST_DIST	0x40000

 
#define UMC_V6_7_UMC_INSTANCE_NUM		4
 
#define UMC_V6_7_CHANNEL_INSTANCE_NUM		8
 
#define UMC_V6_7_TOTAL_CHANNEL_NUM	(UMC_V6_7_CHANNEL_INSTANCE_NUM * UMC_V6_7_UMC_INSTANCE_NUM)
 
#define UMC_V6_7_NA_MAP_PA_NUM	8
 
#define UMC_V6_7_BAD_PAGE_NUM_PER_CHANNEL	(UMC_V6_7_NA_MAP_PA_NUM * 2)
 
#define UMC_V6_7_PA_CH4_BIT	12
 
#define UMC_V6_7_PA_C2_BIT	17
 
#define UMC_V6_7_PA_R14_BIT	34
 
#define UMC_V6_7_PER_CHANNEL_OFFSET		0x400

 
#define CHANNEL_HASH(channel_idx, pa) (((channel_idx) >> 4) ^ \
			(((pa)  >> 20) & 0x1ULL & adev->df.hash_status.hash_64k) ^ \
			(((pa)  >> 25) & 0x1ULL & adev->df.hash_status.hash_2m) ^ \
			(((pa)  >> 34) & 0x1ULL & adev->df.hash_status.hash_1g))
#define SET_CHANNEL_HASH(channel_idx, pa) do { \
		(pa) &= ~(0x1ULL << UMC_V6_7_PA_CH4_BIT); \
		(pa) |= (CHANNEL_HASH(channel_idx, pa) << UMC_V6_7_PA_CH4_BIT); \
	} while (0)

extern struct amdgpu_umc_ras umc_v6_7_ras;
extern const uint32_t
	umc_v6_7_channel_idx_tbl_second[UMC_V6_7_UMC_INSTANCE_NUM][UMC_V6_7_CHANNEL_INSTANCE_NUM];
extern const uint32_t
	umc_v6_7_channel_idx_tbl_first[UMC_V6_7_UMC_INSTANCE_NUM][UMC_V6_7_CHANNEL_INSTANCE_NUM];
void umc_v6_7_convert_error_address(struct amdgpu_device *adev,
                                    struct ras_err_data *err_data, uint64_t err_addr,
                                    uint32_t ch_inst, uint32_t umc_inst);
#endif
