#ifndef _DCN321_RESOURCE_H_
#define _DCN321_RESOURCE_H_
#include "core_types.h"
#define TO_DCN321_RES_POOL(pool)\
	container_of(pool, struct dcn321_resource_pool, base)
extern struct _vcs_dpi_ip_params_st dcn3_21_ip;
extern struct _vcs_dpi_soc_bounding_box_st dcn3_21_soc;
struct dcn321_resource_pool {
	struct resource_pool base;
};
struct resource_pool *dcn321_create_resource_pool(
		const struct dc_init_data *init_data,
		struct dc *dc);
#endif  
