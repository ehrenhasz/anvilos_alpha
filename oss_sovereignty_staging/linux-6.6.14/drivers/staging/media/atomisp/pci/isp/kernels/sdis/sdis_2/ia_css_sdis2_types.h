 
 

#ifndef __IA_CSS_SDIS2_TYPES_H
#define __IA_CSS_SDIS2_TYPES_H

 

 
#define IA_CSS_DVS2_NUM_COEF_TYPES     4

#ifndef PIPE_GENERATION
#include "isp/kernels/sdis/common/ia_css_sdis_common_types.h"
#endif

 
struct ia_css_dvs2_coef_types {
	s16 *odd_real;  
	s16 *odd_imag;  
	s16 *even_real; 
	s16 *even_imag; 
};

 
struct ia_css_dvs2_coefficients {
	struct ia_css_dvs_grid_info
		grid;         
	struct ia_css_dvs2_coef_types
		hor_coefs;  
	struct ia_css_dvs2_coef_types
		ver_coefs;  
};

 
struct ia_css_dvs2_stat_types {
	s32 *odd_real;  
	s32 *odd_imag;  
	s32 *even_real; 
	s32 *even_imag; 
};

 
struct ia_css_dvs2_statistics {
	struct ia_css_dvs_grid_info
		grid;        
	struct ia_css_dvs2_stat_types
		hor_prod;  
	struct ia_css_dvs2_stat_types
		ver_prod;  
};

#endif  
