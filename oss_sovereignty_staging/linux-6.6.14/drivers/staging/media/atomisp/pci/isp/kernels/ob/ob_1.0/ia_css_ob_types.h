 
 

#ifndef __IA_CSS_OB_TYPES_H
#define __IA_CSS_OB_TYPES_H

 

#include "ia_css_frac.h"

 
enum ia_css_ob_mode {
	IA_CSS_OB_MODE_NONE,	 
	IA_CSS_OB_MODE_FIXED,	 
	IA_CSS_OB_MODE_RASTER	 
};

 
struct ia_css_ob_config {
	enum ia_css_ob_mode mode;  
	ia_css_u0_16 level_gr;     
	ia_css_u0_16 level_r;      
	ia_css_u0_16 level_b;      
	ia_css_u0_16 level_gb;     
	u16 start_position;  
	u16 end_position;   
};

#endif  
