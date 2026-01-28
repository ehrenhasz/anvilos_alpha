#ifndef __IA_CSS_SC_TYPES_H
#define __IA_CSS_SC_TYPES_H
#define IA_CSS_SC_NUM_COLORS           4
enum ia_css_sc_color {
	IA_CSS_SC_COLOR_GR,  
	IA_CSS_SC_COLOR_R,   
	IA_CSS_SC_COLOR_B,   
	IA_CSS_SC_COLOR_GB   
};
struct ia_css_shading_table {
	u32 enable;  
	u32 sensor_width;   
	u32 sensor_height;  
	u32 width;   
	u32 height;  
	u32 fraction_bits;  
	u16 *data[IA_CSS_SC_NUM_COLORS];
};
struct ia_css_shading_settings {
	u32 enable_shading_table_conversion;  
};
#endif  
