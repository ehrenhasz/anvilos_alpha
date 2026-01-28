#ifndef ZL10353_H
#define ZL10353_H
#include <linux/dvb/frontend.h>
struct zl10353_config
{
	u8 demod_address;
	int adc_clock;	 
	int if2;	 
	int no_tuner;
	int parallel_ts;
	u8 disable_i2c_gate_ctrl:1;
	u8 clock_ctl_1;   
	u8 pll_0;         
};
#if IS_REACHABLE(CONFIG_DVB_ZL10353)
extern struct dvb_frontend* zl10353_attach(const struct zl10353_config *config,
					   struct i2c_adapter *i2c);
#else
static inline struct dvb_frontend* zl10353_attach(const struct zl10353_config *config,
					   struct i2c_adapter *i2c)
{
	printk(KERN_WARNING "%s: driver disabled by Kconfig\n", __func__);
	return NULL;
}
#endif  
#endif  
