#ifndef __IA_CSS_XNR_PARAM_H
#define __IA_CSS_XNR_PARAM_H
#include "type_support.h"
#include <system_global.h>
#ifndef PIPE_GENERATION
#define SH_CSS_ISP_XNR_TABLE_SIZE_LOG2       IA_CSS_VAMEM_2_XNR_TABLE_SIZE_LOG2
#define SH_CSS_ISP_XNR_TABLE_SIZE            IA_CSS_VAMEM_2_XNR_TABLE_SIZE
#else
#define SH_CSS_ISP_XNR_TABLE_SIZE 0
#endif
struct sh_css_isp_xnr_vamem_params {
	u16 xnr[SH_CSS_ISP_XNR_TABLE_SIZE];
};
struct sh_css_isp_xnr_params {
	u16 threshold;
};
#endif  
