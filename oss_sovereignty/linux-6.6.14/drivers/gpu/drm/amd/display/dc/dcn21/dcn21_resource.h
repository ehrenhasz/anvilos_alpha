 

#ifndef _DCN21_RESOURCE_H_
#define _DCN21_RESOURCE_H_

#include "core_types.h"

#define TO_DCN21_RES_POOL(pool)\
	container_of(pool, struct dcn21_resource_pool, base)

struct dc;
struct resource_pool;
struct _vcs_dpi_display_pipe_params_st;

extern struct _vcs_dpi_ip_params_st dcn2_1_ip;
extern struct _vcs_dpi_soc_bounding_box_st dcn2_1_soc;

struct dcn21_resource_pool {
	struct resource_pool base;
};
struct resource_pool *dcn21_create_resource_pool(
		const struct dc_init_data *init_data,
		struct dc *dc);
bool dcn21_fast_validate_bw(
		struct dc *dc,
		struct dc_state *context,
		display_e2e_pipe_params_st *pipes,
		int *pipe_cnt_out,
		int *pipe_split_from,
		int *vlevel_out,
		bool fast_validate);

#endif  
