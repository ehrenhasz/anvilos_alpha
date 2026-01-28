#ifndef CX24123_H
#define CX24123_H
#include <linux/dvb/frontend.h>
struct cx24123_config {
	u8 demod_address;
	int (*set_ts_params)(struct dvb_frontend *fe, int is_punctured);
	int lnb_polarity;
	u8 dont_use_pll;
	void (*agc_callback) (struct dvb_frontend *);
};
#if IS_REACHABLE(CONFIG_DVB_CX24123)
extern struct dvb_frontend *cx24123_attach(const struct cx24123_config *config,
					   struct i2c_adapter *i2c);
extern struct i2c_adapter *cx24123_get_tuner_i2c_adapter(struct dvb_frontend *);
#else
static inline struct dvb_frontend *cx24123_attach(
	const struct cx24123_config *config, struct i2c_adapter *i2c)
{
	printk(KERN_WARNING "%s: driver disabled by Kconfig\n", __func__);
	return NULL;
}
static inline struct i2c_adapter *
	cx24123_get_tuner_i2c_adapter(struct dvb_frontend *fe)
{
	printk(KERN_WARNING "%s: driver disabled by Kconfig\n", __func__);
	return NULL;
}
#endif
#endif  
