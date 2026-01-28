#ifndef TDA8083_H
#define TDA8083_H
#include <linux/dvb/frontend.h>
struct tda8083_config
{
	u8 demod_address;
};
#if IS_REACHABLE(CONFIG_DVB_TDA8083)
extern struct dvb_frontend* tda8083_attach(const struct tda8083_config* config,
					   struct i2c_adapter* i2c);
#else
static inline struct dvb_frontend* tda8083_attach(const struct tda8083_config* config,
					   struct i2c_adapter* i2c)
{
	printk(KERN_WARNING "%s: driver disabled by Kconfig\n", __func__);
	return NULL;
}
#endif  
#endif  
