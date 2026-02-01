 
 

#ifndef _ATOM_ISP_H
#define _ATOM_ISP_H

#include <linux/types.h>
#include <linux/version.h>

 
#define ATOMISP_HW_REVISION_MASK	0x0000ff00
#define ATOMISP_HW_REVISION_SHIFT	8
#define ATOMISP_HW_REVISION_ISP2300	0x00
#define ATOMISP_HW_REVISION_ISP2400	0x10
#define ATOMISP_HW_REVISION_ISP2401_LEGACY 0x11
#define ATOMISP_HW_REVISION_ISP2401	0x20

#define ATOMISP_HW_STEPPING_MASK	0x000000ff
#define ATOMISP_HW_STEPPING_A0		0x00
#define ATOMISP_HW_STEPPING_B0		0x10

 
#define CI_MODE_PREVIEW		0x8000
#define CI_MODE_VIDEO		0x4000
#define CI_MODE_STILL_CAPTURE	0x2000
#define CI_MODE_NONE		0x0000

#define OUTPUT_MODE_FILE 0x0100
#define OUTPUT_MODE_TEXT 0x0200

 
#define ATOMISP_BUFFER_HAS_PER_FRAME_SETTING	0x80000000

 
#define V4L2_PIX_FMT_CUSTOM_M10MO_RAW	v4l2_fourcc('M', '1', '0', '1')

 
#define V4L2_MBUS_FMT_CUSTOM_YUV420	0x8001
#define V4L2_MBUS_FMT_CUSTOM_YVU420	0x8002
#define V4L2_MBUS_FMT_CUSTOM_YUV422P	0x8003
#define V4L2_MBUS_FMT_CUSTOM_YUV444	0x8004
#define V4L2_MBUS_FMT_CUSTOM_NV12	0x8005
#define V4L2_MBUS_FMT_CUSTOM_NV21	0x8006
#define V4L2_MBUS_FMT_CUSTOM_NV16	0x8007
#define V4L2_MBUS_FMT_CUSTOM_YUYV	0x8008
#define V4L2_MBUS_FMT_CUSTOM_SBGGR16	0x8009
#define V4L2_MBUS_FMT_CUSTOM_RGB32	0x800a

 
#if 0
#define V4L2_MBUS_FMT_CUSTOM_M10MO_RAW	0x800b
#endif

 
struct atomisp_nr_config {
	 
	unsigned int bnr_gain;
	 
	unsigned int ynr_gain;
	 
	unsigned int direction;
	 
	unsigned int threshold_cb;
	 
	unsigned int threshold_cr;
};

 
struct atomisp_tnr_config {
	unsigned int gain;	  
	unsigned int threshold_y; 
	unsigned int threshold_uv; 
};

 
struct atomisp_histogram {
	unsigned int num_elements;
	void __user *data;
};

enum atomisp_ob_mode {
	atomisp_ob_mode_none,
	atomisp_ob_mode_fixed,
	atomisp_ob_mode_raster
};

 
struct atomisp_ob_config {
	 
	enum atomisp_ob_mode mode;
	 
	unsigned int level_gr;
	 
	unsigned int level_r;
	 
	unsigned int level_b;
	 
	unsigned int level_gb;
	 
	unsigned short start_position;
	 
	unsigned short end_position;
};

 
struct atomisp_ee_config {
	 
	unsigned int gain;
	 
	unsigned int threshold;
	 
	unsigned int detail_gain;
};

struct atomisp_3a_output {
	int ae_y;
	int awb_cnt;
	int awb_gr;
	int awb_r;
	int awb_b;
	int awb_gb;
	int af_hpf1;
	int af_hpf2;
};

enum atomisp_calibration_type {
	calibration_type1,
	calibration_type2,
	calibration_type3
};

struct atomisp_gc_config {
	__u16 gain_k1;
	__u16 gain_k2;
};

struct atomisp_3a_config {
	unsigned int ae_y_coef_r;	 
	unsigned int ae_y_coef_g;	 
	unsigned int ae_y_coef_b;	 
	unsigned int awb_lg_high_raw;	 
	unsigned int awb_lg_low;	 
	unsigned int awb_lg_high;	 
	int af_fir1_coef[7];	 
	int af_fir2_coef[7];	 
};

struct atomisp_dvs_grid_info {
	u32 enable;
	u32 width;
	u32 aligned_width;
	u32 height;
	u32 aligned_height;
	u32 bqs_per_grid_cell;
	u32 num_hor_coefs;
	u32 num_ver_coefs;
};

