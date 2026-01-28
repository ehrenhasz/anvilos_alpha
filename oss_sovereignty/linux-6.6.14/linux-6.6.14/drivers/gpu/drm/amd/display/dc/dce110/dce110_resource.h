#ifndef __DC_RESOURCE_DCE110_H__
#define __DC_RESOURCE_DCE110_H__
#include "core_types.h"
struct dc;
struct resource_pool;
#define TO_DCE110_RES_POOL(pool)\
	container_of(pool, struct dce110_resource_pool, base)
struct dce110_resource_pool {
	struct resource_pool base;
};
void dce110_resource_build_pipe_hw_param(struct pipe_ctx *pipe_ctx);
struct resource_pool *dce110_create_resource_pool(
	uint8_t num_virtual_links,
	struct dc *dc,
	struct hw_asic_id asic_id);
struct stream_encoder *dce110_find_first_free_match_stream_enc_for_link(
		struct resource_context *res_ctx,
		const struct resource_pool *pool,
		struct dc_stream_state *stream);
#endif  
