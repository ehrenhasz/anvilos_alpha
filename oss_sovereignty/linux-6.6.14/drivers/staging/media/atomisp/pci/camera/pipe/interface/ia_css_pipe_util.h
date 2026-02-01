 
 

#ifndef __IA_CSS_PIPE_UTIL_H__
#define __IA_CSS_PIPE_UTIL_H__

#include <ia_css_types.h>
#include <ia_css_frame_public.h>

 
unsigned int ia_css_pipe_util_pipe_input_format_bpp(
    const struct ia_css_pipe *const pipe);

void ia_css_pipe_util_create_output_frames(
    struct ia_css_frame *frames[]);

void ia_css_pipe_util_set_output_frames(
    struct ia_css_frame *frames[],
    unsigned int idx,
    struct ia_css_frame *frame);

#endif  
