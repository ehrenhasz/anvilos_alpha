#ifndef _INPUT_BUF_ISP_H_
#define _INPUT_BUF_ISP_H_
#include "sh_css_defs.h"
#include "isp_const.h"  
#define INPUT_BUF_HEIGHT	2  
#define INPUT_BUF_LINES		2
#ifndef ENABLE_CONTINUOUS
#define ENABLE_CONTINUOUS 0
#endif
#define EXTRA_INPUT_VECTORS	2  
#define MAX_VECTORS_PER_INPUT_LINE_CONT (CEIL_DIV(SH_CSS_MAX_SENSOR_WIDTH, ISP_NWAY) + EXTRA_INPUT_VECTORS)
#define INPUT_BUF_ADDR 0x0
#endif  
