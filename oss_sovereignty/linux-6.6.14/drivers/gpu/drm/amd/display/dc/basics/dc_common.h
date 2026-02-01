 

#ifndef __DAL_DC_COMMON_H__
#define __DAL_DC_COMMON_H__

#include "core_types.h"

bool is_rgb_cspace(enum dc_color_space output_color_space);

bool is_lower_pipe_tree_visible(struct pipe_ctx *pipe_ctx);

bool is_upper_pipe_tree_visible(struct pipe_ctx *pipe_ctx);

bool is_pipe_tree_visible(struct pipe_ctx *pipe_ctx);

void build_prescale_params(struct  dc_bias_and_scale *bias_and_scale,
		const struct dc_plane_state *plane_state);

#endif
