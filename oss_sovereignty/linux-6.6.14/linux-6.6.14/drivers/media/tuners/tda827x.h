#ifndef __DVB_TDA827X_H__
#define __DVB_TDA827X_H__
#include <linux/i2c.h>
#include <media/dvb_frontend.h>
#include "tda8290.h"
struct tda827x_config
{
	int (*init) (struct dvb_frontend *fe);
	int (*sleep) (struct dvb_frontend *fe);
	enum tda8290_lna config;
	int	     switch_addr;
	void (*agcf)(struct dvb_frontend *fe);
};
#if IS_REACHABLE(CONFIG_MEDIA_TUNER_TDA827X)
extern struct dvb_frontend* tda827x_attach(struct dvb_frontend *fe, int addr,
					   struct i2c_adapter *i2c,
					   struct tda827x_config *cfg);
#else
static inline struct dvb_frontend* tda827x_attach(struct dvb_frontend *fe,
						  int addr,
						  struct i2c_adapter *i2c,
						  struct tda827x_config *cfg)
{
	printk(KERN_WARNING "%s: driver disabled by Kconfig\n", __func__);
	return NULL;
}
#endif  
#endif  
