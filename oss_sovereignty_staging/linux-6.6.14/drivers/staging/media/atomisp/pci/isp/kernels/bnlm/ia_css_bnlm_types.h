 
 

#ifndef __IA_CSS_BNLM_TYPES_H
#define __IA_CSS_BNLM_TYPES_H

 

#include "type_support.h"  

 
struct ia_css_bnlm_config {
	bool		rad_enable;	 
	s32		rad_x_origin;	 
	s32		rad_y_origin;	 
	 
	s32		avg_min_th;
	 
	s32		max_min_th;

	 
	 
	s32		exp_coeff_a;
	u32	exp_coeff_b;
	s32		exp_coeff_c;
	u32	exp_exponent;
	 

	s32 nl_th[3];	 

	 
	s32 match_quality_max_idx[4];

	 
	 
	s32 mu_root_lut_thr[15];
	s32 mu_root_lut_val[16];
	 
	 
	 
	s32 sad_norm_lut_thr[15];
	s32 sad_norm_lut_val[16];
	 
	 
	 
	s32 sig_detail_lut_thr[15];
	s32 sig_detail_lut_val[16];
	 
	 
	 
	s32 sig_rad_lut_thr[15];
	s32 sig_rad_lut_val[16];
	 
	 
	 
	s32 rad_pow_lut_thr[15];
	s32 rad_pow_lut_val[16];
	 
	 
	 
	 
	 
	s32 nl_0_lut_thr[15];
	s32 nl_0_lut_val[16];
	 
	 
	 
	s32 nl_1_lut_thr[15];
	s32 nl_1_lut_val[16];
	 
	 
	 
	s32 nl_2_lut_thr[15];
	s32 nl_2_lut_val[16];
	 
	 
	 
	s32 nl_3_lut_thr[15];
	s32 nl_3_lut_val[16];
	 
	 
};

#endif  
