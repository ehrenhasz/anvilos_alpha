 
 

#ifndef __ATBM8830_H__
#define __ATBM8830_H__

#include <linux/dvb/frontend.h>
#include <linux/i2c.h>

#define ATBM8830_PROD_8830 0
#define ATBM8830_PROD_8831 1

struct atbm8830_config {

	 
	u8 prod;

	 
	u8 demod_address;

	 
	u8 serial_ts;

	 
	u8 ts_clk_gated;

	 
	u8 ts_sampling_edge;

	 
	u32 osc_clk_freq;  

	 
	u32 if_freq;  

	 
	u8 zif_swap_iq;

	 
	u8 agc_min;
	u8 agc_max;
	u8 agc_hold_loop;
};

#if IS_REACHABLE(CONFIG_DVB_ATBM8830)
extern struct dvb_frontend *atbm8830_attach(const struct atbm8830_config *config,
		struct i2c_adapter *i2c);
#else
static inline
struct dvb_frontend *atbm8830_attach(const struct atbm8830_config *config,
		struct i2c_adapter *i2c) {
	printk(KERN_WARNING "%s: driver disabled by Kconfig\n", __func__);
	return NULL;
}
#endif  

#endif  
