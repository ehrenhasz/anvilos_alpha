 
 

#ifndef QT1010_H
#define QT1010_H

#include <media/dvb_frontend.h>

struct qt1010_config {
	u8 i2c_address;
};

 
#if IS_REACHABLE(CONFIG_MEDIA_TUNER_QT1010)
extern struct dvb_frontend *qt1010_attach(struct dvb_frontend *fe,
					  struct i2c_adapter *i2c,
					  struct qt1010_config *cfg);
#else
static inline struct dvb_frontend *qt1010_attach(struct dvb_frontend *fe,
						 struct i2c_adapter *i2c,
						 struct qt1010_config *cfg)
{
	printk(KERN_WARNING "%s: driver disabled by Kconfig\n", __func__);
	return NULL;
}
#endif 

#endif
