 
 

#ifndef ET8EK8REGS_H
#define ET8EK8REGS_H

#include <linux/i2c.h>
#include <linux/types.h>
#include <linux/videodev2.h>
#include <linux/v4l2-subdev.h>

struct v4l2_mbus_framefmt;
struct v4l2_subdev_pad_mbus_code_enum;

struct et8ek8_mode {
	 
	u16 sensor_width;
	u16 sensor_height;
	u16 sensor_window_origin_x;
	u16 sensor_window_origin_y;
	u16 sensor_window_width;
	u16 sensor_window_height;

	 
	u16 width;
	u16 height;
	u16 window_origin_x;
	u16 window_origin_y;
	u16 window_width;
	u16 window_height;

	u32 pixel_clock;		 
	u32 ext_clock;			 
	struct v4l2_fract timeperframe;
	u32 max_exp;			 
	u32 bus_format;			 
	u32 sensitivity;		 
};

#define ET8EK8_REG_8BIT			1
#define ET8EK8_REG_16BIT		2
#define ET8EK8_REG_DELAY		100
#define ET8EK8_REG_TERM			0xff
struct et8ek8_reg {
	u16 type;
	u16 reg;			 
	u32 val;			 
};

 
#define ET8EK8_REGLIST_STANDBY		0
#define ET8EK8_REGLIST_POWERON		1
#define ET8EK8_REGLIST_RESUME		2
#define ET8EK8_REGLIST_STREAMON		3
#define ET8EK8_REGLIST_STREAMOFF	4
#define ET8EK8_REGLIST_DISABLED		5

#define ET8EK8_REGLIST_MODE		10

#define ET8EK8_REGLIST_LSC_ENABLE	100
#define ET8EK8_REGLIST_LSC_DISABLE	101
#define ET8EK8_REGLIST_ANR_ENABLE	102
#define ET8EK8_REGLIST_ANR_DISABLE	103

struct et8ek8_reglist {
	u32 type;
	struct et8ek8_mode mode;
	struct et8ek8_reg regs[];
};

#define ET8EK8_MAX_LEN			32
struct et8ek8_meta_reglist {
	char version[ET8EK8_MAX_LEN];
	union {
		struct et8ek8_reglist *ptr;
	} reglist[];
};

extern struct et8ek8_meta_reglist meta_reglist;

#endif  
