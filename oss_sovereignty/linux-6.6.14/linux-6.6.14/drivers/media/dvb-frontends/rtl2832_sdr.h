#ifndef RTL2832_SDR_H
#define RTL2832_SDR_H
#include <linux/i2c.h>
#include <media/v4l2-subdev.h>
#include <media/dvb_frontend.h>
struct rtl2832_sdr_platform_data {
	u32 clk;
#define RTL2832_SDR_TUNER_FC2580    0x21
#define RTL2832_SDR_TUNER_TUA9001   0x24
#define RTL2832_SDR_TUNER_FC0012    0x26
#define RTL2832_SDR_TUNER_E4000     0x27
#define RTL2832_SDR_TUNER_FC0013    0x29
#define RTL2832_SDR_TUNER_R820T     0x2a
#define RTL2832_SDR_TUNER_R828D     0x2b
	u8 tuner;
	struct regmap *regmap;
	struct dvb_frontend *dvb_frontend;
	struct v4l2_subdev *v4l2_subdev;
	struct dvb_usb_device *dvb_usb_device;
};
#endif  
