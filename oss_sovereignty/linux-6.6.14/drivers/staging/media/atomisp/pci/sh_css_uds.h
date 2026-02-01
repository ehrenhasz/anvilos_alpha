 
 

#ifndef _SH_CSS_UDS_H_
#define _SH_CSS_UDS_H_

#include <type_support.h>

#define SIZE_OF_SH_CSS_UDS_INFO_IN_BITS (4 * 16)
#define SIZE_OF_SH_CSS_CROP_POS_IN_BITS (2 * 16)

 

struct sh_css_uds_info {
	u16 curr_dx;
	u16 curr_dy;
	u16 xc;
	u16 yc;
};

struct sh_css_crop_pos {
	u16 x;
	u16 y;
};

#endif  
