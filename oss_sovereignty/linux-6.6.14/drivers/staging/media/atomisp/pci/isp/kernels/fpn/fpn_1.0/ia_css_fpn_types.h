 
 

#ifndef __IA_CSS_FPN_TYPES_H
#define __IA_CSS_FPN_TYPES_H

 

 

struct ia_css_fpn_table {
	s16 *data;		 
	u32 width;		 
	u32 height;	 
	u32 shift;		 
	u32 enabled;	 
};

struct ia_css_fpn_configuration {
	const struct ia_css_frame_info *info;
};

#endif  
