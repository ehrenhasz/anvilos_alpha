 
 

#ifndef __IA_CSS_YNR_TYPES_H
#define __IA_CSS_YNR_TYPES_H

 

 
struct ia_css_nr_config {
	ia_css_u0_16 bnr_gain;	    
	ia_css_u0_16 ynr_gain;	    
	ia_css_u0_16 direction;     
	ia_css_u0_16 threshold_cb;  
	ia_css_u0_16 threshold_cr;  
};

 
struct ia_css_ee_config {
	ia_css_u5_11 gain;	   
	ia_css_u8_8 threshold;     
	ia_css_u5_11 detail_gain;  
};

 
struct ia_css_yee_config {
	struct ia_css_nr_config nr;  
	struct ia_css_ee_config ee;  
};

#endif  
