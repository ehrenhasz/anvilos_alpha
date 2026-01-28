#ifndef __IA_CSS_HB_PARAM_H
#define __IA_CSS_HB_PARAM_H
#include "type_support.h"
#ifndef PIPE_GENERATION
#define __INLINE_HMEM__
#include "hmem.h"
#endif
#include "ia_css_bh_types.h"
struct sh_css_isp_bh_params {
	s32 y_coef_r;
	s32 y_coef_g;
	s32 y_coef_b;
};
struct sh_css_isp_bh_hmem_params {
	u32 bh[ISP_HIST_COMPONENTS][IA_CSS_HMEM_BH_UNIT_SIZE];
};
#endif  
