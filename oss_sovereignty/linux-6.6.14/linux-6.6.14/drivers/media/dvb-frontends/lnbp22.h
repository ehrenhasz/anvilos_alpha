#ifndef _LNBP22_H
#define _LNBP22_H
#define LNBP22_EN	  0x10
#define LNBP22_VSEL	0x02
#define LNBP22_LLC	0x01
#include <linux/dvb/frontend.h>
#if IS_REACHABLE(CONFIG_DVB_LNBP22)
extern struct dvb_frontend *lnbp22_attach(struct dvb_frontend *fe,
						struct i2c_adapter *i2c);
#else
static inline struct dvb_frontend *lnbp22_attach(struct dvb_frontend *fe,
						struct i2c_adapter *i2c)
{
	printk(KERN_WARNING "%s: driver disabled by Kconfig\n", __func__);
	return NULL;
}
#endif  
#endif  
