 
 

#ifndef __IA_CSS_TDF_TYPES_H
#define __IA_CSS_TDF_TYPES_H

 

#include "type_support.h"

 
struct ia_css_tdf_config {
	s32 thres_flat_table[64];	 
	s32 thres_detail_table[64];	 
	s32 epsilon_0;		 
	s32 epsilon_1;		 
	s32 eps_scale_text;		 
	s32 eps_scale_edge;		 
	s32 sepa_flat;		 
	s32 sepa_edge;		 
	s32 blend_flat;		 
	s32 blend_text;		 
	s32 blend_edge;		 
	s32 shading_gain;		 
	s32 shading_base_gain;	 
	s32 local_y_gain;		 
	s32 local_y_base_gain;	 
	s32 rad_x_origin;		 
	s32 rad_y_origin;		 
};

#endif  
