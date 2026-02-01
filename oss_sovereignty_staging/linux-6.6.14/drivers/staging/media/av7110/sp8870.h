 
 

#ifndef SP8870_H
#define SP8870_H

#include <linux/dvb/frontend.h>
#include <linux/firmware.h>

struct sp8870_config
{
	 
	u8 demod_address;

	 
	int (*request_firmware)(struct dvb_frontend* fe, const struct firmware **fw, char* name);
};

#if IS_REACHABLE(CONFIG_DVB_SP8870)
extern struct dvb_frontend* sp8870_attach(const struct sp8870_config* config,
					  struct i2c_adapter* i2c);
#else
static inline struct dvb_frontend* sp8870_attach(const struct sp8870_config* config,
					  struct i2c_adapter* i2c)
{
	printk(KERN_WARNING "%s: driver disabled by Kconfig\n", __func__);
	return NULL;
}
#endif 

#endif 
