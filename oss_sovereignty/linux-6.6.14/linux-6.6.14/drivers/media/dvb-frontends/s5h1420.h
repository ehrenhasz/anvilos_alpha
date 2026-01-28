#ifndef S5H1420_H
#define S5H1420_H
#include <linux/dvb/frontend.h>
struct s5h1420_config
{
	u8 demod_address;
	u8 invert:1;
	u8 repeated_start_workaround:1;
	u8 cdclk_polarity:1;  
	u8 serial_mpeg:1;
};
#if IS_REACHABLE(CONFIG_DVB_S5H1420)
extern struct dvb_frontend *s5h1420_attach(const struct s5h1420_config *config,
	     struct i2c_adapter *i2c);
extern struct i2c_adapter *s5h1420_get_tuner_i2c_adapter(struct dvb_frontend *fe);
#else
static inline struct dvb_frontend *s5h1420_attach(const struct s5h1420_config *config,
					   struct i2c_adapter *i2c)
{
	printk(KERN_WARNING "%s: driver disabled by Kconfig\n", __func__);
	return NULL;
}
static inline struct i2c_adapter *s5h1420_get_tuner_i2c_adapter(struct dvb_frontend *fe)
{
	return NULL;
}
#endif  
#endif  
