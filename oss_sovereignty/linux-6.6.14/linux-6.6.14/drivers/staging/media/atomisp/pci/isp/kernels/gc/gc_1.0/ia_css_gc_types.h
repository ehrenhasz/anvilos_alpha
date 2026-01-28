#ifndef __IA_CSS_GC_TYPES_H
#define __IA_CSS_GC_TYPES_H
#include "isp/kernels/ctc/ctc_1.0/ia_css_ctc_types.h"   
#define IA_CSS_GAMMA_GAIN_K_SHIFT      13
#define IA_CSS_VAMEM_1_GAMMA_TABLE_SIZE_LOG2    10
#define IA_CSS_VAMEM_1_GAMMA_TABLE_SIZE         BIT(IA_CSS_VAMEM_1_GAMMA_TABLE_SIZE_LOG2)
#define IA_CSS_VAMEM_2_GAMMA_TABLE_SIZE_LOG2    8
#define IA_CSS_VAMEM_2_GAMMA_TABLE_SIZE         ((1U << IA_CSS_VAMEM_2_GAMMA_TABLE_SIZE_LOG2) + 1)
union ia_css_gc_data {
	u16 vamem_1[IA_CSS_VAMEM_1_GAMMA_TABLE_SIZE];
	u16 vamem_2[IA_CSS_VAMEM_2_GAMMA_TABLE_SIZE];
};
struct ia_css_gamma_table {
	enum ia_css_vamem_type vamem_type;
	union ia_css_gc_data data;
};
struct ia_css_gc_config {
	u16 gain_k1;  
	u16 gain_k2;  
};
struct ia_css_ce_config {
	u8 uv_level_min;  
	u8 uv_level_max;  
};
struct ia_css_macc_config {
	u8 exp;	 
};
#endif  
