#ifndef NXT6000_H
#define NXT6000_H
#include <linux/dvb/frontend.h>
struct nxt6000_config
{
	u8 demod_address;
	u8 clock_inversion:1;
};
#if IS_REACHABLE(CONFIG_DVB_NXT6000)
extern struct dvb_frontend* nxt6000_attach(const struct nxt6000_config* config,
					   struct i2c_adapter* i2c);
#else
static inline struct dvb_frontend* nxt6000_attach(const struct nxt6000_config* config,
					   struct i2c_adapter* i2c)
{
	printk(KERN_WARNING "%s: driver disabled by Kconfig\n", __func__);
	return NULL;
}
#endif  
#endif  
