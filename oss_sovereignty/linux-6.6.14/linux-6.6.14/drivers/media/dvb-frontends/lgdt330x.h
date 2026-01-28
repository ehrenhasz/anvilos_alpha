#ifndef LGDT330X_H
#define LGDT330X_H
#include <linux/dvb/frontend.h>
typedef enum lg_chip_t {
		UNDEFINED,
		LGDT3302,
		LGDT3303
}lg_chip_type;
struct lgdt330x_config
{
	lg_chip_type demod_chip;
	int serial_mpeg;
	int (*pll_rf_set) (struct dvb_frontend* fe, int index);
	int (*set_ts_params)(struct dvb_frontend* fe, int is_punctured);
	int clock_polarity_flip;
	struct dvb_frontend* (*get_dvb_frontend)(struct i2c_client *);
};
#if IS_REACHABLE(CONFIG_DVB_LGDT330X)
struct dvb_frontend *lgdt330x_attach(const struct lgdt330x_config *config,
				     u8 demod_address,
				     struct i2c_adapter *i2c);
#else
static inline
struct dvb_frontend *lgdt330x_attach(const struct lgdt330x_config *config,
				     u8 demod_address,
				     struct i2c_adapter *i2c)
{
	printk(KERN_WARNING "%s: driver disabled by Kconfig\n", __func__);
	return NULL;
}
#endif  
#endif  
