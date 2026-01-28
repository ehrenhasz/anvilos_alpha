#ifndef _DCN315_RESOURCE_H_
#define _DCN315_RESOURCE_H_
#include "core_types.h"
#define TO_DCN315_RES_POOL(pool)\
	container_of(pool, struct dcn315_resource_pool, base)
extern struct _vcs_dpi_ip_params_st dcn3_15_ip;
struct dcn315_resource_pool {
	struct resource_pool base;
};
struct resource_pool *dcn315_create_resource_pool(
		const struct dc_init_data *init_data,
		struct dc *dc);
#endif  
