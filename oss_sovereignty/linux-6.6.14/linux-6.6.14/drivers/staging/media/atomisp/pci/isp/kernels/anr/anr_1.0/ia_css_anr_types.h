#ifndef __IA_CSS_ANR_TYPES_H
#define __IA_CSS_ANR_TYPES_H
#define ANR_BPP                 10
#define ANR_ELEMENT_BITS        ((CEIL_DIV(ANR_BPP, 8)) * 8)
struct ia_css_anr_config {
	s32 threshold;  
	s32 thresholds[4 * 4 * 4];
	s32 factors[3];
};
#endif  
