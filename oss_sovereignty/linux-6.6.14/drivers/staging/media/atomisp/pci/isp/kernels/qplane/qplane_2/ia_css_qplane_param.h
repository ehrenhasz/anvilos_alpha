 
 

#ifndef __IA_CSS_QPLANE_PARAM_H
#define __IA_CSS_QPLANE_PARAM_H

#include <type_support.h>
#include "dma.h"

 
struct sh_css_isp_qplane_isp_config {
	u32 width_a_over_b;
	struct dma_port_config port_b;
	u32 inout_port_config;
	u32 input_needs_raw_binning;
	u32 format;  
};

#endif  
