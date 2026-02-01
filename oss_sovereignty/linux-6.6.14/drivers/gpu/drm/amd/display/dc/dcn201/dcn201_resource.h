 

#ifndef __DC_RESOURCE_DCN201_H__
#define __DC_RESOURCE_DCN201_H__

#include "core_types.h"

#define RRDPCS_PHY_DP_TX_PSTATE_POWER_UP    0x00000000
#define RRDPCS_PHY_DP_TX_PSTATE_HOLD        0x00000001
#define RRDPCS_PHY_DP_TX_PSTATE_HOLD_OFF    0x00000002
#define RRDPCS_PHY_DP_TX_PSTATE_POWER_DOWN  0x00000003

#define TO_DCN201_RES_POOL(pool)\
	container_of(pool, struct dcn201_resource_pool, base)

struct dc;
struct resource_pool;
struct _vcs_dpi_display_pipe_params_st;

struct dcn201_resource_pool {
	struct resource_pool base;
};
struct resource_pool *dcn201_create_resource_pool(
		const struct dc_init_data *init_data,
		struct dc *dc);

#endif  
