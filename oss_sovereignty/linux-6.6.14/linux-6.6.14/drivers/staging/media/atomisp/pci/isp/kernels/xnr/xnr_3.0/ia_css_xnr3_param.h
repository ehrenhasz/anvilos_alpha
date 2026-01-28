#ifndef __IA_CSS_XNR3_PARAM_H
#define __IA_CSS_XNR3_PARAM_H
#include "type_support.h"
#include "vmem.h"  
#define XNR_ALPHA_SCALE_LOG2        5
#define XNR_ALPHA_SCALE_FACTOR      BIT(XNR_ALPHA_SCALE_LOG2)
#define XNR_CORING_SCALE_LOG2       (ISP_VEC_ELEMBITS - 1)
#define XNR_CORING_SCALE_FACTOR     BIT(XNR_CORING_SCALE_LOG2)
#define XNR_BLENDING_SCALE_LOG2     (ISP_VEC_ELEMBITS - 1)
#define XNR_BLENDING_SCALE_FACTOR   BIT(XNR_BLENDING_SCALE_LOG2)
#define XNR_FILTER_SIZE             5
struct sh_css_xnr3_alpha_params {
	s32 y0;
	s32 u0;
	s32 v0;
	s32 ydiff;
	s32 udiff;
	s32 vdiff;
};
struct sh_css_xnr3_coring_params {
	s32 u0;
	s32 v0;
	s32 udiff;
	s32 vdiff;
};
struct sh_css_xnr3_blending_params {
	s32 strength;
};
struct sh_css_isp_xnr3_params {
	struct sh_css_xnr3_alpha_params    alpha;
	struct sh_css_xnr3_coring_params   coring;
	struct sh_css_xnr3_blending_params blending;
};
struct sh_css_isp_xnr3_vmem_params {
	VMEM_ARRAY(x, ISP_VEC_NELEMS);
	VMEM_ARRAY(a, ISP_VEC_NELEMS);
	VMEM_ARRAY(b, ISP_VEC_NELEMS);
	VMEM_ARRAY(c, ISP_VEC_NELEMS);
};
#endif   
