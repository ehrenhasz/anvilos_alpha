#ifndef __IA_CSS_XNR3_TYPES_H
#define __IA_CSS_XNR3_TYPES_H
#define IA_CSS_XNR3_SIGMA_SCALE  BIT(10)
#define IA_CSS_XNR3_CORING_SCALE BIT(15)
#define IA_CSS_XNR3_BLENDING_SCALE BIT(11)
struct ia_css_xnr3_sigma_params {
	int y0;      
	int y1;      
	int u0;      
	int u1;      
	int v0;      
	int v1;      
};
struct ia_css_xnr3_coring_params {
	int u0;      
	int u1;      
	int v0;      
	int v1;      
};
struct ia_css_xnr3_blending_params {
	int strength;    
};
struct ia_css_xnr3_config {
	struct ia_css_xnr3_sigma_params    sigma;     
	struct ia_css_xnr3_coring_params   coring;    
	struct ia_css_xnr3_blending_params blending;  
};
#endif  
