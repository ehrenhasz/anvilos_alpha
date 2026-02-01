 
 

#ifndef __IA_CSS_SDIS_TYPES_H
#define __IA_CSS_SDIS_TYPES_H

 

 
#define IA_CSS_DVS_NUM_COEF_TYPES      6

#ifndef PIPE_GENERATION
#include "isp/kernels/sdis/common/ia_css_sdis_common_types.h"
#endif

 

struct ia_css_dvs_coefficients {
	struct ia_css_dvs_grid_info
		grid; 
	s16 *hor_coefs;	 
	s16 *ver_coefs;	 
};

 

struct ia_css_dvs_statistics {
	struct ia_css_dvs_grid_info
		grid; 
	s32 *hor_proj;	 
	s32 *ver_proj;	 
};

#endif  
