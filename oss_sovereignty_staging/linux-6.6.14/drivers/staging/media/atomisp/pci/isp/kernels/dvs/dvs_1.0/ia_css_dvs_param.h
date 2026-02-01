 
 

#ifndef __IA_CSS_DVS_PARAM_H
#define __IA_CSS_DVS_PARAM_H

#include <type_support.h>

#if !defined(ENABLE_TPROXY) && !defined(ENABLE_CRUN_FOR_TD) && !defined(PARAMBIN_GENERATION)
#include "dma.h"
#endif  

#include "uds/uds_1.0/ia_css_uds_param.h"

 
struct sh_css_isp_dvs_isp_config {
	u32 num_horizontal_blocks;
	u32 num_vertical_blocks;
};

#endif  
