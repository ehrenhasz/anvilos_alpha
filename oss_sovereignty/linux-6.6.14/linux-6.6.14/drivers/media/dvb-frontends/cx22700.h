#ifndef CX22700_H
#define CX22700_H
#include <linux/dvb/frontend.h>
struct cx22700_config
{
	u8 demod_address;
};
#if IS_REACHABLE(CONFIG_DVB_CX22700)
extern struct dvb_frontend* cx22700_attach(const struct cx22700_config* config,
					   struct i2c_adapter* i2c);
#else
static inline struct dvb_frontend* cx22700_attach(const struct cx22700_config* config,
					   struct i2c_adapter* i2c)
{
	printk(KERN_WARNING "%s: driver disabled by Kconfig\n", __func__);
	return NULL;
}
#endif  
#endif  
