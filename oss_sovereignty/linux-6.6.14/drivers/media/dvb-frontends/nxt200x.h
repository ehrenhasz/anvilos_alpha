 
 

#ifndef NXT200X_H
#define NXT200X_H

#include <linux/dvb/frontend.h>
#include <linux/firmware.h>

typedef enum nxt_chip_t {
		NXTUNDEFINED,
		NXT2002,
		NXT2004
}nxt_chip_type;

struct nxt200x_config
{
	 
	u8 demod_address;

	 
	int (*set_ts_params)(struct dvb_frontend* fe, int is_punctured);
};

#if IS_REACHABLE(CONFIG_DVB_NXT200X)
extern struct dvb_frontend* nxt200x_attach(const struct nxt200x_config* config,
					   struct i2c_adapter* i2c);
#else
static inline struct dvb_frontend* nxt200x_attach(const struct nxt200x_config* config,
					   struct i2c_adapter* i2c)
{
	printk(KERN_WARNING "%s: driver disabled by Kconfig\n", __func__);
	return NULL;
}
#endif 

#endif  
