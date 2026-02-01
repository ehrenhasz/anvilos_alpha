 
 
#ifndef ATOMISP_PLATFORM_H_
#define ATOMISP_PLATFORM_H_

#include <asm/intel-family.h>
#include <asm/processor.h>

#include <linux/i2c.h>
#include <media/v4l2-subdev.h>
#include "atomisp.h"

#define MAX_SENSORS_PER_PORT 4
#define MAX_STREAMS_PER_CHANNEL 2

#define CAMERA_MODULE_ID_LEN 64

enum atomisp_bayer_order {
	atomisp_bayer_order_grbg,
	atomisp_bayer_order_rggb,
	atomisp_bayer_order_bggr,
	atomisp_bayer_order_gbrg
};

enum atomisp_input_stream_id {
	ATOMISP_INPUT_STREAM_GENERAL = 0,
	ATOMISP_INPUT_STREAM_CAPTURE = 0,
	ATOMISP_INPUT_STREAM_POSTVIEW,
	ATOMISP_INPUT_STREAM_PREVIEW,
	ATOMISP_INPUT_STREAM_VIDEO,
	ATOMISP_INPUT_STREAM_NUM
};

enum atomisp_input_format {
	ATOMISP_INPUT_FORMAT_YUV420_8_LEGACY, 
	ATOMISP_INPUT_FORMAT_YUV420_8,  
	ATOMISP_INPUT_FORMAT_YUV420_10, 
	ATOMISP_INPUT_FORMAT_YUV420_16, 
	ATOMISP_INPUT_FORMAT_YUV422_8,  
	ATOMISP_INPUT_FORMAT_YUV422_10, 
	ATOMISP_INPUT_FORMAT_YUV422_16, 
	ATOMISP_INPUT_FORMAT_RGB_444,   
	ATOMISP_INPUT_FORMAT_RGB_555,   
	ATOMISP_INPUT_FORMAT_RGB_565,   
	ATOMISP_INPUT_FORMAT_RGB_666,   
	ATOMISP_INPUT_FORMAT_RGB_888,   
	ATOMISP_INPUT_FORMAT_RAW_6,     
	ATOMISP_INPUT_FORMAT_RAW_7,     
	ATOMISP_INPUT_FORMAT_RAW_8,     
	ATOMISP_INPUT_FORMAT_RAW_10,    
	ATOMISP_INPUT_FORMAT_RAW_12,    
	ATOMISP_INPUT_FORMAT_RAW_14,    
	ATOMISP_INPUT_FORMAT_RAW_16,    
	ATOMISP_INPUT_FORMAT_BINARY_8,  

	 
	ATOMISP_INPUT_FORMAT_GENERIC_SHORT1,   
	ATOMISP_INPUT_FORMAT_GENERIC_SHORT2,   
	ATOMISP_INPUT_FORMAT_GENERIC_SHORT3,   
	ATOMISP_INPUT_FORMAT_GENERIC_SHORT4,   
	ATOMISP_INPUT_FORMAT_GENERIC_SHORT5,   
	ATOMISP_INPUT_FORMAT_GENERIC_SHORT6,   
	ATOMISP_INPUT_FORMAT_GENERIC_SHORT7,   
	ATOMISP_INPUT_FORMAT_GENERIC_SHORT8,   

	 
	ATOMISP_INPUT_FORMAT_YUV420_8_SHIFT,   
	ATOMISP_INPUT_FORMAT_YUV420_10_SHIFT,  

	 
	ATOMISP_INPUT_FORMAT_EMBEDDED,  

	 
	ATOMISP_INPUT_FORMAT_USER_DEF1,   
	ATOMISP_INPUT_FORMAT_USER_DEF2,   
	ATOMISP_INPUT_FORMAT_USER_DEF3,   
	ATOMISP_INPUT_FORMAT_USER_DEF4,   
	ATOMISP_INPUT_FORMAT_USER_DEF5,   
	ATOMISP_INPUT_FORMAT_USER_DEF6,   
	ATOMISP_INPUT_FORMAT_USER_DEF7,   
	ATOMISP_INPUT_FORMAT_USER_DEF8,   
};

#define N_ATOMISP_INPUT_FORMAT (ATOMISP_INPUT_FORMAT_USER_DEF8 + 1)

enum intel_v4l2_subdev_type {
	RAW_CAMERA = 1,
	CAMERA_MOTOR = 2,
	LED_FLASH = 3,
	TEST_PATTERN = 4,
};

