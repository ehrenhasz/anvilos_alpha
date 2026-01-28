#ifndef __IA_CSS_S3A_TYPES_H
#define __IA_CSS_S3A_TYPES_H
#include <ia_css_frac.h>
#if (defined(SYSTEM_css_skycam_c0_system)) && (!defined(PIPE_GENERATION))
#include "../../../../components/stats_3a/src/stats_3a_public.h"
#endif
struct ia_css_3a_grid_info {
#if defined(SYSTEM_css_skycam_c0_system)
	u32 ae_enable;					 
	struct ae_public_config_grid_config
		ae_grd_info;	 
	u32 awb_enable;					 
	struct awb_public_config_grid_config
		awb_grd_info;	 
	u32 af_enable;					 
	struct af_public_grid_config		af_grd_info;	 
	u32 awb_fr_enable;					 
	struct awb_fr_public_grid_config
		awb_fr_grd_info; 
	u32 elem_bit_depth;     
#else
	u32 enable;             
	u32 use_dmem;           
	u32 has_histogram;      
	u32 width;		     
	u32 height;	     
	u32 aligned_width;      
	u32 aligned_height;     
	u32 bqs_per_grid_cell;  
	u32 deci_factor_log2;   
	u32 elem_bit_depth;     
#endif
};
struct ia_css_3a_config {
	ia_css_u0_16 ae_y_coef_r;	 
	ia_css_u0_16 ae_y_coef_g;	 
	ia_css_u0_16 ae_y_coef_b;	 
	ia_css_u0_16 awb_lg_high_raw;	 
	ia_css_u0_16 awb_lg_low;	 
	ia_css_u0_16 awb_lg_high;	 
	ia_css_s0_15 af_fir1_coef[7];	 
	ia_css_s0_15 af_fir2_coef[7];	 
};
struct ia_css_3a_output {
	s32 ae_y;     
	s32 awb_cnt;  
	s32 awb_gr;   
	s32 awb_r;    
	s32 awb_b;    
	s32 awb_gb;   
	s32 af_hpf1;  
	s32 af_hpf2;  
};
struct ia_css_3a_statistics {
	struct ia_css_3a_grid_info
		grid;	 
	struct ia_css_3a_output
		*data;	 
	struct ia_css_3a_rgby_output *rgby_data; 
};
struct ia_css_3a_rgby_output {
	u32 r;	 
	u32 g;	 
	u32 b;	 
	u32 y;	 
};
#endif  
