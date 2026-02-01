 
 

#ifndef LNBH29_H
#define LNBH29_H

#include <linux/i2c.h>
#include <linux/dvb/frontend.h>

 
#define LNBH29_DATA_COMP    BIT(3)

struct lnbh29_config {
	u8 i2c_address;
	u8 data_config;
};

#if IS_REACHABLE(CONFIG_DVB_LNBH29)
struct dvb_frontend *lnbh29_attach(struct dvb_frontend *fe,
				   struct lnbh29_config *cfg,
				   struct i2c_adapter *i2c);
#else
static inline struct dvb_frontend *lnbh29_attach(struct dvb_frontend *fe,
						 struct lnbh29_config *cfg,
						 struct i2c_adapter *i2c)
{
	printk(KERN_WARNING "%s: driver disabled by Kconfig\n", __func__);
	return NULL;
}
#endif

#endif
