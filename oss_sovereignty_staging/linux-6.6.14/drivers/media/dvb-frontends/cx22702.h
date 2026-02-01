 
 

#ifndef CX22702_H
#define CX22702_H

#include <linux/dvb/frontend.h>

struct cx22702_config {
	 
	u8 demod_address;

	 
#define CX22702_PARALLEL_OUTPUT 0
#define CX22702_SERIAL_OUTPUT   1
	u8 output_mode;
};

#if IS_REACHABLE(CONFIG_DVB_CX22702)
extern struct dvb_frontend *cx22702_attach(
	const struct cx22702_config *config,
	struct i2c_adapter *i2c);
#else
static inline struct dvb_frontend *cx22702_attach(
	const struct cx22702_config *config,
	struct i2c_adapter *i2c)
{
	printk(KERN_WARNING "%s: driver disabled by Kconfig\n", __func__);
	return NULL;
}
#endif

#endif
