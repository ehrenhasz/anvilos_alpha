 
 

#ifndef __IA_CSS_BNR_PARAM_H
#define __IA_CSS_BNR_PARAM_H

#include "type_support.h"

 
struct sh_css_isp_bnr_params {
	s32 gain_all;
	s32 gain_dir;
	s32 threshold_low;
	s32 threshold_width_log2;
	s32 threshold_width;
	s32 clip;
};

#endif  
