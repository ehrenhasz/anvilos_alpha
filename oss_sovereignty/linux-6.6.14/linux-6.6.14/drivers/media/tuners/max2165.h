#ifndef __MAX2165_H__
#define __MAX2165_H__
struct dvb_frontend;
struct i2c_adapter;
struct max2165_config {
	u8 i2c_address;
	u8 osc_clk;  
};
#if IS_REACHABLE(CONFIG_MEDIA_TUNER_MAX2165)
extern struct dvb_frontend *max2165_attach(struct dvb_frontend *fe,
	struct i2c_adapter *i2c,
	struct max2165_config *cfg);
#else
static inline struct dvb_frontend *max2165_attach(struct dvb_frontend *fe,
	struct i2c_adapter *i2c,
	struct max2165_config *cfg)
{
	printk(KERN_WARNING "%s: driver disabled by Kconfig\n", __func__);
	return NULL;
}
#endif
#endif
