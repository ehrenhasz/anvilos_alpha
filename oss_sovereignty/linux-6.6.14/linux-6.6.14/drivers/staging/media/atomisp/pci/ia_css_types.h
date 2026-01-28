#ifndef _IA_CSS_TYPES_H
#define _IA_CSS_TYPES_H
#include <type_support.h>
#include "ia_css_frac.h"
#include "isp/kernels/aa/aa_2/ia_css_aa2_types.h"
#include "isp/kernels/anr/anr_1.0/ia_css_anr_types.h"
#include "isp/kernels/anr/anr_2/ia_css_anr2_types.h"
#include "isp/kernels/cnr/cnr_2/ia_css_cnr2_types.h"
#include "isp/kernels/csc/csc_1.0/ia_css_csc_types.h"
#include "isp/kernels/ctc/ctc_1.0/ia_css_ctc_types.h"
#include "isp/kernels/dp/dp_1.0/ia_css_dp_types.h"
#include "isp/kernels/de/de_1.0/ia_css_de_types.h"
#include "isp/kernels/de/de_2/ia_css_de2_types.h"
#include "isp/kernels/fc/fc_1.0/ia_css_formats_types.h"
#include "isp/kernels/fpn/fpn_1.0/ia_css_fpn_types.h"
#include "isp/kernels/gc/gc_1.0/ia_css_gc_types.h"
#include "isp/kernels/gc/gc_2/ia_css_gc2_types.h"
#include "isp/kernels/macc/macc_1.0/ia_css_macc_types.h"
#include "isp/kernels/ob/ob_1.0/ia_css_ob_types.h"
#include "isp/kernels/s3a/s3a_1.0/ia_css_s3a_types.h"
#include "isp/kernels/sc/sc_1.0/ia_css_sc_types.h"
#include "isp/kernels/sdis/sdis_1.0/ia_css_sdis_types.h"
#include "isp/kernels/sdis/sdis_2/ia_css_sdis2_types.h"
#include "isp/kernels/tnr/tnr_1.0/ia_css_tnr_types.h"
#include "isp/kernels/wb/wb_1.0/ia_css_wb_types.h"
#include "isp/kernels/xnr/xnr_1.0/ia_css_xnr_types.h"
#include "isp/kernels/xnr/xnr_3.0/ia_css_xnr3_types.h"
#include "isp/kernels/tnr/tnr3/ia_css_tnr3_types.h"
#include "isp/kernels/ynr/ynr_1.0/ia_css_ynr_types.h"
#include "isp/kernels/ynr/ynr_2/ia_css_ynr2_types.h"
#include "isp/kernels/output/output_1.0/ia_css_output_types.h"
#define IA_CSS_DVS_STAT_GRID_INFO_SUPPORTED
#define IA_CSS_VERSION_MAJOR    2
#define IA_CSS_VERSION_MINOR    0
#define IA_CSS_VERSION_REVISION 2
#define IA_CSS_MORPH_TABLE_NUM_PLANES  6
#define IA_CSS_ISYS_MIN_EXPOSURE_ID 1    
#define IA_CSS_ISYS_MAX_EXPOSURE_ID 250  
struct ia_css_isp_parameters;
struct ia_css_pipe;
struct ia_css_memory_offsets;
struct ia_css_config_memory_offsets;
struct ia_css_state_memory_offsets;
typedef u32 ia_css_ptr;
struct ia_css_resolution {
	u32 width;   
	u32 height;  
};
struct ia_css_coordinate {
	s32 x;	 
	s32 y;	 
};
struct ia_css_vector {
	s32 x;  
	s32 y;  
};
#define IA_CSS_ISP_DMEM IA_CSS_ISP_DMEM0
#define IA_CSS_ISP_VMEM IA_CSS_ISP_VMEM0
struct ia_css_data {
	ia_css_ptr address;  
	u32   size;     
};
struct ia_css_host_data {
	char      *address;  
	u32   size;     
};
struct ia_css_isp_data {
	u32   address;  
	u32   size;     
};
enum ia_css_shading_correction_type {
	IA_CSS_SHADING_CORRECTION_NONE,	  
	IA_CSS_SHADING_CORRECTION_TYPE_1  
};
struct ia_css_shading_info {
	enum ia_css_shading_correction_type type;  
	union {	 
		struct {
			u32 enable;	 
			u32 num_hor_grids;	 
			u32 num_ver_grids;	 
			u32 bqs_per_grid_cell;  
			u32 bayer_scale_hor_ratio_in;
			u32 bayer_scale_hor_ratio_out;
			u32 bayer_scale_ver_ratio_in;
			u32 bayer_scale_ver_ratio_out;
			u32 sc_bayer_origin_x_bqs_on_shading_table;
			u32 sc_bayer_origin_y_bqs_on_shading_table;
			struct ia_css_resolution isp_input_sensor_data_res_bqs;
			struct ia_css_resolution sensor_data_res_bqs;
			struct ia_css_coordinate sensor_data_origin_bqs_on_sctbl;
		} type_1;
	} info;
};
#define DEFAULT_SHADING_INFO_TYPE_1 \
(struct ia_css_shading_info) { \
	.type = IA_CSS_SHADING_CORRECTION_TYPE_1, \
	.info = { \
		.type_1 = { \
			.bayer_scale_hor_ratio_in	= 1, \
			.bayer_scale_hor_ratio_out	= 1, \
			.bayer_scale_ver_ratio_in	= 1, \
			.bayer_scale_ver_ratio_out	= 1, \
		} \
	} \
}
#define DEFAULT_SHADING_INFO	DEFAULT_SHADING_INFO_TYPE_1
struct ia_css_grid_info {
	u32 isp_in_width;
	u32 isp_in_height;
	struct ia_css_3a_grid_info  s3a_grid;  
	union ia_css_dvs_grid_u dvs_grid;
	enum ia_css_vamem_type vamem_type;
};
#define DEFAULT_GRID_INFO { \
	.dvs_grid	= DEFAULT_DVS_GRID_INFO, \
	.vamem_type	= IA_CSS_VAMEM_TYPE_1 \
}
struct ia_css_morph_table {
	u32 enable;  
	u32 height;  
	u32 width;   
	u16 *coordinates_x[IA_CSS_MORPH_TABLE_NUM_PLANES];
	u16 *coordinates_y[IA_CSS_MORPH_TABLE_NUM_PLANES];
};
struct ia_css_dvs_6axis_config {
	unsigned int exp_id;
	u32 width_y;
	u32 height_y;
	u32 width_uv;
	u32 height_uv;
	u32 *xcoords_y;
	u32 *ycoords_y;
	u32 *xcoords_uv;
	u32 *ycoords_uv;
};
struct ia_css_point {
	s32 x;  
	s32 y;  
};
struct ia_css_region {
	struct ia_css_point origin;  
	struct ia_css_resolution resolution;  
};
struct ia_css_dz_config {
	u32 dx;  
	u32 dy;  
	struct ia_css_region zoom_region;  
};
enum ia_css_capture_mode {
	IA_CSS_CAPTURE_MODE_RAW,       
	IA_CSS_CAPTURE_MODE_BAYER,     
	IA_CSS_CAPTURE_MODE_PRIMARY,   
	IA_CSS_CAPTURE_MODE_ADVANCED,  
	IA_CSS_CAPTURE_MODE_LOW_LIGHT  
};
struct ia_css_capture_config {
	enum ia_css_capture_mode mode;  
	u32 enable_xnr;	        
	u32 enable_raw_output;
	bool enable_capture_pp_bli;     
};
#define DEFAULT_CAPTURE_CONFIG { \
	.mode	= IA_CSS_CAPTURE_MODE_PRIMARY, \
}
struct ia_css_isp_config {
	struct ia_css_wb_config   *wb_config;	 
	struct ia_css_cc_config   *cc_config;	 
	struct ia_css_tnr_config  *tnr_config;	 
	struct ia_css_ecd_config  *ecd_config;	 
	struct ia_css_ynr_config  *ynr_config;	 
	struct ia_css_fc_config   *fc_config;	 
	struct ia_css_formats_config
		*formats_config;	 
	struct ia_css_cnr_config  *cnr_config;	 
	struct ia_css_macc_config *macc_config;	 
	struct ia_css_ctc_config  *ctc_config;	 
	struct ia_css_aa_config   *aa_config;	 
	struct ia_css_aa_config   *baa_config;	 
	struct ia_css_ce_config   *ce_config;	 
	struct ia_css_dvs_6axis_config *dvs_6axis_config;
	struct ia_css_ob_config   *ob_config;   
	struct ia_css_dp_config   *dp_config;   
	struct ia_css_nr_config   *nr_config;   
	struct ia_css_ee_config   *ee_config;   
	struct ia_css_de_config   *de_config;   
	struct ia_css_gc_config   *gc_config;   
	struct ia_css_anr_config  *anr_config;  
	struct ia_css_3a_config   *s3a_config;  
	struct ia_css_xnr_config  *xnr_config;  
	struct ia_css_dz_config   *dz_config;   
	struct ia_css_cc_config *yuv2rgb_cc_config;  
	struct ia_css_cc_config *rgb2yuv_cc_config;  
	struct ia_css_macc_table  *macc_table;	 
	struct ia_css_gamma_table *gamma_table;	 
	struct ia_css_ctc_table   *ctc_table;	 
	struct ia_css_xnr_table   *xnr_table;	 
	struct ia_css_rgb_gamma_table *r_gamma_table; 
	struct ia_css_rgb_gamma_table *g_gamma_table; 
	struct ia_css_rgb_gamma_table *b_gamma_table; 
	struct ia_css_vector      *motion_vector;  
	struct ia_css_shading_table *shading_table;
	struct ia_css_morph_table   *morph_table;
	struct ia_css_dvs_coefficients *dvs_coefs;  
	struct ia_css_dvs2_coefficients *dvs2_coefs;  
	struct ia_css_capture_config   *capture_config;
	struct ia_css_anr_thres   *anr_thres;
	struct ia_css_shading_settings *shading_settings;
	struct ia_css_xnr3_config *xnr3_config;  
	struct ia_css_output_config
		*output_config;	 
	struct ia_css_scaler_config
		*scaler_config;          
	struct ia_css_formats_config
		*formats_config_display; 
	struct ia_css_output_config
		*output_config_display;  
	struct ia_css_frame
		*output_frame;           
	u32			isp_config_id;	 
};
#endif  
