 

#ifndef __DC_RESOURCE_DCN10_H__
#define __DC_RESOURCE_DCN10_H__

#include "core_types.h"
#include "dml/dcn10/dcn10_fpu.h"

#define TO_DCN10_RES_POOL(pool)\
	container_of(pool, struct dcn10_resource_pool, base)

struct dc;
struct resource_pool;
struct _vcs_dpi_display_pipe_params_st;

extern struct _vcs_dpi_ip_params_st dcn1_0_ip;
extern struct _vcs_dpi_soc_bounding_box_st dcn1_0_soc;

struct dcn10_resource_pool {
	struct resource_pool base;
};
struct resource_pool *dcn10_create_resource_pool(
		const struct dc_init_data *init_data,
		struct dc *dc);

struct stream_encoder *dcn10_find_first_free_match_stream_enc_for_link(
		struct resource_context *res_ctx,
		const struct resource_pool *pool,
		struct dc_stream_state *stream);


#endif  

