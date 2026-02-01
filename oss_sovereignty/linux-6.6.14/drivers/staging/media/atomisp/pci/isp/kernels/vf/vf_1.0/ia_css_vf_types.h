 
 

#ifndef __IA_CSS_VF_TYPES_H
#define __IA_CSS_VF_TYPES_H

 

#include <ia_css_frame_public.h>
#include <type_support.h>

struct ia_css_vf_configuration {
	u32 vf_downscale_bits;  
	const struct ia_css_frame_info *info;
};

#endif  
