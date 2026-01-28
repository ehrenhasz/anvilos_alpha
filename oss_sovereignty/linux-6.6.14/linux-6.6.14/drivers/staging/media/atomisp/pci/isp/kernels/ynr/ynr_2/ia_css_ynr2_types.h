#ifndef __IA_CSS_YNR2_TYPES_H
#define __IA_CSS_YNR2_TYPES_H
struct ia_css_ynr_config {
	u16 edge_sense_gain_0;    
	u16 edge_sense_gain_1;    
	u16 corner_sense_gain_0;  
	u16 corner_sense_gain_1;  
};
struct ia_css_fc_config {
	u8  gain_exp;    
	u16 coring_pos_0;  
	u16 coring_pos_1;  
	u16 coring_neg_0;  
	u16 coring_neg_1;  
	u16 gain_pos_0;  
	u16 gain_pos_1;  
	u16 gain_neg_0;  
	u16 gain_neg_1;  
	u16 crop_pos_0;  
	u16 crop_pos_1;  
	s16  crop_neg_0;  
	s16  crop_neg_1;  
};
#endif  
