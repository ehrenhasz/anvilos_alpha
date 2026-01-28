#ifndef __IA_CSS_HDR_PARAMS_H
#define __IA_CSS_HDR_PARAMS_H
#include "type_support.h"
#define HDR_NUM_INPUT_FRAMES         (3)
struct sh_css_hdr_irradiance_params {
	s32 test_irr;
	s32 match_shift[HDR_NUM_INPUT_FRAMES -
					     1];   
	s32 match_mul[HDR_NUM_INPUT_FRAMES -
					   1];     
	s32 thr_low[HDR_NUM_INPUT_FRAMES -
					 1];       
	s32 thr_high[HDR_NUM_INPUT_FRAMES -
					  1];      
	s32 thr_coeff[HDR_NUM_INPUT_FRAMES -
					   1];     
	s32 thr_shift[HDR_NUM_INPUT_FRAMES -
					   1];     
	s32 weight_bpp;                              
};
struct sh_css_hdr_deghost_params {
	s32 test_deg;
};
struct sh_css_hdr_exclusion_params {
	s32 test_excl;
};
struct sh_css_isp_hdr_params {
	struct sh_css_hdr_irradiance_params irradiance;
	struct sh_css_hdr_deghost_params    deghost;
	struct sh_css_hdr_exclusion_params  exclusion;
};
#endif  
