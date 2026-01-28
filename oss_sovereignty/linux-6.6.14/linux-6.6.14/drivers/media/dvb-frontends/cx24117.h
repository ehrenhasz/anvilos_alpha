#ifndef CX24117_H
#define CX24117_H
#include <linux/dvb/frontend.h>
struct cx24117_config {
	u8 demod_address;
};
#if IS_REACHABLE(CONFIG_DVB_CX24117)
extern struct dvb_frontend *cx24117_attach(
	const struct cx24117_config *config,
	struct i2c_adapter *i2c);
#else
static inline struct dvb_frontend *cx24117_attach(
	const struct cx24117_config *config,
	struct i2c_adapter *i2c)
{
	dev_warn(&i2c->dev, "%s: driver disabled by Kconfig\n", __func__);
	return NULL;
}
#endif
#endif  
