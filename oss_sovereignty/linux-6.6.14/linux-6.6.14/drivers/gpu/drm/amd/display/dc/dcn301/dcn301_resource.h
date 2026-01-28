#ifndef _DCN301_RESOURCE_H_
#define _DCN301_RESOURCE_H_
#include "core_types.h"
struct dc;
struct resource_pool;
struct _vcs_dpi_display_pipe_params_st;
extern struct _vcs_dpi_ip_params_st dcn3_01_ip;
extern struct _vcs_dpi_soc_bounding_box_st dcn3_01_soc;
struct dcn301_resource_pool {
	struct resource_pool base;
};
struct resource_pool *dcn301_create_resource_pool(
		const struct dc_init_data *init_data,
		struct dc *dc);
#endif  
