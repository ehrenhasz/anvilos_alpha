#ifndef __IA_CSS_UDS_PARAM_H
#define __IA_CSS_UDS_PARAM_H
#include "sh_css_uds.h"
struct ia_css_uds_config {
	struct sh_css_crop_pos crop_pos;
	struct sh_css_uds_info uds;
};
struct sh_css_sp_uds_params {
	struct sh_css_crop_pos crop_pos;
	struct sh_css_uds_info uds;
};
#endif  
