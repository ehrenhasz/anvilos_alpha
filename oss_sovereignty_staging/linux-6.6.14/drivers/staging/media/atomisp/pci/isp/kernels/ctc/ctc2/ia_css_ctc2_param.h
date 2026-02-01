 
 

#ifndef __IA_CSS_CTC2_PARAM_H
#define __IA_CSS_CTC2_PARAM_H

#define IA_CSS_CTC_COEF_SHIFT          13
#include "vmem.h"  

 

 
struct ia_css_isp_ctc2_vmem_params {
	 
	VMEM_ARRAY(y_x, ISP_VEC_NELEMS);
	 
	VMEM_ARRAY(y_y, ISP_VEC_NELEMS);
	 
	VMEM_ARRAY(e_y_slope, ISP_VEC_NELEMS);
};

 
struct ia_css_isp_ctc2_dmem_params {
	 
	s32 uv_y0;
	s32 uv_y1;

	 
	s32 uv_x0;
	s32 uv_x1;

	 
	s32 uv_dydx;

};
#endif  