struct atomisp_dvs_envelop {
	unsigned int width;
	unsigned int height;
};

struct atomisp_grid_info {
	u32 enable;
	u32 use_dmem;
	u32 has_histogram;
	u32 s3a_width;
	u32 s3a_height;
	u32 aligned_width;
	u32 aligned_height;
	u32 s3a_bqs_per_grid_cell;
	u32 deci_factor_log2;
	u32 elem_bit_depth;
};

struct atomisp_dis_vector {
	int x;
	int y;
};

 
struct atomisp_dvs2_coef_types {
	short __user *odd_real;  
	short __user *odd_imag;  
	short __user *even_real; 
	short __user *even_imag; 
};

 
struct atomisp_dvs2_stat_types {
	int __user *odd_real;  
	int __user *odd_imag;  
	int __user *even_real; 
	int __user *even_imag; 
};

struct atomisp_dis_coefficients {
	struct atomisp_dvs_grid_info grid_info;
	struct atomisp_dvs2_coef_types hor_coefs;
	struct atomisp_dvs2_coef_types ver_coefs;
};

struct atomisp_dvs2_statistics {
	struct atomisp_dvs_grid_info grid_info;
	struct atomisp_dvs2_stat_types hor_prod;
	struct atomisp_dvs2_stat_types ver_prod;
};

struct atomisp_dis_statistics {
	struct atomisp_dvs2_statistics dvs2_stat;
	u32 exp_id;
};

struct atomisp_3a_rgby_output {
	u32 r;
	u32 g;
	u32 b;
	u32 y;
};

 
enum atomisp_metadata_type {
	ATOMISP_MAIN_METADATA = 0,
	ATOMISP_SEC_METADATA,
	ATOMISP_METADATA_TYPE_NUM,
};

struct atomisp_ext_isp_ctrl {
	u32 id;
	u32 data;
};

struct atomisp_3a_statistics {
	struct atomisp_grid_info  grid_info;
	struct atomisp_3a_output __user *data;
	struct atomisp_3a_rgby_output __user *rgby_data;
	u32 exp_id;  
	u32 isp_config_id;  
};

 
struct atomisp_wb_config {
	unsigned int integer_bits;
	unsigned int gr;	 
	unsigned int r;		 
	unsigned int b;		 
	unsigned int gb;	 
};

 
struct atomisp_cc_config {
	unsigned int fraction_bits;
	int matrix[3 * 3];	 
};

 
struct atomisp_de_config {
	unsigned int pixelnoise;
	unsigned int c1_coring_threshold;
	unsigned int c2_coring_threshold;
};

 
struct atomisp_ce_config {
	unsigned char uv_level_min;
	unsigned char uv_level_max;
};

 
struct atomisp_dp_config {
	 
	unsigned int threshold;
	 
	unsigned int gain;
	unsigned int gr;
	unsigned int r;
	unsigned int b;
	unsigned int gb;
};

 
struct atomisp_xnr_config {
	__u16 threshold;
};

 
struct atomisp_metadata_config {
	u32 metadata_height;
	u32 metadata_stride;
};

 
struct atomisp_resolution {
	u32 width;   
	u32 height;  
};

 
struct atomisp_zoom_point {
	s32 x;  
	s32 y;  
};

 
struct atomisp_zoom_region {
	struct atomisp_zoom_point
		origin;  
	struct atomisp_resolution resolution;  
};

struct atomisp_dz_config {
	u32 dx;  
	u32 dy;  
	struct atomisp_zoom_region zoom_region;  
};

struct atomisp_parm {
	struct atomisp_grid_info info;
	struct atomisp_dvs_grid_info dvs_grid;
	struct atomisp_dvs_envelop dvs_envelop;
	struct atomisp_wb_config wb_config;
	struct atomisp_cc_config cc_config;
	struct atomisp_ob_config ob_config;
	struct atomisp_de_config de_config;
	struct atomisp_dz_config dz_config;
	struct atomisp_ce_config ce_config;
	struct atomisp_dp_config dp_config;
	struct atomisp_nr_config nr_config;
	struct atomisp_ee_config ee_config;
	struct atomisp_tnr_config tnr_config;
	struct atomisp_metadata_config metadata_config;
};

struct dvs2_bq_resolution {
	int width_bq;          
	int height_bq;         
};

struct atomisp_dvs2_bq_resolutions {
	 
	struct dvs2_bq_resolution source_bq;
	 
	struct dvs2_bq_resolution output_bq;
	 
