 
 

#ifndef __IA_CSS_VF_PARAM_H
#define __IA_CSS_VF_PARAM_H

#include "type_support.h"
#include "dma.h"
#include "gc/gc_1.0/ia_css_gc_param.h"  
#include "ia_css_frame_comm.h"  
#include "ia_css_vf_types.h"

#define VFDEC_BITS_PER_PIXEL	GAMMA_OUTPUT_BITS

 
struct sh_css_isp_vf_isp_config {
	u32 vf_downscale_bits;  
	u32 enable;
	struct ia_css_frame_sp_info info;
	struct {
		u32 width_a_over_b;
		struct dma_port_config port_b;
	} dma;
};

#endif  
