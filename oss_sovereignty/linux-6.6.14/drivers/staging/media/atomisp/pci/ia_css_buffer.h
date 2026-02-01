 
 

#ifndef __IA_CSS_BUFFER_H
#define __IA_CSS_BUFFER_H

 

#include <type_support.h>
#include "ia_css_types.h"
#include "ia_css_timer.h"

 
enum ia_css_buffer_type {
	IA_CSS_BUFFER_TYPE_INVALID = -1,
	IA_CSS_BUFFER_TYPE_3A_STATISTICS = 0,
	IA_CSS_BUFFER_TYPE_DIS_STATISTICS,
	IA_CSS_BUFFER_TYPE_LACE_STATISTICS,
	IA_CSS_BUFFER_TYPE_INPUT_FRAME,
	IA_CSS_BUFFER_TYPE_OUTPUT_FRAME,
	IA_CSS_BUFFER_TYPE_SEC_OUTPUT_FRAME,
	IA_CSS_BUFFER_TYPE_VF_OUTPUT_FRAME,
	IA_CSS_BUFFER_TYPE_SEC_VF_OUTPUT_FRAME,
	IA_CSS_BUFFER_TYPE_RAW_OUTPUT_FRAME,
	IA_CSS_BUFFER_TYPE_CUSTOM_INPUT,
	IA_CSS_BUFFER_TYPE_CUSTOM_OUTPUT,
	IA_CSS_BUFFER_TYPE_METADATA,
	IA_CSS_BUFFER_TYPE_PARAMETER_SET,
	IA_CSS_BUFFER_TYPE_PER_FRAME_PARAMETER_SET,
	IA_CSS_NUM_DYNAMIC_BUFFER_TYPE,
	IA_CSS_NUM_BUFFER_TYPE
};

 

 
struct ia_css_buffer {
	enum ia_css_buffer_type type;  
	unsigned int exp_id;
	 
	union {
		struct ia_css_isp_3a_statistics
			*stats_3a;     
		struct ia_css_isp_dvs_statistics *stats_dvs;    
		struct ia_css_isp_skc_dvs_statistics *stats_skc_dvs;   
		struct ia_css_frame              *frame;        
		struct ia_css_acc_param          *custom_data;  
		struct ia_css_metadata           *metadata;     
	} data;  
	u64 driver_cookie;  
	struct ia_css_time_meas
		timing_data;  
	struct ia_css_clock_tick
		isys_eof_clock_tick;  
};

 
void
ia_css_dequeue_param_buffers(void);

#endif  
