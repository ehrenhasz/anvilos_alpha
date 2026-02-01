 
 
#ifndef __ATOMISP_COMPAT_IOCTL32_H__
#define __ATOMISP_COMPAT_IOCTL32_H__

#include <linux/compat.h>
#include <linux/videodev2.h>

#include "atomisp_compat.h"

struct atomisp_histogram32 {
	unsigned int num_elements;
	compat_uptr_t data;
};

struct atomisp_dvs2_stat_types32 {
	compat_uptr_t odd_real;  
	compat_uptr_t odd_imag;  
	compat_uptr_t even_real; 
	compat_uptr_t even_imag; 
};

struct atomisp_dvs2_coef_types32 {
	compat_uptr_t odd_real;  
	compat_uptr_t odd_imag;  
	compat_uptr_t even_real; 
	compat_uptr_t even_imag; 
};

struct atomisp_dvs2_statistics32 {
	struct atomisp_dvs_grid_info grid_info;
	struct atomisp_dvs2_stat_types32 hor_prod;
	struct atomisp_dvs2_stat_types32 ver_prod;
};

struct atomisp_dis_statistics32 {
	struct atomisp_dvs2_statistics32 dvs2_stat;
	u32 exp_id;
};

struct atomisp_dis_coefficients32 {
	struct atomisp_dvs_grid_info grid_info;
	struct atomisp_dvs2_coef_types32 hor_coefs;
	struct atomisp_dvs2_coef_types32 ver_coefs;
};

struct atomisp_3a_statistics32 {
	struct atomisp_grid_info  grid_info;
	compat_uptr_t data;
	compat_uptr_t rgby_data;
	u32 exp_id;
	u32 isp_config_id;
};

struct atomisp_morph_table32 {
	unsigned int enabled;
	unsigned int height;
	unsigned int width;	 
	compat_uptr_t coordinates_x[ATOMISP_MORPH_TABLE_NUM_PLANES];
	compat_uptr_t coordinates_y[ATOMISP_MORPH_TABLE_NUM_PLANES];
};

struct v4l2_framebuffer32 {
	__u32			capability;
	__u32			flags;
	compat_uptr_t		base;
	struct v4l2_pix_format	fmt;
};

struct atomisp_overlay32 {
	 
	compat_uptr_t frame;
	 
	unsigned char bg_y;
	 
	char bg_u;
	 
	char bg_v;
	 
	unsigned char blend_input_perc_y;
	 
	unsigned char blend_input_perc_u;
	 
	unsigned char blend_input_perc_v;
	 
	unsigned char blend_overlay_perc_y;
	 
	unsigned char blend_overlay_perc_u;
	 
	unsigned char blend_overlay_perc_v;
	 
	unsigned int overlay_start_x;
	 
	unsigned int overlay_start_y;
};

struct atomisp_shading_table32 {
	__u32 enable;
	__u32 sensor_width;
	__u32 sensor_height;
	__u32 width;
	__u32 height;
	__u32 fraction_bits;

	compat_uptr_t data[ATOMISP_NUM_SC_COLORS];
};

struct atomisp_parameters32 {
	compat_uptr_t wb_config;   
	compat_uptr_t cc_config;   
	compat_uptr_t tnr_config;  
	compat_uptr_t ecd_config;  
	compat_uptr_t ynr_config;  
	compat_uptr_t fc_config;   
	compat_uptr_t formats_config;   
	compat_uptr_t cnr_config;  
	compat_uptr_t macc_config;   
	compat_uptr_t ctc_config;  
	compat_uptr_t aa_config;   
	compat_uptr_t baa_config;   
	compat_uptr_t ce_config;
	compat_uptr_t dvs_6axis_config;
	compat_uptr_t ob_config;   
	compat_uptr_t dp_config;   
	compat_uptr_t nr_config;   
	compat_uptr_t ee_config;   
	compat_uptr_t de_config;   
	compat_uptr_t gc_config;   
	compat_uptr_t anr_config;  
	compat_uptr_t a3a_config;  
	compat_uptr_t xnr_config;  
	compat_uptr_t dz_config;   
	compat_uptr_t yuv2rgb_cc_config;  
	compat_uptr_t rgb2yuv_cc_config;  
	compat_uptr_t macc_table;
	compat_uptr_t gamma_table;
	compat_uptr_t ctc_table;
	compat_uptr_t xnr_table;
	compat_uptr_t r_gamma_table;
	compat_uptr_t g_gamma_table;
	compat_uptr_t b_gamma_table;
	compat_uptr_t motion_vector;  
	compat_uptr_t shading_table;
	compat_uptr_t morph_table;
	compat_uptr_t dvs_coefs;  
	compat_uptr_t dvs2_coefs;  
	compat_uptr_t capture_config;
	compat_uptr_t anr_thres;

