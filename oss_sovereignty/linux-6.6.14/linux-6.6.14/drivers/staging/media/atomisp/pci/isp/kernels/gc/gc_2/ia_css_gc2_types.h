#ifndef __IA_CSS_GC2_TYPES_H
#define __IA_CSS_GC2_TYPES_H
#include "isp/kernels/ctc/ctc_1.0/ia_css_ctc_types.h"   
#define IA_CSS_VAMEM_1_RGB_GAMMA_TABLE_SIZE_LOG2 8
#define IA_CSS_VAMEM_1_RGB_GAMMA_TABLE_SIZE      BIT(IA_CSS_VAMEM_1_RGB_GAMMA_TABLE_SIZE_LOG2)
#define IA_CSS_VAMEM_2_RGB_GAMMA_TABLE_SIZE_LOG2    8
#define IA_CSS_VAMEM_2_RGB_GAMMA_TABLE_SIZE     ((1U << IA_CSS_VAMEM_2_RGB_GAMMA_TABLE_SIZE_LOG2) + 1)
union ia_css_rgb_gamma_data {
	u16 vamem_1[IA_CSS_VAMEM_1_RGB_GAMMA_TABLE_SIZE];
	u16 vamem_2[IA_CSS_VAMEM_2_RGB_GAMMA_TABLE_SIZE];
};
struct ia_css_rgb_gamma_table {
	enum ia_css_vamem_type vamem_type;
	union ia_css_rgb_gamma_data data;
};
#endif  
