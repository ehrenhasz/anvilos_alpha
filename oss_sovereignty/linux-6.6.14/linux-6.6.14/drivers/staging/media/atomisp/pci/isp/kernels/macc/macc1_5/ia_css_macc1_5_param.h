#ifndef __IA_CSS_MACC1_5_PARAM_H
#define __IA_CSS_MACC1_5_PARAM_H
#include "type_support.h"
#include "vmem.h"
#include "ia_css_macc1_5_types.h"
struct sh_css_isp_macc1_5_params {
	s32 exp;
};
struct sh_css_isp_macc1_5_vmem_params {
	VMEM_ARRAY(data, IA_CSS_MACC_NUM_COEFS * ISP_NWAY);
};
#endif  
