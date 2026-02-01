 
 

#ifndef DCE100_RESOURCE_H_
#define DCE100_RESOURCE_H_

struct dc;
struct resource_pool;
struct dc_validation_set;

struct resource_pool *dce100_create_resource_pool(
	uint8_t num_virtual_links,
	struct dc *dc);

enum dc_status dce100_validate_plane(const struct dc_plane_state *plane_state, struct dc_caps *caps);

enum dc_status dce100_add_stream_to_ctx(
		struct dc *dc,
		struct dc_state *new_ctx,
		struct dc_stream_state *dc_stream);

struct stream_encoder *dce100_find_first_free_match_stream_enc_for_link(
		struct resource_context *res_ctx,
		const struct resource_pool *pool,
		struct dc_stream_state *stream);

#endif  
