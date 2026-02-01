 
 

#ifndef __IA_CSS_RAW_PARAM_H
#define __IA_CSS_RAW_PARAM_H

#include "type_support.h"

#include "dma.h"

 
struct sh_css_isp_raw_isp_config {
	u32 width_a_over_b;
	struct dma_port_config port_b;
	u32 inout_port_config;
	u32 input_needs_raw_binning;
	u32 format;  
	u32 required_bds_factor;
	u32 two_ppc;
	u32 stream_format;  
	u32 deinterleaved;
	u32 start_column;  
	u32 start_line;  
	u8 enable_left_padding;  
};

#endif  
