#ifndef __IA_CSS_CTC_TYPES_H
#define __IA_CSS_CTC_TYPES_H
#include <linux/bitops.h>
#define IA_CSS_CTC_COEF_SHIFT          13
#define IA_CSS_VAMEM_1_CTC_TABLE_SIZE_LOG2      10
#define IA_CSS_VAMEM_1_CTC_TABLE_SIZE           BIT(IA_CSS_VAMEM_1_CTC_TABLE_SIZE_LOG2)
#define IA_CSS_VAMEM_2_CTC_TABLE_SIZE_LOG2      8
#define IA_CSS_VAMEM_2_CTC_TABLE_SIZE           ((1U << IA_CSS_VAMEM_2_CTC_TABLE_SIZE_LOG2) + 1)
enum ia_css_vamem_type {
	IA_CSS_VAMEM_TYPE_1,
	IA_CSS_VAMEM_TYPE_2
};
struct ia_css_ctc_config {
	u16 y0;	 
	u16 y1;	 
	u16 y2;	 
	u16 y3;	 
	u16 y4;	 
	u16 y5;	 
	u16 ce_gain_exp;	 
	u16 x1;	 
	u16 x2;	 
	u16 x3;	 
	u16 x4;	 
};
union ia_css_ctc_data {
	u16 vamem_1[IA_CSS_VAMEM_1_CTC_TABLE_SIZE];
	u16 vamem_2[IA_CSS_VAMEM_2_CTC_TABLE_SIZE];
};
struct ia_css_ctc_table {
	enum ia_css_vamem_type vamem_type;
	union ia_css_ctc_data data;
};
#endif  
