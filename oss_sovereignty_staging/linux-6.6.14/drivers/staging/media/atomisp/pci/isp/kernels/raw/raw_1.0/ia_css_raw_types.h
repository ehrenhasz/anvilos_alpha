 
 

#ifndef __IA_CSS_RAW_TYPES_H
#define __IA_CSS_RAW_TYPES_H

#include <ia_css_frame_public.h>
#include "sh_css_internal.h"

 

struct ia_css_raw_configuration {
	const struct sh_css_sp_pipeline *pipe;
	const struct ia_css_frame_info  *in_info;
	const struct ia_css_frame_info  *internal_info;
	bool two_ppc;
	enum atomisp_input_format stream_format;
	bool deinterleaved;
	u8 enable_left_padding;
};

#endif  
