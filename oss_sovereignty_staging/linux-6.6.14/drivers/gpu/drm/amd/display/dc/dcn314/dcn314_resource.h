 
 

#ifndef _DCN314_RESOURCE_H_
#define _DCN314_RESOURCE_H_

#include "core_types.h"

extern struct _vcs_dpi_ip_params_st dcn3_14_ip;
extern struct _vcs_dpi_soc_bounding_box_st dcn3_14_soc;

#define TO_DCN314_RES_POOL(pool)\
	container_of(pool, struct dcn314_resource_pool, base)

struct dcn314_resource_pool {
	struct resource_pool base;
};

bool dcn314_validate_bandwidth(struct dc *dc,
		struct dc_state *context,
		bool fast_validate);

struct resource_pool *dcn314_create_resource_pool(
		const struct dc_init_data *init_data,
		struct dc *dc);

#endif  
