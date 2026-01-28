#ifndef _IA_CSS_DEBUG_PIPE_H_
#define _IA_CSS_DEBUG_PIPE_H_
#include <ia_css_frame_public.h>
#include <ia_css_stream_public.h>
#include "ia_css_pipeline.h"
void ia_css_debug_pipe_graph_dump_prologue(void);
void ia_css_debug_pipe_graph_dump_epilogue(void);
void ia_css_debug_pipe_graph_dump_stage(
    struct ia_css_pipeline_stage *stage,
    enum ia_css_pipe_id id);
void ia_css_debug_pipe_graph_dump_sp_raw_copy(
    struct ia_css_frame *out_frame);
void ia_css_debug_pipe_graph_dump_stream_config(
    const struct ia_css_stream_config *stream_config);
#endif  
