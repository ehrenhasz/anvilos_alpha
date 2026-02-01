 
 

#ifndef __IA_CSS_S3A_PARAM_H
#define __IA_CSS_S3A_PARAM_H

#include "type_support.h"

 
struct sh_css_isp_ae_params {
	 
	s32 y_coef_r;
	s32 y_coef_g;
	s32 y_coef_b;
};

 
struct sh_css_isp_awb_params {
	s32 lg_high_raw;
	s32 lg_low;
	s32 lg_high;
};

 
struct sh_css_isp_af_params {
	s32 fir1[7];
	s32 fir2[7];
};

 
struct sh_css_isp_s3a_params {
	 
	struct sh_css_isp_ae_params ae;

	 
	struct sh_css_isp_awb_params awb;

	 
	struct sh_css_isp_af_params af;
};

#endif  
