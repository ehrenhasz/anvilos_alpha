#ifndef __XC4000_H__
#define __XC4000_H__
#include <linux/firmware.h>
struct dvb_frontend;
struct i2c_adapter;
struct xc4000_config {
	u8	i2c_address;
	u8	default_pm;
	u8	dvb_amplitude;
	u8	set_smoothedcvbs;
	u32	if_khz;
};
#define XC4000_TUNER_RESET		0
#if IS_REACHABLE(CONFIG_MEDIA_TUNER_XC4000)
extern struct dvb_frontend *xc4000_attach(struct dvb_frontend *fe,
					  struct i2c_adapter *i2c,
					  struct xc4000_config *cfg);
#else
static inline struct dvb_frontend *xc4000_attach(struct dvb_frontend *fe,
						 struct i2c_adapter *i2c,
						 struct xc4000_config *cfg)
{
	printk(KERN_WARNING "%s: driver disabled by Kconfig\n", __func__);
	return NULL;
}
#endif
#endif
