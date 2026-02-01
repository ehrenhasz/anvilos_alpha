 

#include "amdgpu_dm_trace.h"

#define TRACE_DC_PIPE_STATE(pipe_ctx, index, max_pipes) \
	for (index = 0; index < max_pipes; ++index) { \
		struct pipe_ctx *pipe_ctx = &dc->current_state->res_ctx.pipe_ctx[index]; \
		if (pipe_ctx->plane_state) \
			trace_amdgpu_dm_dc_pipe_state(pipe_ctx->pipe_idx, pipe_ctx->plane_state, \
						      pipe_ctx->stream, &pipe_ctx->plane_res, \
						      pipe_ctx->update_flags.raw); \
	}

#define TRACE_DCE_CLOCK_STATE(dce_clocks) \
	trace_amdgpu_dm_dce_clocks_state(dce_clocks)

#define TRACE_DCN_CLOCK_STATE(dcn_clocks) \
	trace_amdgpu_dm_dc_clocks_state(dcn_clocks)

#define TRACE_DCN_FPU(begin, function, line, ref_count) \
	trace_dcn_fpu(begin, function, line, ref_count)
#define TRACE_OPTC_LOCK_UNLOCK_STATE(optc, inst, lock) \
	trace_dcn_optc_lock_unlock_state(optc, inst, lock, __func__, __LINE__)