struct intel_v4l2_subdev_id {
	char name[17];
	enum intel_v4l2_subdev_type type;
	enum atomisp_camera_port    port;
};

struct intel_v4l2_subdev_table {
	enum intel_v4l2_subdev_type type;
	enum atomisp_camera_port port;
	unsigned int lanes;
	struct v4l2_subdev *subdev;
};

struct atomisp_platform_data {
	struct intel_v4l2_subdev_table *subdevs;
};

 
struct atomisp_isys_config_info {
	u8 input_format;
	u16 width;
	u16 height;
};

struct atomisp_input_stream_info {
	enum atomisp_input_stream_id stream;
	u8 enable;
	 
	u8 ch_id;
	 
	u8 isys_configs;
	 
	struct atomisp_isys_config_info isys_info[MAX_STREAMS_PER_CHANNEL];
};

struct camera_vcm_control;
struct camera_vcm_ops {
	int (*power_up)(struct v4l2_subdev *sd, struct camera_vcm_control *vcm);
	int (*power_down)(struct v4l2_subdev *sd,
			  struct camera_vcm_control *vcm);
	int (*queryctrl)(struct v4l2_subdev *sd, struct v4l2_queryctrl *qc,
			 struct camera_vcm_control *vcm);
	int (*g_ctrl)(struct v4l2_subdev *sd, struct v4l2_control *ctrl,
		      struct camera_vcm_control *vcm);
	int (*s_ctrl)(struct v4l2_subdev *sd, struct v4l2_control *ctrl,
		      struct camera_vcm_control *vcm);
};

struct camera_vcm_control {
	char camera_module[CAMERA_MODULE_ID_LEN];
	struct camera_vcm_ops *ops;
	struct list_head list;
};

struct camera_sensor_platform_data {
	int (*flisclk_ctrl)(struct v4l2_subdev *subdev, int flag);
	int (*csi_cfg)(struct v4l2_subdev *subdev, int flag);

	 
	int (*gpio0_ctrl)(struct v4l2_subdev *subdev, int on);
	int (*gpio1_ctrl)(struct v4l2_subdev *subdev, int on);
	int (*v1p8_ctrl)(struct v4l2_subdev *subdev, int on);
	int (*v2p8_ctrl)(struct v4l2_subdev *subdev, int on);
	int (*v1p2_ctrl)(struct v4l2_subdev *subdev, int on);
	struct camera_vcm_control *(*get_vcm_ctrl)(struct v4l2_subdev *subdev,
		char *module_id);
};

struct camera_mipi_info {
	enum atomisp_camera_port        port;
	unsigned int                    num_lanes;
	enum atomisp_input_format       input_format;
	enum atomisp_bayer_order        raw_bayer_order;
	enum atomisp_input_format       metadata_format;
	u32                             metadata_width;
	u32                             metadata_height;
	const u32                       *metadata_effective_width;
};

const struct atomisp_platform_data *atomisp_get_platform_data(void);
int atomisp_register_sensor_no_gmin(struct v4l2_subdev *subdev, u32 lanes,
				    enum atomisp_input_format format,
				    enum atomisp_bayer_order bayer_order);
void atomisp_unregister_subdev(struct v4l2_subdev *subdev);

int v4l2_get_acpi_sensor_info(struct device *dev, char **module_id_str);

 
#define __IS_SOC(x) (boot_cpu_data.x86_vendor == X86_VENDOR_INTEL && \
		     boot_cpu_data.x86 == 6 &&                       \
		     boot_cpu_data.x86_model == (x))
#define __IS_SOCS(x,y) (boot_cpu_data.x86_vendor == X86_VENDOR_INTEL && \
		        boot_cpu_data.x86 == 6 &&                       \
		        (boot_cpu_data.x86_model == (x) || \
		         boot_cpu_data.x86_model == (y)))

#define IS_MFLD	__IS_SOC(INTEL_FAM6_ATOM_SALTWELL_MID)
#define IS_BYT	__IS_SOC(INTEL_FAM6_ATOM_SILVERMONT)
#define IS_CHT	__IS_SOC(INTEL_FAM6_ATOM_AIRMONT)
#define IS_MRFD	__IS_SOC(INTEL_FAM6_ATOM_SILVERMONT_MID)
#define IS_MOFD	__IS_SOC(INTEL_FAM6_ATOM_AIRMONT_MID)

 
#define IS_ISP2401 __IS_SOCS(INTEL_FAM6_ATOM_AIRMONT, \
			     INTEL_FAM6_ATOM_AIRMONT_MID)

#endif  
