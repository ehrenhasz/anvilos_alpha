 

#ifndef _DCN316_RESOURCE_H_
#define _DCN316_RESOURCE_H_

#include "core_types.h"

#define TO_DCN316_RES_POOL(pool)\
	container_of(pool, struct dcn316_resource_pool, base)

extern struct _vcs_dpi_ip_params_st dcn3_16_ip;

struct dcn316_resource_pool {
	struct resource_pool base;
};

struct resource_pool *dcn316_create_resource_pool(
		const struct dc_init_data *init_data,
		struct dc *dc);

#endif  
