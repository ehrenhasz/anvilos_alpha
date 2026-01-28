#ifndef __IA_CSS_CROP_TYPES_H
#define __IA_CSS_CROP_TYPES_H
#include <ia_css_frame_public.h>
#include "sh_css_uds.h"  
struct ia_css_crop_config {
	struct sh_css_crop_pos crop_pos;
};
struct ia_css_crop_configuration {
	const struct ia_css_frame_info *info;
};
#endif  
