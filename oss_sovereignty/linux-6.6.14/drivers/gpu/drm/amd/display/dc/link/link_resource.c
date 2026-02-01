 
 

#include "link_resource.h"
#include "protocols/link_dp_capability.h"

void link_get_cur_link_res(const struct dc_link *link,
		struct link_resource *link_res)
{
	int i;
	struct pipe_ctx *pipe = NULL;

	memset(link_res, 0, sizeof(*link_res));

	for (i = 0; i < MAX_PIPES; i++) {
		pipe = &link->dc->current_state->res_ctx.pipe_ctx[i];
		if (pipe->stream && pipe->stream->link && pipe->top_pipe == NULL) {
			if (pipe->stream->link == link) {
				*link_res = pipe->link_res;
				break;
			}
		}
	}

}

void link_get_cur_res_map(const struct dc *dc, uint32_t *map)
{
	struct dc_link *link;
	uint32_t i;
	uint32_t hpo_dp_recycle_map = 0;

	*map = 0;

	if (dc->caps.dp_hpo) {
		for (i = 0; i < dc->caps.max_links; i++) {
			link = dc->links[i];
			if (link->link_status.link_active &&
					link_dp_get_encoding_format(&link->reported_link_cap) == DP_128b_132b_ENCODING &&
					link_dp_get_encoding_format(&link->cur_link_settings) != DP_128b_132b_ENCODING)
				 
				hpo_dp_recycle_map |= (1 << i);
		}
		*map |= (hpo_dp_recycle_map << LINK_RES_HPO_DP_REC_MAP__SHIFT);
	}
}

void link_restore_res_map(const struct dc *dc, uint32_t *map)
{
	struct dc_link *link;
	uint32_t i;
	unsigned int available_hpo_dp_count;
	uint32_t hpo_dp_recycle_map = (*map & LINK_RES_HPO_DP_REC_MAP__MASK)
			>> LINK_RES_HPO_DP_REC_MAP__SHIFT;

	if (dc->caps.dp_hpo) {
		available_hpo_dp_count = dc->res_pool->hpo_dp_link_enc_count;
		 
		for (i = 0; i < dc->caps.max_links; i++) {
			if ((hpo_dp_recycle_map & (1 << i)) == 0) {
				link = dc->links[i];
				if (link->type != dc_connection_none &&
						link_dp_get_encoding_format(&link->verified_link_cap) == DP_128b_132b_ENCODING) {
					if (available_hpo_dp_count > 0)
						available_hpo_dp_count--;
					else
						 
						link->verified_link_cap.link_rate = LINK_RATE_HIGH3;
				}
			}
		}
		 
		for (i = 0; i < dc->caps.max_links; i++) {
			if ((hpo_dp_recycle_map & (1 << i)) != 0) {
				link = dc->links[i];
				if (link->type != dc_connection_none &&
						link_dp_get_encoding_format(&link->verified_link_cap) == DP_128b_132b_ENCODING) {
					if (available_hpo_dp_count > 0)
						available_hpo_dp_count--;
					else
						 
						link->verified_link_cap.link_rate = LINK_RATE_HIGH3;
				}
			}
		}
	}
}
