#ifndef __IA_CSS_DPC2_TYPES_H
#define __IA_CSS_DPC2_TYPES_H
#include "type_support.h"
#define METRIC1_ONE_FP	BIT(12)
#define METRIC2_ONE_FP	BIT(5)
#define METRIC3_ONE_FP	BIT(12)
#define WBGAIN_ONE_FP	BIT(9)
struct ia_css_dpc2_config {
	s32 metric1;
	s32 metric2;
	s32 metric3;
	s32 wb_gain_gr;
	s32 wb_gain_r;
	s32 wb_gain_b;
	s32 wb_gain_gb;
};
#endif  
