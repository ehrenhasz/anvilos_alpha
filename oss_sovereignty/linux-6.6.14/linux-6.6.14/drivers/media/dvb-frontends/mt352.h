#ifndef MT352_H
#define MT352_H
#include <linux/dvb/frontend.h>
struct mt352_config
{
	u8 demod_address;
	int adc_clock;   
	int if2;         
	int no_tuner;
	int (*demod_init)(struct dvb_frontend* fe);
};
#if IS_REACHABLE(CONFIG_DVB_MT352)
extern struct dvb_frontend* mt352_attach(const struct mt352_config* config,
					 struct i2c_adapter* i2c);
#else
static inline struct dvb_frontend* mt352_attach(const struct mt352_config* config,
					 struct i2c_adapter* i2c)
{
	printk(KERN_WARNING "%s: driver disabled by Kconfig\n", __func__);
	return NULL;
}
#endif  
static inline int mt352_write(struct dvb_frontend *fe, const u8 buf[], int len) {
	int r = 0;
	if (fe->ops.write)
		r = fe->ops.write(fe, buf, len);
	return r;
}
#endif  