	struct dvs2_bq_resolution envelope_bq;
	 
	struct dvs2_bq_resolution ispfilter_bq;
	 
	struct dvs2_bq_resolution gdc_shift_bq;
};

struct atomisp_dvs_6axis_config {
	u32 exp_id;
	u32 width_y;
	u32 height_y;
	u32 width_uv;
	u32 height_uv;
	u32 *xcoords_y;
	u32 *ycoords_y;
	u32 *xcoords_uv;
	u32 *ycoords_uv;
};

struct atomisp_formats_config {
	u32 video_full_range_flag;
};

struct atomisp_parameters {
	struct atomisp_wb_config   *wb_config;   
	struct atomisp_cc_config   *cc_config;   
	struct atomisp_tnr_config  *tnr_config;  
	struct atomisp_ecd_config  *ecd_config;  
	struct atomisp_ynr_config  *ynr_config;  
	struct atomisp_fc_config   *fc_config;   
	struct atomisp_formats_config *formats_config;  
	struct atomisp_cnr_config  *cnr_config;  
	struct atomisp_macc_config *macc_config;   
	struct atomisp_ctc_config  *ctc_config;  
	struct atomisp_aa_config   *aa_config;   
	struct atomisp_aa_config   *baa_config;   
	struct atomisp_ce_config   *ce_config;
	struct atomisp_dvs_6axis_config *dvs_6axis_config;
	struct atomisp_ob_config   *ob_config;   
	struct atomisp_dp_config   *dp_config;   
	struct atomisp_nr_config   *nr_config;   
	struct atomisp_ee_config   *ee_config;   
	struct atomisp_de_config   *de_config;   
	struct atomisp_gc_config   *gc_config;   
	struct atomisp_anr_config  *anr_config;  
	struct atomisp_3a_config   *a3a_config;  
	struct atomisp_xnr_config  *xnr_config;  
	struct atomisp_dz_config   *dz_config;   
	struct atomisp_cc_config *yuv2rgb_cc_config;  
	struct atomisp_cc_config *rgb2yuv_cc_config;  
	struct atomisp_macc_table  *macc_table;
	struct atomisp_gamma_table *gamma_table;
	struct atomisp_ctc_table   *ctc_table;
	struct atomisp_xnr_table   *xnr_table;
	struct atomisp_rgb_gamma_table *r_gamma_table;
	struct atomisp_rgb_gamma_table *g_gamma_table;
	struct atomisp_rgb_gamma_table *b_gamma_table;
	struct atomisp_vector      *motion_vector;  
	struct atomisp_shading_table *shading_table;
	struct atomisp_morph_table   *morph_table;
	struct atomisp_dvs_coefficients *dvs_coefs;  
	struct atomisp_dis_coefficients *dvs2_coefs;  
	struct atomisp_capture_config   *capture_config;
	struct atomisp_anr_thres   *anr_thres;

	void	*lin_2500_config;        
	void	*obgrid_2500_config;     
	void	*bnr_2500_config;        
	void	*shd_2500_config;        
	void	*dm_2500_config;         
	void	*rgbpp_2500_config;      
	void	*dvs_stat_2500_config;   
	void	*lace_stat_2500_config;  
	void	*yuvp1_2500_config;      
	void	*yuvp2_2500_config;      
	void	*tnr_2500_config;        
	void	*dpc_2500_config;        
	void	*awb_2500_config;        
	void	*awb_fr_2500_config;     
	void	*anr_2500_config;        
	void	*af_2500_config;         
	void	*ae_2500_config;         
	void	*bds_2500_config;        
	void	*dvs_2500_config;        
	void	*res_mgr_2500_config;

	 
	void	*output_frame;
	 
	u32	isp_config_id;

	 
	u32	per_frame_setting;
};

#define ATOMISP_GAMMA_TABLE_SIZE        1024
struct atomisp_gamma_table {
	unsigned short data[ATOMISP_GAMMA_TABLE_SIZE];
};

 
#define ATOMISP_MORPH_TABLE_NUM_PLANES  6
struct atomisp_morph_table {
	unsigned int enabled;

	unsigned int height;
	unsigned int width;	 
	unsigned short __user *coordinates_x[ATOMISP_MORPH_TABLE_NUM_PLANES];
	unsigned short __user *coordinates_y[ATOMISP_MORPH_TABLE_NUM_PLANES];
};

#define ATOMISP_NUM_SC_COLORS	4
#define ATOMISP_SC_FLAG_QUERY	BIT(0)

struct atomisp_shading_table {
	__u32 enable;

