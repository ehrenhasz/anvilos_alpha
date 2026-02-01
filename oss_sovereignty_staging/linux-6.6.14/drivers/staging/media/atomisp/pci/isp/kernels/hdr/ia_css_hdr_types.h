 
 
 

#ifndef __IA_CSS_HDR_TYPES_H
#define __IA_CSS_HDR_TYPES_H

#define IA_CSS_HDR_MAX_NUM_INPUT_FRAMES         (3)

 
struct ia_css_hdr_irradiance_params {
	int test_irr;                                           
	int match_shift[IA_CSS_HDR_MAX_NUM_INPUT_FRAMES -
							1];   
	int match_mul[IA_CSS_HDR_MAX_NUM_INPUT_FRAMES -
						      1];     
	int thr_low[IA_CSS_HDR_MAX_NUM_INPUT_FRAMES -
						    1];       
	int thr_high[IA_CSS_HDR_MAX_NUM_INPUT_FRAMES -
						     1];      
	int thr_coeff[IA_CSS_HDR_MAX_NUM_INPUT_FRAMES -
						      1];     
	int thr_shift[IA_CSS_HDR_MAX_NUM_INPUT_FRAMES -
						      1];     
	int weight_bpp;                                         
};

 
struct ia_css_hdr_deghost_params {
	int test_deg;  
};

 
struct ia_css_hdr_exclusion_params {
	int test_excl;  
};

 
struct ia_css_hdr_config {
	struct ia_css_hdr_irradiance_params irradiance;  
	struct ia_css_hdr_deghost_params    deghost;     
	struct ia_css_hdr_exclusion_params  exclusion;  
};

#endif  
