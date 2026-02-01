 

#ifndef DC_INC_LINK_ENC_CFG_H_
#define DC_INC_LINK_ENC_CFG_H_

 

#include "core_types.h"

 
void link_enc_cfg_init(
		const struct dc *dc,
		struct dc_state *state);

 
void link_enc_cfg_copy(const struct dc_state *src_ctx, struct dc_state *dst_ctx);

 
void link_enc_cfg_link_encs_assign(
		struct dc *dc,
		struct dc_state *state,
		struct dc_stream_state *streams[],
		uint8_t stream_count);

 
void link_enc_cfg_link_enc_unassign(
		struct dc_state *state,
		struct dc_stream_state *stream);

 
bool link_enc_cfg_is_transmitter_mappable(
		struct dc *dc,
		struct link_encoder *link_enc);

 
struct dc_stream_state *link_enc_cfg_get_stream_using_link_enc(
		struct dc *dc,
		enum engine_id eng_id);

 
struct dc_link *link_enc_cfg_get_link_using_link_enc(
		struct dc *dc,
		enum engine_id eng_id);

 
struct link_encoder *link_enc_cfg_get_link_enc_used_by_link(
		struct dc *dc,
		const struct dc_link *link);

 
struct link_encoder *link_enc_cfg_get_next_avail_link_enc(struct dc *dc);

 
struct link_encoder *link_enc_cfg_get_link_enc_used_by_stream(
		struct dc *dc,
		const struct dc_stream_state *stream);

 
struct link_encoder *link_enc_cfg_get_link_enc(const struct dc_link *link);

 
struct link_encoder *link_enc_cfg_get_link_enc_used_by_stream_current(
		struct dc *dc,
		const struct dc_stream_state *stream);

 
bool link_enc_cfg_is_link_enc_avail(struct dc *dc, enum engine_id eng_id, struct dc_link *link);

 
bool link_enc_cfg_validate(struct dc *dc, struct dc_state *state);

 
void link_enc_cfg_set_transient_mode(struct dc *dc, struct dc_state *current_state, struct dc_state *new_state);

#endif  