	__u32 sensor_width;
	__u32 sensor_height;
	__u32 width;
	__u32 height;
	__u32 fraction_bits;

	__u16 *data[ATOMISP_NUM_SC_COLORS];
};

 
#define ATOMISP_NUM_MACC_AXES           16
struct atomisp_macc_table {
	short data[4 * ATOMISP_NUM_MACC_AXES];
};

struct atomisp_macc_config {
	int color_effect;
	struct atomisp_macc_table table;
};

 
#define ATOMISP_CTC_TABLE_SIZE          1024
struct atomisp_ctc_table {
	unsigned short data[ATOMISP_CTC_TABLE_SIZE];
};

 
struct atomisp_overlay {
	 
	struct v4l2_framebuffer *frame;
	 
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

struct atomisp_exposure {
	unsigned int integration_time[8];
	unsigned int shutter_speed[8];
	unsigned int gain[4];
	unsigned int aperture;
};

 
struct atomisp_bc_video_package {
	int ioctl_cmd;
	int device_id;
	int inputparam;
	int outputparam;
};

enum atomisp_focus_hp {
	ATOMISP_FOCUS_HP_IN_PROGRESS = (1U << 2),
	ATOMISP_FOCUS_HP_COMPLETE    = (2U << 2),
	ATOMISP_FOCUS_HP_FAILED      = (3U << 2)
};

 
#define ATOMISP_FOCUS_STATUS_MOVING           BIT(0)
#define ATOMISP_FOCUS_STATUS_ACCEPTS_NEW_MOVE BIT(1)
#define ATOMISP_FOCUS_STATUS_HOME_POSITION    (3U << 2)

enum atomisp_camera_port {
	ATOMISP_CAMERA_PORT_SECONDARY,
	ATOMISP_CAMERA_PORT_PRIMARY,
	ATOMISP_CAMERA_PORT_TERTIARY,
	ATOMISP_CAMERA_NR_PORTS
};

 
enum atomisp_flash_mode {
	ATOMISP_FLASH_MODE_OFF,
	ATOMISP_FLASH_MODE_FLASH,
	ATOMISP_FLASH_MODE_TORCH,
	ATOMISP_FLASH_MODE_INDICATOR,
};

 
enum atomisp_flash_status {
	ATOMISP_FLASH_STATUS_OK,
	ATOMISP_FLASH_STATUS_HW_ERROR,
	ATOMISP_FLASH_STATUS_INTERRUPTED,
	ATOMISP_FLASH_STATUS_TIMEOUT,
};

 
enum atomisp_frame_status {
	ATOMISP_FRAME_STATUS_OK,
	ATOMISP_FRAME_STATUS_CORRUPTED,
	ATOMISP_FRAME_STATUS_FLASH_EXPOSED,
	ATOMISP_FRAME_STATUS_FLASH_PARTIAL,
	ATOMISP_FRAME_STATUS_FLASH_FAILED,
};

enum atomisp_ext_isp_id {
	EXT_ISP_CID_ISO = 0,
	EXT_ISP_CID_CAPTURE_HDR,
	EXT_ISP_CID_CAPTURE_LLS,
	EXT_ISP_CID_FOCUS_MODE,
	EXT_ISP_CID_FOCUS_EXECUTION,
	EXT_ISP_CID_TOUCH_POSX,
	EXT_ISP_CID_TOUCH_POSY,
	EXT_ISP_CID_CAF_STATUS,
	EXT_ISP_CID_AF_STATUS,
	EXT_ISP_CID_GET_AF_MODE,
	EXT_ISP_CID_CAPTURE_BURST,
	EXT_ISP_CID_FLASH_MODE,
	EXT_ISP_CID_ZOOM,
	EXT_ISP_CID_SHOT_MODE
};

#define EXT_ISP_FOCUS_MODE_NORMAL	0
#define EXT_ISP_FOCUS_MODE_MACRO	1
#define EXT_ISP_FOCUS_MODE_TOUCH_AF	2
#define EXT_ISP_FOCUS_MODE_PREVIEW_CAF	3
#define EXT_ISP_FOCUS_MODE_MOVIE_CAF	4
#define EXT_ISP_FOCUS_MODE_FACE_CAF	5
#define EXT_ISP_FOCUS_MODE_TOUCH_MACRO	6
#define EXT_ISP_FOCUS_MODE_TOUCH_CAF	7

#define EXT_ISP_FOCUS_STOP		0
#define EXT_ISP_FOCUS_SEARCH		1
#define EXT_ISP_PAN_FOCUSING		2

#define EXT_ISP_CAF_RESTART_CHECK	1
#define EXT_ISP_CAF_STATUS_FOCUSING	2
#define EXT_ISP_CAF_STATUS_SUCCESS	3
#define EXT_ISP_CAF_STATUS_FAIL         4

#define EXT_ISP_AF_STATUS_INVALID	1
#define EXT_ISP_AF_STATUS_FOCUSING	2
#define EXT_ISP_AF_STATUS_SUCCESS	3
#define EXT_ISP_AF_STATUS_FAIL		4

enum atomisp_burst_capture_options {
	EXT_ISP_BURST_CAPTURE_CTRL_START = 0,
	EXT_ISP_BURST_CAPTURE_CTRL_STOP
};

#define EXT_ISP_FLASH_MODE_OFF		0
#define EXT_ISP_FLASH_MODE_ON		1
#define EXT_ISP_FLASH_MODE_AUTO		2
#define EXT_ISP_LED_TORCH_OFF		3
#define EXT_ISP_LED_TORCH_ON		4

#define EXT_ISP_SHOT_MODE_AUTO		0
#define EXT_ISP_SHOT_MODE_BEAUTY_FACE	1
#define EXT_ISP_SHOT_MODE_BEST_PHOTO	2
#define EXT_ISP_SHOT_MODE_DRAMA		3
#define EXT_ISP_SHOT_MODE_BEST_FACE	4
#define EXT_ISP_SHOT_MODE_ERASER	5
#define EXT_ISP_SHOT_MODE_PANORAMA	6
#define EXT_ISP_SHOT_MODE_RICH_TONE_HDR	7
#define EXT_ISP_SHOT_MODE_NIGHT		8
#define EXT_ISP_SHOT_MODE_SOUND_SHOT	9
#define EXT_ISP_SHOT_MODE_ANIMATED_PHOTO	10
#define EXT_ISP_SHOT_MODE_SPORTS	11

 
struct atomisp_s_runmode {
	__u32 mode;
};

 
#define ATOMISP_IOC_G_XNR \
	_IOR('v', BASE_VIDIOC_PRIVATE + 0, int)
#define ATOMISP_IOC_S_XNR \
	_IOW('v', BASE_VIDIOC_PRIVATE + 0, int)
#define ATOMISP_IOC_G_NR \
	_IOR('v', BASE_VIDIOC_PRIVATE + 1, struct atomisp_nr_config)
#define ATOMISP_IOC_S_NR \
	_IOW('v', BASE_VIDIOC_PRIVATE + 1, struct atomisp_nr_config)
#define ATOMISP_IOC_G_TNR \
	_IOR('v', BASE_VIDIOC_PRIVATE + 2, struct atomisp_tnr_config)
#define ATOMISP_IOC_S_TNR \
	_IOW('v', BASE_VIDIOC_PRIVATE + 2, struct atomisp_tnr_config)
#define ATOMISP_IOC_G_HISTOGRAM \
	_IOWR('v', BASE_VIDIOC_PRIVATE + 3, struct atomisp_histogram)
#define ATOMISP_IOC_S_HISTOGRAM \
	_IOW('v', BASE_VIDIOC_PRIVATE + 3, struct atomisp_histogram)
#define ATOMISP_IOC_G_BLACK_LEVEL_COMP \
	_IOR('v', BASE_VIDIOC_PRIVATE + 4, struct atomisp_ob_config)
#define ATOMISP_IOC_S_BLACK_LEVEL_COMP \
	_IOW('v', BASE_VIDIOC_PRIVATE + 4, struct atomisp_ob_config)
#define ATOMISP_IOC_G_EE \
	_IOR('v', BASE_VIDIOC_PRIVATE + 5, struct atomisp_ee_config)
#define ATOMISP_IOC_S_EE \
	_IOW('v', BASE_VIDIOC_PRIVATE + 5, struct atomisp_ee_config)
 
#define ATOMISP_IOC_G_DIS_STAT \
	_IOWR('v', BASE_VIDIOC_PRIVATE + 6, struct atomisp_dis_statistics)

#define ATOMISP_IOC_G_DVS2_BQ_RESOLUTIONS \
	_IOR('v', BASE_VIDIOC_PRIVATE + 6, struct atomisp_dvs2_bq_resolutions)

#define ATOMISP_IOC_S_DIS_COEFS \
	_IOW('v', BASE_VIDIOC_PRIVATE + 6, struct atomisp_dis_coefficients)

#define ATOMISP_IOC_S_DIS_VECTOR \
	_IOW('v', BASE_VIDIOC_PRIVATE + 6, struct atomisp_dvs_6axis_config)

#define ATOMISP_IOC_G_3A_STAT \
	_IOWR('v', BASE_VIDIOC_PRIVATE + 7, struct atomisp_3a_statistics)
#define ATOMISP_IOC_G_ISP_PARM \
	_IOR('v', BASE_VIDIOC_PRIVATE + 8, struct atomisp_parm)
#define ATOMISP_IOC_S_ISP_PARM \
	_IOW('v', BASE_VIDIOC_PRIVATE + 8, struct atomisp_parm)
#define ATOMISP_IOC_G_ISP_GAMMA \
	_IOR('v', BASE_VIDIOC_PRIVATE + 9, struct atomisp_gamma_table)
#define ATOMISP_IOC_S_ISP_GAMMA \
	_IOW('v', BASE_VIDIOC_PRIVATE + 9, struct atomisp_gamma_table)
#define ATOMISP_IOC_G_ISP_GDC_TAB \
	_IOR('v', BASE_VIDIOC_PRIVATE + 10, struct atomisp_morph_table)
#define ATOMISP_IOC_S_ISP_GDC_TAB \
	_IOW('v', BASE_VIDIOC_PRIVATE + 10, struct atomisp_morph_table)

 
#define ATOMISP_IOC_G_ISP_MACC \
	_IOR('v', BASE_VIDIOC_PRIVATE + 12, struct atomisp_macc_config)
#define ATOMISP_IOC_S_ISP_MACC \
	_IOW('v', BASE_VIDIOC_PRIVATE + 12, struct atomisp_macc_config)

 
#define ATOMISP_IOC_G_ISP_BAD_PIXEL_DETECTION \
	_IOR('v', BASE_VIDIOC_PRIVATE + 13, struct atomisp_dp_config)
#define ATOMISP_IOC_S_ISP_BAD_PIXEL_DETECTION \
	_IOW('v', BASE_VIDIOC_PRIVATE + 13, struct atomisp_dp_config)

 
#define ATOMISP_IOC_G_ISP_FALSE_COLOR_CORRECTION \
	_IOR('v', BASE_VIDIOC_PRIVATE + 14, struct atomisp_de_config)
#define ATOMISP_IOC_S_ISP_FALSE_COLOR_CORRECTION \
	_IOW('v', BASE_VIDIOC_PRIVATE + 14, struct atomisp_de_config)

 
#define ATOMISP_IOC_G_ISP_CTC \
	_IOR('v', BASE_VIDIOC_PRIVATE + 15, struct atomisp_ctc_table)
#define ATOMISP_IOC_S_ISP_CTC \
	_IOW('v', BASE_VIDIOC_PRIVATE + 15, struct atomisp_ctc_table)

 
#define ATOMISP_IOC_G_ISP_WHITE_BALANCE \
	_IOR('v', BASE_VIDIOC_PRIVATE + 16, struct atomisp_wb_config)
#define ATOMISP_IOC_S_ISP_WHITE_BALANCE \
	_IOW('v', BASE_VIDIOC_PRIVATE + 16, struct atomisp_wb_config)

 
#define ATOMISP_IOC_S_ISP_FPN_TABLE \
	_IOW('v', BASE_VIDIOC_PRIVATE + 17, struct v4l2_framebuffer)

 
#define ATOMISP_IOC_G_ISP_OVERLAY \
	_IOWR('v', BASE_VIDIOC_PRIVATE + 18, struct atomisp_overlay)
#define ATOMISP_IOC_S_ISP_OVERLAY \
	_IOW('v', BASE_VIDIOC_PRIVATE + 18, struct atomisp_overlay)

 
#define ATOMISP_IOC_CAMERA_BRIDGE \
	_IOWR('v', BASE_VIDIOC_PRIVATE + 19, struct atomisp_bc_video_package)

#define ATOMISP_IOC_S_EXPOSURE \
	_IOW('v', BASE_VIDIOC_PRIVATE + 21, struct atomisp_exposure)

 
#define ATOMISP_IOC_G_3A_CONFIG \
	_IOR('v', BASE_VIDIOC_PRIVATE + 23, struct atomisp_3a_config)
#define ATOMISP_IOC_S_3A_CONFIG \
	_IOW('v', BASE_VIDIOC_PRIVATE + 23, struct atomisp_3a_config)

 
#define ATOMISP_IOC_S_ISP_SHD_TAB \
	_IOWR('v', BASE_VIDIOC_PRIVATE + 27, struct atomisp_shading_table)

 
#define ATOMISP_IOC_G_ISP_GAMMA_CORRECTION \
	_IOR('v', BASE_VIDIOC_PRIVATE + 28, struct atomisp_gc_config)

#define ATOMISP_IOC_S_ISP_GAMMA_CORRECTION \
	_IOW('v', BASE_VIDIOC_PRIVATE + 28, struct atomisp_gc_config)

#define ATOMISP_IOC_S_PARAMETERS \
	_IOW('v', BASE_VIDIOC_PRIVATE + 32, struct atomisp_parameters)

#define ATOMISP_IOC_EXT_ISP_CTRL \
	_IOWR('v', BASE_VIDIOC_PRIVATE + 35, struct atomisp_ext_isp_ctrl)

#define ATOMISP_IOC_EXP_ID_UNLOCK \
	_IOW('v', BASE_VIDIOC_PRIVATE + 36, int)

#define ATOMISP_IOC_EXP_ID_CAPTURE \
	_IOW('v', BASE_VIDIOC_PRIVATE + 37, int)

#define ATOMISP_IOC_S_ENABLE_DZ_CAPT_PIPE \
	_IOWR('v', BASE_VIDIOC_PRIVATE + 38, unsigned int)

#define ATOMISP_IOC_G_FORMATS_CONFIG \
	_IOR('v', BASE_VIDIOC_PRIVATE + 39, struct atomisp_formats_config)

#define ATOMISP_IOC_S_FORMATS_CONFIG \
	_IOW('v', BASE_VIDIOC_PRIVATE + 39, struct atomisp_formats_config)

#define ATOMISP_IOC_INJECT_A_FAKE_EVENT \
	_IOW('v', BASE_VIDIOC_PRIVATE + 42, int)

#define ATOMISP_IOC_S_ARRAY_RESOLUTION \
	_IOW('v', BASE_VIDIOC_PRIVATE + 45, struct atomisp_resolution)

 
#define ATOMISP_IOC_G_DEPTH_SYNC_COMP \
	_IOR('v', BASE_VIDIOC_PRIVATE + 46, unsigned int)

#define ATOMISP_IOC_S_SENSOR_EE_CONFIG \
	_IOW('v', BASE_VIDIOC_PRIVATE + 47, unsigned int)

#define ATOMISP_IOC_S_SENSOR_RUNMODE \
	_IOW('v', BASE_VIDIOC_PRIVATE + 48, struct atomisp_s_runmode)

 

 
#define V4L2_CID_ATOMISP_BAD_PIXEL_DETECTION \
	(V4L2_CID_PRIVATE_BASE + 0)
#define V4L2_CID_ATOMISP_POSTPROCESS_GDC_CAC \
	(V4L2_CID_PRIVATE_BASE + 1)
#define V4L2_CID_ATOMISP_VIDEO_STABLIZATION \
	(V4L2_CID_PRIVATE_BASE + 2)
#define V4L2_CID_ATOMISP_FIXED_PATTERN_NR \
	(V4L2_CID_PRIVATE_BASE + 3)
#define V4L2_CID_ATOMISP_FALSE_COLOR_CORRECTION \
	(V4L2_CID_PRIVATE_BASE + 4)
#define V4L2_CID_ATOMISP_LOW_LIGHT \
	(V4L2_CID_PRIVATE_BASE + 5)

 
#define V4L2_CID_CAMERA_LASTP1             (V4L2_CID_CAMERA_CLASS_BASE + 1024)

 

 
#define V4L2_CID_REQUEST_FLASH             (V4L2_CID_CAMERA_LASTP1 + 3)
 
#define V4L2_CID_FLASH_STATUS              (V4L2_CID_CAMERA_LASTP1 + 5)
 
#define V4L2_CID_FLASH_MODE                (V4L2_CID_CAMERA_LASTP1 + 10)

 
#define V4L2_CID_VCM_SLEW                  (V4L2_CID_CAMERA_LASTP1 + 11)
 
#define V4L2_CID_VCM_TIMING                (V4L2_CID_CAMERA_LASTP1 + 12)

 
#define V4L2_CID_FOCUS_STATUS              (V4L2_CID_CAMERA_LASTP1 + 14)

 
#define V4L2_CID_G_SKIP_FRAMES		   (V4L2_CID_CAMERA_LASTP1 + 17)

 
#define V4L2_CID_2A_STATUS                 (V4L2_CID_CAMERA_LASTP1 + 18)
#define V4L2_2A_STATUS_AE_READY            BIT(0)
#define V4L2_2A_STATUS_AWB_READY           BIT(1)

#define V4L2_CID_RUN_MODE			(V4L2_CID_CAMERA_LASTP1 + 20)
#define ATOMISP_RUN_MODE_VIDEO			1
#define ATOMISP_RUN_MODE_STILL_CAPTURE		2
#define ATOMISP_RUN_MODE_PREVIEW		3
#define ATOMISP_RUN_MODE_MIN			1
#define ATOMISP_RUN_MODE_MAX			3

#define V4L2_CID_ENABLE_VFPP			(V4L2_CID_CAMERA_LASTP1 + 21)
#define V4L2_CID_ATOMISP_CONTINUOUS_MODE	(V4L2_CID_CAMERA_LASTP1 + 22)
#define V4L2_CID_ATOMISP_CONTINUOUS_RAW_BUFFER_SIZE \
						(V4L2_CID_CAMERA_LASTP1 + 23)
#define V4L2_CID_ATOMISP_CONTINUOUS_VIEWFINDER \
						(V4L2_CID_CAMERA_LASTP1 + 24)

#define V4L2_CID_VFPP				(V4L2_CID_CAMERA_LASTP1 + 25)
#define ATOMISP_VFPP_ENABLE			0
#define ATOMISP_VFPP_DISABLE_SCALER		1
#define ATOMISP_VFPP_DISABLE_LOWLAT		2

 
#define V4L2_CID_FLASH_STATUS_REGISTER  (V4L2_CID_CAMERA_LASTP1 + 26)

#define V4L2_CID_START_ZSL_CAPTURE	(V4L2_CID_CAMERA_LASTP1 + 28)
 
#define V4L2_CID_ENABLE_RAW_BUFFER_LOCK (V4L2_CID_CAMERA_LASTP1 + 29)

#define V4L2_CID_EXPOSURE_ZONE_NUM	(V4L2_CID_CAMERA_LASTP1 + 31)
 
#define V4L2_CID_DISABLE_DZ		(V4L2_CID_CAMERA_LASTP1 + 32)

#define V4L2_CID_TEST_PATTERN_COLOR_R	(V4L2_CID_CAMERA_LASTP1 + 33)
#define V4L2_CID_TEST_PATTERN_COLOR_GR	(V4L2_CID_CAMERA_LASTP1 + 34)
#define V4L2_CID_TEST_PATTERN_COLOR_GB	(V4L2_CID_CAMERA_LASTP1 + 35)
#define V4L2_CID_TEST_PATTERN_COLOR_B	(V4L2_CID_CAMERA_LASTP1 + 36)

#define V4L2_CID_ATOMISP_SELECT_ISP_VERSION	(V4L2_CID_CAMERA_LASTP1 + 38)

#define V4L2_BUF_FLAG_BUFFER_INVALID       0x0400
#define V4L2_BUF_FLAG_BUFFER_VALID         0x0800

#define V4L2_BUF_TYPE_VIDEO_CAPTURE_ION  (V4L2_BUF_TYPE_PRIVATE + 1024)

#define V4L2_EVENT_ATOMISP_3A_STATS_READY   (V4L2_EVENT_PRIVATE_START + 1)
#define V4L2_EVENT_ATOMISP_METADATA_READY   (V4L2_EVENT_PRIVATE_START + 2)
#define V4L2_EVENT_ATOMISP_ACC_COMPLETE     (V4L2_EVENT_PRIVATE_START + 4)
#define V4L2_EVENT_ATOMISP_PAUSE_BUFFER	    (V4L2_EVENT_PRIVATE_START + 5)
#define V4L2_EVENT_ATOMISP_CSS_RESET	    (V4L2_EVENT_PRIVATE_START + 6)
 
enum {
	V4L2_COLORFX_SKIN_WHITEN_LOW = 1001,
	V4L2_COLORFX_SKIN_WHITEN_HIGH = 1002,
	V4L2_COLORFX_WARM = 1003,
	V4L2_COLORFX_COLD = 1004,
	V4L2_COLORFX_WASHED = 1005,
	V4L2_COLORFX_RED = 1006,
	V4L2_COLORFX_GREEN = 1007,
	V4L2_COLORFX_BLUE = 1008,
	V4L2_COLORFX_PINK = 1009,
	V4L2_COLORFX_YELLOW = 1010,
	V4L2_COLORFX_PURPLE = 1011,
};

#endif  
