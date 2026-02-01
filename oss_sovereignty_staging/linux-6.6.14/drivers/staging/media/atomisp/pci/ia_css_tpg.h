 
 

#ifndef __IA_CSS_TPG_H
#define __IA_CSS_TPG_H

 

 
enum ia_css_tpg_id {
	IA_CSS_TPG_ID0,
	IA_CSS_TPG_ID1,
	IA_CSS_TPG_ID2
};

 
#define N_CSS_TPG_IDS (IA_CSS_TPG_ID2 + 1)

 
enum ia_css_tpg_mode {
	IA_CSS_TPG_MODE_RAMP,
	IA_CSS_TPG_MODE_CHECKERBOARD,
	IA_CSS_TPG_MODE_FRAME_BASED_COLOR,
	IA_CSS_TPG_MODE_MONO
};

 
struct ia_css_tpg_config {
	enum ia_css_tpg_id   id;
	enum ia_css_tpg_mode mode;
	unsigned int         x_mask;
	int                  x_delta;
	unsigned int         y_mask;
	int                  y_delta;
	unsigned int         xy_mask;
};

#endif  
