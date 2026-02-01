 
 

#ifndef VES1X93_H
#define VES1X93_H

#include <linux/dvb/frontend.h>

struct ves1x93_config
{
	 
	u8 demod_address;

	 
	u32 xin;

	 
	u8 invert_pwm:1;
};

#if IS_REACHABLE(CONFIG_DVB_VES1X93)
extern struct dvb_frontend* ves1x93_attach(const struct ves1x93_config* config,
					   struct i2c_adapter* i2c);
#else
static inline struct dvb_frontend* ves1x93_attach(const struct ves1x93_config* config,
					   struct i2c_adapter* i2c)
{
	printk(KERN_WARNING "%s: driver disabled by Kconfig\n", __func__);
	return NULL;
}
#endif 

#endif 
