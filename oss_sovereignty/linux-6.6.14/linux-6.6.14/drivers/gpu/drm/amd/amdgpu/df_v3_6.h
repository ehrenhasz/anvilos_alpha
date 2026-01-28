#ifndef __DF_V3_6_H__
#define __DF_V3_6_H__
#include "soc15_common.h"
enum DF_V3_6_MGCG {
	DF_V3_6_MGCG_DISABLE = 0,
	DF_V3_6_MGCG_ENABLE_00_CYCLE_DELAY = 1,
	DF_V3_6_MGCG_ENABLE_01_CYCLE_DELAY = 2,
	DF_V3_6_MGCG_ENABLE_15_CYCLE_DELAY = 13,
	DF_V3_6_MGCG_ENABLE_31_CYCLE_DELAY = 14,
	DF_V3_6_MGCG_ENABLE_63_CYCLE_DELAY = 15
};
extern const struct attribute_group *df_v3_6_attr_groups[];
extern const struct amdgpu_df_funcs df_v3_6_funcs;
#endif
