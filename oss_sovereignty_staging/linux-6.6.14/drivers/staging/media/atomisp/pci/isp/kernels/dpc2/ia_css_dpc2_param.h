 
 

#ifndef __IA_CSS_DPC2_PARAM_H
#define __IA_CSS_DPC2_PARAM_H

#include "type_support.h"
#include "vmem.h"  

 
#define NUM_PLANES		4

 
#define MAX_FRAME_SIMDWIDTH	30

 
#define DPC2_STATE_INPUT_BUFFER_HEIGHT	(3 * NUM_PLANES)
 
#define DPC2_STATE_INPUT_BUFFER_WIDTH	CEIL_DIV(MAX_FRAME_SIMDWIDTH, 2)

 
#define DPC2_STATE_LOCAL_DEVIATION_BUFFER_HEIGHT	(1 * NUM_PLANES)
 
#define DPC2_STATE_LOCAL_DEVIATION_BUFFER_WIDTH		CEIL_DIV(MAX_FRAME_SIMDWIDTH, 2)

 
#define DPC2_STATE_SECOND_MINMAX_BUFFER_HEIGHT	1
#define DPC2_STATE_SECOND_MINMAX_BUFFER_WIDTH	MAX_FRAME_SIMDWIDTH

struct ia_css_isp_dpc2_params {
	s32 metric1;
	s32 metric2;
	s32 metric3;
	s32 wb_gain_gr;
	s32 wb_gain_r;
	s32 wb_gain_b;
	s32 wb_gain_gb;
};

#endif  
