 
 

#ifndef __IA_CSS_CROP_PARAM_H
#define __IA_CSS_CROP_PARAM_H

#include <type_support.h>
#include "dma.h"
#include "sh_css_internal.h"  

 
struct sh_css_isp_crop_isp_config {
	u32 width_a_over_b;
	struct dma_port_config port_b;
};

struct sh_css_isp_crop_isp_params {
	struct sh_css_crop_pos crop_pos;
};

#endif  
