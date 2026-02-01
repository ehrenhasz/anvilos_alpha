 
 

#ifndef STV0297_H
#define STV0297_H

#include <linux/dvb/frontend.h>
#include <media/dvb_frontend.h>

struct stv0297_config
{
	 
	u8 demod_address;

	 
	u8* inittab;

	 
	u8 invert:1;

	 
	u8 stop_during_read:1;
};

#if IS_REACHABLE(CONFIG_DVB_STV0297)
extern struct dvb_frontend* stv0297_attach(const struct stv0297_config* config,
					   struct i2c_adapter* i2c);
#else
static inline struct dvb_frontend* stv0297_attach(const struct stv0297_config* config,
					   struct i2c_adapter* i2c)
{
	printk(KERN_WARNING "%s: driver disabled by Kconfig\n", __func__);
	return NULL;
}
#endif 

#endif 
