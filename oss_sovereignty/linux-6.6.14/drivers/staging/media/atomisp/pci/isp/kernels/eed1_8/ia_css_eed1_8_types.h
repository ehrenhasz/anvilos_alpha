 
 

#ifndef __IA_CSS_EED1_8_TYPES_H
#define __IA_CSS_EED1_8_TYPES_H

 

#include "type_support.h"

 

 

 
#define IA_CSS_NUMBER_OF_DEW_ENHANCE_SEGMENTS	9

 

struct ia_css_eed1_8_config {
	s32 rbzp_strength;	 

	s32 fcstrength;	 
	s32 fcthres_0;	 
	s32 fcthres_1;	 
	s32 fc_sat_coef;	 
	s32 fc_coring_prm;	 

	s32 aerel_thres0;	 
	s32 aerel_gain0;	 
	s32 aerel_thres1;	 
	s32 aerel_gain1;	 

	s32 derel_thres0;	 
	s32 derel_gain0;	 
	s32 derel_thres1;	 
	s32 derel_gain1;	 

	s32 coring_pos0;	 
	s32 coring_pos1;	 
	s32 coring_neg0;	 
	s32 coring_neg1;	 

	s32 gain_exp;	 
	s32 gain_pos0;	 
	s32 gain_pos1;	 
	s32 gain_neg0;	 
	s32 gain_neg1;	 

	s32 pos_margin0;	 
	s32 pos_margin1;	 
	s32 neg_margin0;	 
	s32 neg_margin1;	 

	s32 dew_enhance_seg_x[IA_CSS_NUMBER_OF_DEW_ENHANCE_SEGMENTS];		 
	s32 dew_enhance_seg_y[IA_CSS_NUMBER_OF_DEW_ENHANCE_SEGMENTS];		 
	s32 dew_enhance_seg_slope[(IA_CSS_NUMBER_OF_DEW_ENHANCE_SEGMENTS -
				   1)];	 
	s32 dew_enhance_seg_exp[(IA_CSS_NUMBER_OF_DEW_ENHANCE_SEGMENTS -
				 1)];	 
	s32 dedgew_max;	 
};

#endif  
