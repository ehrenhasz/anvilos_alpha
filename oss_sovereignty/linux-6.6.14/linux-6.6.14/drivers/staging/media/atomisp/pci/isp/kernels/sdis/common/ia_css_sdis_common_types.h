#ifndef __IA_CSS_SDIS_COMMON_TYPES_H
#define __IA_CSS_SDIS_COMMON_TYPES_H
#include <type_support.h>
struct ia_css_dvs_grid_dim {
	u32 width;		 
	u32 height;	 
};
struct ia_css_sdis_info {
	struct {
		struct ia_css_dvs_grid_dim dim;  
		struct ia_css_dvs_grid_dim pad;  
	} grid, coef, proj;
	u32 deci_factor_log2;
};
struct ia_css_dvs_grid_res {
	u32 width;		 
	u32 aligned_width;  
	u32 height;	 
	u32 aligned_height; 
};
struct ia_css_dvs_grid_info {
	u32 enable;         
	u32 width;		 
	u32 aligned_width;  
	u32 height;	 
	u32 aligned_height; 
	u32 bqs_per_grid_cell;  
	u32 num_hor_coefs;	 
	u32 num_ver_coefs;	 
};
#define IA_CSS_DVS_STAT_NUM_OF_LEVELS	3
struct dvs_stat_public_dvs_global_cfg {
	unsigned char kappa;
	unsigned char match_shift;
	unsigned char ybin_mode;
};
struct dvs_stat_public_dvs_level_grid_cfg {
	unsigned char grid_width;
	unsigned char grid_height;
	unsigned char block_width;
	unsigned char block_height;
};
struct dvs_stat_public_dvs_level_grid_start {
	unsigned short x_start;
	unsigned short y_start;
	unsigned char enable;
};
struct dvs_stat_public_dvs_level_grid_end {
	unsigned short x_end;
	unsigned short y_end;
};
struct dvs_stat_public_dvs_level_fe_roi_cfg {
	unsigned char x_start;
	unsigned char y_start;
	unsigned char x_end;
	unsigned char y_end;
};
struct dvs_stat_public_dvs_grd_cfg {
	struct dvs_stat_public_dvs_level_grid_cfg    grd_cfg;
	struct dvs_stat_public_dvs_level_grid_start  grd_start;
	struct dvs_stat_public_dvs_level_grid_end    grd_end;
};
struct ia_css_dvs_stat_grid_info {
	struct dvs_stat_public_dvs_global_cfg       dvs_gbl_cfg;
	struct dvs_stat_public_dvs_grd_cfg       grd_cfg[IA_CSS_DVS_STAT_NUM_OF_LEVELS];
	struct dvs_stat_public_dvs_level_fe_roi_cfg
		fe_roi_cfg[IA_CSS_DVS_STAT_NUM_OF_LEVELS];
};
#define DEFAULT_DVS_GRID_INFO { \
	.dvs_stat_grid_info = { \
		.fe_roi_cfg = { \
			[1] = { \
			    .x_start = 4 \
			} \
		} \
	} \
}
union ia_css_dvs_grid_u {
	struct ia_css_dvs_stat_grid_info dvs_stat_grid_info;
	struct ia_css_dvs_grid_info dvs_grid_info;
};
#endif  
