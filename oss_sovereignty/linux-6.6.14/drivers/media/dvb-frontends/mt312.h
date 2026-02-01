 
 

#ifndef MT312_H
#define MT312_H

#include <linux/dvb/frontend.h>

struct mt312_config {
	 
	u8 demod_address;

	 
	unsigned int voltage_inverted:1;
};

#if IS_REACHABLE(CONFIG_DVB_MT312)
struct dvb_frontend *mt312_attach(const struct mt312_config *config,
					struct i2c_adapter *i2c);
#else
static inline struct dvb_frontend *mt312_attach(
	const struct mt312_config *config, struct i2c_adapter *i2c)
{
	printk(KERN_WARNING "%s: driver disabled by Kconfig\n", __func__);
	return NULL;
}
#endif  

#endif  
