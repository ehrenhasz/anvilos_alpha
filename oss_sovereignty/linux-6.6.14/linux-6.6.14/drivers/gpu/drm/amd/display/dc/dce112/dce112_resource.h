#ifndef __DC_RESOURCE_DCE112_H__
#define __DC_RESOURCE_DCE112_H__
#include "core_types.h"
struct dc;
struct resource_pool;
struct resource_pool *dce112_create_resource_pool(
	uint8_t num_virtual_links,
	struct dc *dc);
enum dc_status dce112_validate_with_context(
		struct dc *dc,
		const struct dc_validation_set set[],
		int set_count,
		struct dc_state *context,
		struct dc_state *old_context);
bool dce112_validate_bandwidth(
	struct dc *dc,
	struct dc_state *context,
	bool fast_validate);
enum dc_status dce112_add_stream_to_ctx(
		struct dc *dc,
		struct dc_state *new_ctx,
		struct dc_stream_state *dc_stream);
#endif  
