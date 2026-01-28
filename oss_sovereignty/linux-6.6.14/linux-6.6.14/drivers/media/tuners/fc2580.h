#ifndef FC2580_H
#define FC2580_H
#include <media/dvb_frontend.h>
#include <media/v4l2-subdev.h>
#include <linux/i2c.h>
struct fc2580_platform_data {
	u32 clk;
	struct dvb_frontend *dvb_frontend;
	struct v4l2_subdev* (*get_v4l2_subdev)(struct i2c_client *);
};
#endif
