 
 

#ifndef _IA_CSS_ISP_PARAM_TYPES_H_
#define _IA_CSS_ISP_PARAM_TYPES_H_

#include "ia_css_types.h"
#include <platform_support.h>
#include <system_global.h>

 
#define IA_CSS_ISP_DMEM IA_CSS_ISP_DMEM0
#define IA_CSS_ISP_VMEM IA_CSS_ISP_VMEM0

 
#define IA_CSS_NUM_ISP_MEMORIES IA_CSS_NUM_MEMORIES

 
enum ia_css_param_class {
	IA_CSS_PARAM_CLASS_PARAM  = 0,	 
	IA_CSS_PARAM_CLASS_CONFIG = 1,	 
	IA_CSS_PARAM_CLASS_STATE  = 2,   
#if 0  
	IA_CSS_PARAM_CLASS_FRAME  = 3,   
#endif
};

#define IA_CSS_NUM_PARAM_CLASSES (IA_CSS_PARAM_CLASS_STATE + 1)

 
struct ia_css_isp_parameter {
	u32 offset;  
	u32 size;    
};

 
struct ia_css_isp_param_host_segments {
	struct ia_css_host_data params[IA_CSS_NUM_PARAM_CLASSES][IA_CSS_NUM_MEMORIES];
};

 
struct ia_css_isp_param_css_segments {
	struct ia_css_data      params[IA_CSS_NUM_PARAM_CLASSES][IA_CSS_NUM_MEMORIES];
};

 
struct ia_css_isp_param_isp_segments {
	struct ia_css_isp_data  params[IA_CSS_NUM_PARAM_CLASSES][IA_CSS_NUM_MEMORIES];
};

 
struct ia_css_isp_param_memory_offsets {
	u32 offsets[IA_CSS_NUM_PARAM_CLASSES];   
};

 
union ia_css_all_memory_offsets {
	struct {
		CSS_ALIGN(struct ia_css_memory_offsets	      *param, 8);
		CSS_ALIGN(struct ia_css_config_memory_offsets *config, 8);
		CSS_ALIGN(struct ia_css_state_memory_offsets  *state, 8);
	} offsets;
	struct {
		CSS_ALIGN(void *ptr, 8);
	} array[IA_CSS_NUM_PARAM_CLASSES];
};

#endif  