	compat_uptr_t	lin_2500_config;        
	compat_uptr_t	obgrid_2500_config;     
	compat_uptr_t	bnr_2500_config;        
	compat_uptr_t	shd_2500_config;        
	compat_uptr_t	dm_2500_config;         
	compat_uptr_t	rgbpp_2500_config;      
	compat_uptr_t	dvs_stat_2500_config;   
	compat_uptr_t	lace_stat_2500_config;  
	compat_uptr_t	yuvp1_2500_config;      
	compat_uptr_t	yuvp2_2500_config;      
	compat_uptr_t	tnr_2500_config;        
	compat_uptr_t	dpc_2500_config;        
	compat_uptr_t	awb_2500_config;        
	compat_uptr_t
	awb_fr_2500_config;     
	compat_uptr_t	anr_2500_config;        
	compat_uptr_t	af_2500_config;         
	compat_uptr_t	ae_2500_config;         
	compat_uptr_t	bds_2500_config;        
	compat_uptr_t
	dvs_2500_config;        
	compat_uptr_t	res_mgr_2500_config;

	 
	compat_uptr_t	output_frame;
	 
	u32	isp_config_id;
	u32	per_frame_setting;
};

struct atomisp_dvs_6axis_config32 {
	u32 exp_id;
	u32 width_y;
	u32 height_y;
	u32 width_uv;
	u32 height_uv;
	compat_uptr_t xcoords_y;
	compat_uptr_t ycoords_y;
	compat_uptr_t xcoords_uv;
	compat_uptr_t ycoords_uv;
};

#define ATOMISP_IOC_G_HISTOGRAM32 \
	_IOWR('v', BASE_VIDIOC_PRIVATE + 3, struct atomisp_histogram32)
#define ATOMISP_IOC_S_HISTOGRAM32 \
	_IOW('v', BASE_VIDIOC_PRIVATE + 3, struct atomisp_histogram32)

#define ATOMISP_IOC_G_DIS_STAT32 \
	_IOWR('v', BASE_VIDIOC_PRIVATE + 6, struct atomisp_dis_statistics32)
#define ATOMISP_IOC_S_DIS_COEFS32 \
	_IOW('v', BASE_VIDIOC_PRIVATE + 6, struct atomisp_dis_coefficients32)

#define ATOMISP_IOC_S_DIS_VECTOR32 \
	_IOW('v', BASE_VIDIOC_PRIVATE + 6, struct atomisp_dvs_6axis_config32)

#define ATOMISP_IOC_G_3A_STAT32 \
	_IOWR('v', BASE_VIDIOC_PRIVATE + 7, struct atomisp_3a_statistics32)

#define ATOMISP_IOC_G_ISP_GDC_TAB32 \
	_IOR('v', BASE_VIDIOC_PRIVATE + 10, struct atomisp_morph_table32)
#define ATOMISP_IOC_S_ISP_GDC_TAB32 \
	_IOW('v', BASE_VIDIOC_PRIVATE + 10, struct atomisp_morph_table32)

#define ATOMISP_IOC_S_ISP_FPN_TABLE32 \
	_IOW('v', BASE_VIDIOC_PRIVATE + 17, struct v4l2_framebuffer32)

#define ATOMISP_IOC_G_ISP_OVERLAY32 \
	_IOWR('v', BASE_VIDIOC_PRIVATE + 18, struct atomisp_overlay32)
#define ATOMISP_IOC_S_ISP_OVERLAY32 \
	_IOW('v', BASE_VIDIOC_PRIVATE + 18, struct atomisp_overlay32)

#define ATOMISP_IOC_S_ISP_SHD_TAB32 \
	_IOWR('v', BASE_VIDIOC_PRIVATE + 27, struct atomisp_shading_table32)

#define ATOMISP_IOC_S_PARAMETERS32 \
	_IOW('v', BASE_VIDIOC_PRIVATE + 32, struct atomisp_parameters32)

#endif  
