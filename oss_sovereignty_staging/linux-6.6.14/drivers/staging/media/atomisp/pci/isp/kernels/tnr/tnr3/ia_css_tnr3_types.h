 
 

#ifndef _IA_CSS_TNR3_TYPES_H
#define _IA_CSS_TNR3_TYPES_H

 

 
#define TNR3_NUM_SEGMENTS    3

 
struct ia_css_tnr3_kernel_config {
	unsigned int maxfb_y;                         
	unsigned int maxfb_u;                         
	unsigned int maxfb_v;                         
	unsigned int round_adj_y;                     
	unsigned int round_adj_u;                     
	unsigned int round_adj_v;                     
	unsigned int knee_y[TNR3_NUM_SEGMENTS - 1];   
	unsigned int sigma_y[TNR3_NUM_SEGMENTS +
					       1];  
	unsigned int sigma_u[TNR3_NUM_SEGMENTS +
					       1];  
	unsigned int sigma_v[TNR3_NUM_SEGMENTS +
					       1];  
	unsigned int
	ref_buf_select;                  
};

#endif
