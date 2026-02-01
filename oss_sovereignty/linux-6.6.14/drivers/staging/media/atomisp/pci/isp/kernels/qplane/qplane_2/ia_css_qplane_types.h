 
 

#ifndef __IA_CSS_QPLANE_TYPES_H
#define __IA_CSS_QPLANE_TYPES_H

#include <ia_css_frame_public.h>
#include "sh_css_internal.h"

 

struct ia_css_qplane_configuration {
	const struct sh_css_sp_pipeline *pipe;
	const struct ia_css_frame_info  *info;
};

#endif  
