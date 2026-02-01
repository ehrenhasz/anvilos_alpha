 
 

#ifndef __IA_CSS_SC_PARAM_H
#define __IA_CSS_SC_PARAM_H

#include "type_support.h"

 
struct sh_css_isp_sc_params {
	s32 gain_shift;
};

 
#define SH_CSS_SC_INTERPED_GAIN_HOR_SLICE_TIMES   8

struct sh_css_isp_sc_isp_config {
	u32 interped_gain_hor_slice_bqs[SH_CSS_SC_INTERPED_GAIN_HOR_SLICE_TIMES];
	u32 internal_frame_origin_y_bqs_on_sctbl;
};

#endif  
