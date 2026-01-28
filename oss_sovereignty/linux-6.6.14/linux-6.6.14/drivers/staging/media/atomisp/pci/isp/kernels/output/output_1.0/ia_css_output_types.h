#ifndef __IA_CSS_OUTPUT_TYPES_H
#define __IA_CSS_OUTPUT_TYPES_H
struct ia_css_frame_info;
struct ia_css_output_configuration {
	const struct ia_css_frame_info *info;
};
struct ia_css_output0_configuration {
	const struct ia_css_frame_info *info;
};
struct ia_css_output1_configuration {
	const struct ia_css_frame_info *info;
};
struct ia_css_output_config {
	u8 enable_hflip;   
	u8 enable_vflip;   
};
#endif  
