 
 

#ifndef TS2020_H
#define TS2020_H

#include <linux/dvb/frontend.h>

struct ts2020_config {
	u8 tuner_address;
	u32 frequency_div;

	 
	bool loop_through:1;

	 
#define TS2020_CLK_OUT_DISABLED        0
#define TS2020_CLK_OUT_ENABLED         1
#define TS2020_CLK_OUT_ENABLED_XTALOUT 2
	u8 clk_out:2;

	 
	u8 clk_out_div:5;

	 
	bool dont_poll:1;

	 
	struct dvb_frontend *fe;

	 
	u8 attach_in_use:1;

	 
	int (*get_agc_pwm)(struct dvb_frontend *fe, u8 *_agc_pwm);
};

 
#if IS_REACHABLE(CONFIG_DVB_TS2020)
extern struct dvb_frontend *ts2020_attach(
	struct dvb_frontend *fe,
	const struct ts2020_config *config,
	struct i2c_adapter *i2c);
#else
static inline struct dvb_frontend *ts2020_attach(
	struct dvb_frontend *fe,
	const struct ts2020_config *config,
	struct i2c_adapter *i2c)
{
	printk(KERN_WARNING "%s: driver disabled by Kconfig\n", __func__);
	return NULL;
}
#endif

#endif  
