 

#ifndef __DML32_DISPLAY_MODE_VBA_H__
#define __DML32_DISPLAY_MODE_VBA_H__

#include "../display_mode_enums.h"








#define __DML_VBA_MIN_VSTARTUP__    9


#define __DML_ARB_TO_RET_DELAY__    7 + 95


#define __DML_MIN_DCFCLK_FACTOR__   1.15


#define __DML_MAX_VRATIO_PRE__ 7.9
#define __DML_MAX_BW_RATIO_PRE__ 4.0

#define __DML_VBA_MAX_DST_Y_PRE__    63.75

#define BPP_INVALID 0
#define BPP_BLENDED_PIPE 0xffffffff

#define MEM_STROBE_FREQ_MHZ 1600
#define DCFCLK_FREQ_EXTRA_PREFETCH_REQ_MHZ 300
#define MEM_STROBE_MAX_DELIVERY_TIME_US 60.0

struct display_mode_lib;

void dml32_ModeSupportAndSystemConfigurationFull(struct display_mode_lib *mode_lib);
void dml32_recalculate(struct display_mode_lib *mode_lib);

#endif
