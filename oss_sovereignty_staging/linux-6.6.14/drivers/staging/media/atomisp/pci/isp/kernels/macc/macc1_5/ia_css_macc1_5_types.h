 
 

#ifndef __IA_CSS_MACC1_5_TYPES_H
#define __IA_CSS_MACC1_5_TYPES_H

 

 

 
#define IA_CSS_MACC_NUM_AXES           16
 
#define IA_CSS_MACC_NUM_COEFS          4

 
struct ia_css_macc1_5_table {
	s16 data[IA_CSS_MACC_NUM_COEFS * IA_CSS_MACC_NUM_AXES];
	 
};

 
struct ia_css_macc1_5_config {
	u8 exp;	 
};

#endif  
